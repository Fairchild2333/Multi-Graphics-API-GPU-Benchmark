import XCTest
import UIKit
import QuartzCore
import Metal
import OpenGLES

private final class TestEaglView: UIView {
    override class var layerClass: AnyClass { CAEAGLLayer.self }
}

private final class OutputLines {
    var lines: [String] = []
}

@MainActor
final class CpuBridgeTests: XCTestCase {
    private func run(_ arguments: [String]) -> (Int32, String) {
        // No view or CAMetalLayer: CPU requests must stay on the CPU path.
        gpb_set_metal_layer(nil)
        let output = OutputLines()
        let context = Unmanaged.passUnretained(output).toOpaque()
        let strings = arguments.map { strdup($0) }
        defer { strings.forEach { free($0) } }
        let pointers = strings.map { UnsafePointer($0) }
        let result = pointers.withUnsafeBufferPointer { buffer in
            gpb_run(buffer.baseAddress, Int32(buffer.count), { line, context in
                guard let line, let context else { return }
                Unmanaged<OutputLines>.fromOpaque(context).takeUnretainedValue()
                    .lines.append(String(cString: line))
            }, context)
        }
        return (result, output.lines.joined(separator: "\n"))
    }

    func testCpuBenchmarkWithoutMetalSurface() {
        let (code, output) = run(["gpu_benchmark", "--cpu-benchmark", "multi",
                                  "--cpu-time", "0.03", "--cpu-warmup", "0", "--cpu-no-save"])
        XCTAssertEqual(code, 0, output)
        XCTAssertTrue(output.contains("CPU_RESULT"), output)
        XCTAssertFalse(output.contains("CAMetalLayer"), output)
    }

    func testMemorySweepWithoutMetalSurface() {
        let (code, output) = run(["gpu_benchmark", "--cpu-membw", "--cpu-membw-max-mib", "1",
                                  "--cpu-membw-no-storage", "--cpu-membw-no-save"])
        XCTAssertEqual(code, 0, output)
        XCTAssertTrue(output.contains("CPUMEM_DONE"), output)
        XCTAssertFalse(output.contains("CAMetalLayer"), output)
    }

    func testSavedCpuMetadataSurvivesBridgeSerialization() throws {
        let (code, output) = run(["gpu_benchmark", "--cpu-benchmark", "multi",
                                  "--cpu-time", "0.03", "--cpu-warmup", "0"])
        XCTAssertEqual(code, 0, output)
        let pointer = try XCTUnwrap(gpb_load_results())
        defer { gpb_free(pointer) }
        let data = Data(String(cString: pointer).utf8)
        let rows = try XCTUnwrap(JSONSerialization.jsonObject(with: data) as? [[String: Any]])
        let cpu = try XCTUnwrap(rows.last(where: { $0["workload"] as? String == "cpu_multi_core" }))
        XCTAssertEqual(cpu["resultSchemaVersion"] as? Int, 4)
        XCTAssertEqual(cpu["platform"] as? String, "iOS")
        XCTAssertTrue((cpu["workloadVersion"] as? String)?.hasSuffix("_ios_preview") == true)
        XCTAssertFalse((cpu["deviceModel"] as? String ?? "").isEmpty)
        XCTAssertGreaterThan(cpu["systemMemoryMB"] as? Int ?? 0, 0)
    }

    func testHeadlessGpuRunsWithoutMetalSurface() async throws {
        gpb_set_metal_layer(nil)
        if let stale = gpb_poll_output() { gpb_free(stale) }
        XCTAssertTrue(gpb_start_workload("stream_headless?particles=65536", 1))
        defer { gpb_stop_workload() }
        for _ in 0..<200 {
            if !gpb_is_running() { break }
            try await Task.sleep(nanoseconds: 100_000_000)
        }
        XCTAssertFalse(gpb_is_running())
        let pointer = try XCTUnwrap(gpb_last_error())
        defer { gpb_free(pointer) }
        XCTAssertEqual(String(cString: pointer), "")
        let lines = try XCTUnwrap(gpb_poll_output())
        defer { gpb_free(lines) }
        XCTAssertTrue(String(cString: lines).contains("Headless mode"))
    }

    func testMalformedCpuOptionDoesNotRunGpu() {
        let (code, output) = run(["gpu_benchmark", "--cpu-benchmark", "multi", "--cpu-time", "nan"])
        XCTAssertEqual(code, 2)
        XCTAssertTrue(output.contains("CPU_ERROR"), output)
        XCTAssertFalse(output.contains("CAMetalLayer"), output)
    }

    private func embedded(_ request: String, translated: Bool = false, seconds: Double = 3,
                          timeoutSeconds: Int = 90) async throws -> String {
        if let stale = gpb_poll_output() { gpb_free(stale) }
        XCTAssertTrue(gpb_start_workload_with_options(request, seconds, translated), request)
        defer { gpb_stop_workload() }
        for _ in 0..<(timeoutSeconds * 10) {
            if !gpb_is_running() { break }
            try await Task.sleep(nanoseconds: 100_000_000)
        }
        XCTAssertFalse(gpb_is_running(), "Embedded workload timed out: \(request)")
        let error = try XCTUnwrap(gpb_last_error())
        defer { gpb_free(error) }
        let lines = try XCTUnwrap(gpb_poll_output())
        defer { gpb_free(lines) }
        let output = String(cString: lines)
        XCTAssertEqual(String(cString: error), "", output)
        return output
    }

    private func savedRows() throws -> [[String: Any]] {
        let pointer = try XCTUnwrap(gpb_load_results())
        defer { gpb_free(pointer) }
        return try XCTUnwrap(JSONSerialization.jsonObject(with: Data(String(cString: pointer).utf8)) as? [[String: Any]])
    }

    func testVulkanRuntimeParticleCapacity() throws {
        let pointer = try XCTUnwrap(gpb_list_gpus())
        defer { gpb_free(pointer) }
        let text = String(cString: pointer)
        let devices = try XCTUnwrap(JSONSerialization.jsonObject(with: Data(text.utf8)) as? [[String: Any]])
        let device = try XCTUnwrap(devices.first)
        guard device["vulkan"] as? Bool == true else { throw XCTSkip("Vulkan unavailable") }
        let maximum = try XCTUnwrap(device["vulkanMaxParticles"] as? Int)
        XCTAssertGreaterThan(maximum, 0)
        XCTAssertGreaterThanOrEqual(maximum, 65_536, "No standard particle preset is supported")
        let attachment = XCTAttachment(string: text)
        attachment.name = "Runtime Vulkan particle capacity"
        attachment.lifetime = .keepAlways
        add(attachment)
    }

    /// Explicit physical A15 acceptance: the complete grid plus a doubled
    /// serial load count. Keep parallel work identical and retain both logs.
    func testA15FullGpuCacheEvidence() async throws {
        #if targetEnvironment(simulator)
        throw XCTSkip("Cache attribution evidence requires the physical A15")
        #else
        let pointer = try XCTUnwrap(gpb_list_gpus())
        defer { gpb_free(pointer) }
        guard String(cString: pointer).contains("A15") else {
            throw XCTSkip("This acceptance profile is for the physical A15")
        }
        gpb_set_metal_layer(nil)
        let oldIDs = Set(try savedRows().compactMap { $0["id"] as? String })
        for steps in [65_536, 131_072] {
            let thermalStart = ProcessInfo.processInfo.thermalState.rawValue
            let request = "gpu_cache?api=metal&max_mib=128&steps=\(steps)&repeats=5" +
                (steps == 65_536 ? "" : "&parallel_steps=8192")
            let output = try await embedded(request, timeoutSeconds: 300)
            XCTAssertTrue(output.contains("GCACHE_DONE\tpoints=124\tchecksum_valid=1"), output)
            XCTAssertTrue(output.contains("counter_inventory="), output)
            let attachment = XCTAttachment(string: "thermal_start=\(thermalStart) thermal_end=\(ProcessInfo.processInfo.thermalState.rawValue)\n" + output)
            attachment.name = "A15 cache evidence \(steps) loads"
            attachment.lifetime = .keepAlways
            add(attachment)
            if steps == 65_536 { try await Task.sleep(nanoseconds: 20_000_000_000) }
        }
        let added = try savedRows().filter { !oldIDs.contains($0["id"] as? String ?? "") }
        XCTAssertEqual(added.count, 2)
        for row in added {
            let config = row["workloadConfig"] as? String ?? ""
            XCTAssertTrue(config.contains("checksumValid=1;counterSets="), config)
            XCTAssertTrue(config.contains("counterInventory="), config)
            XCTAssertTrue(config.contains("attributionEvidence=timing_and_checksum_only"), config)
            XCTAssertTrue(config.contains("cacheAttribution=unverified"), config)
        }
        #endif
    }

    func testA15FirstPassCacheIsolationControls() async throws {
        #if targetEnvironment(simulator)
        throw XCTSkip("Requires the physical A15")
        #else
        let pointer = try XCTUnwrap(gpb_list_gpus())
        defer { gpb_free(pointer) }
        guard String(cString: pointer).contains("A15") else { throw XCTSkip("Requires A15") }
        gpb_set_metal_layer(nil)
        let oldIDs = Set(try savedRows().compactMap { $0["id"] as? String })
        let thermalStart = ProcessInfo.processInfo.thermalState.rawValue
        let output = try await embedded("gpu_cache?api=metal&max_mib=128&repeats=7&isolation_controls=1&alu_conditioning=1",
                                        timeoutSeconds: 300)
        let attachment = XCTAttachment(string: "thermal_start=\(thermalStart) thermal_end=\(ProcessInfo.processInfo.thermalState.rawValue)\n" + output)
        attachment.name = "A15 first-pass and eviction-dose controls"
        attachment.lifetime = .keepAlways
        add(attachment)
        XCTAssertTrue(output.contains("GCACHE_DONE\tpoints=242\tchecksum_valid=1"), output)
        let samples = output.components(separatedBy: "\n").filter { $0.hasPrefix("GCACHE_CONTROL_SAMPLE\t") }
        XCTAssertEqual(samples.count, 11 * 11 * 7)
        XCTAssertTrue(samples.allSatisfy { $0.contains("checksum_valid=1") })
        let added = try savedRows().filter { !oldIDs.contains($0["id"] as? String ?? "") }
        XCTAssertEqual(added.count, 1)
        let row = try XCTUnwrap(added.first)
        XCTAssertEqual(row["workload"] as? String, "gpu_cache_controls")
        let config = row["workloadConfig"] as? String ?? ""
        XCTAssertTrue(config.contains("pairedImmediateRevisit=1"), config)
        XCTAssertTrue(config.contains("curve_eviction_pass_revisit="), config)
        XCTAssertTrue(config.contains("cacheAttribution=unverified"), config)
        XCTAssertTrue(config.contains("aluConditioningSteps=131072"), config)
        XCTAssertTrue(config.contains("zeroLoadMedianMs="), config)
        #endif
    }

    func testVulkanRequiresExplicitTranslationOptIn() {
        let pointer = gpb_list_gpus()!
        defer { gpb_free(pointer) }
        XCTAssertTrue(String(cString: pointer).contains("\"vulkan\":true"), "Bundled runtime must be visible to the GUI")
        gpb_set_metal_layer(nil)
        XCTAssertFalse(gpb_start_workload("stream_headless?particles=65536&api=vulkan", 3))
        let error = gpb_last_error()!
        defer { gpb_free(error) }
        XCTAssertTrue(String(cString: error).contains("enable translated APIs"))
        XCTAssertFalse(gpb_is_running())
    }

    func testVulkanHeadlessUsesTranslatedContract() async throws {
        gpb_set_metal_layer(nil)
        let output = try await embedded("stream_headless?particles=65536&api=vulkan", translated: true)
        XCTAssertTrue(output.contains("MoltenVK"), output)
        let row = try XCTUnwrap(try savedRows().last(where: {
            $0["graphicsApi"] as? String == "Vulkan" && $0["headless"] as? Bool == true && $0["workload"] as? String == "stream"
        }))
        let version = row["workloadVersion"] as? String ?? ""
        XCTAssertTrue(version.contains("ios_preview") && version.contains("moltenvk"))
        XCTAssertTrue((row["workloadConfig"] as? String ?? "").contains("apiImplementation=MoltenVK;translatedTo=Metal"))
        XCTAssertGreaterThan(row["score"] as? Double ?? 0, 0)
    }

    func testVulkanMemorySavesAllCurveFamilies() async throws {
        gpb_set_metal_layer(nil)
        let output = try await embedded("memory_sweep?api=vulkan&max_mib=1", translated: true)
        XCTAssertTrue(output.contains("MEMBW_POINT"), output)
        let row = try XCTUnwrap(try savedRows().last(where: {
            $0["graphicsApi"] as? String == "Vulkan" && $0["workload"] as? String == "membw"
        }))
        XCTAssertEqual(row["workloadVersion"] as? String, "membw_v3_streaming_ios_preview_moltenvk")
        let config = row["workloadConfig"] as? String ?? ""
        for family in ["read", "write", "copy", "l1", "l1_single"] {
            XCTAssertTrue(config.contains("curve_\(family)="), config)
        }
        XCTAssertTrue(config.contains("memoryPlateauReached=0"), config)
        XCTAssertTrue(config.contains("apiImplementation=MoltenVK;translatedTo=Metal"), config)
    }

    func testCacheDiagnosticHasChecksumsAndSeparateUnits() async throws {
        try XCTSkipIf(MTLCreateSystemDefaultDevice()?.hasUnifiedMemory != true,
            "Virtual GPU lacks unified-memory cache controls; verify on a physical iPhone.")
        gpb_set_metal_layer(nil)
        let output = try await embedded("gpu_cache?api=metal&max_mib=1&steps=4096&repeats=3")
        XCTAssertTrue(output.contains("GCACHE_DONE"), output)
        XCTAssertTrue(output.contains("unit=ns/load") && output.contains("unit=GB/s_payload"), output)
        let row = try XCTUnwrap(try savedRows().last(where: { $0["workload"] as? String == "gpu_cache" }))
        let pointer = try XCTUnwrap(gpb_list_gpus())
        defer { gpb_free(pointer) }
        let devices = try XCTUnwrap(JSONSerialization.jsonObject(with: Data(String(cString: pointer).utf8)) as? [[String: Any]])
        let parallelSteps = devices.first?["gpuCacheParallelSteps"] as? Int ?? 0
        let version = parallelSteps > 0 ? "metal_dependent_cache_v3_parallel_\(parallelSteps)_preview_ios_preview"
            : "metal_dependent_cache_v2_preview_ios_preview"
        XCTAssertEqual(row["workloadVersion"] as? String, version)
        let config = row["workloadConfig"] as? String ?? ""
        for condition in ["gpu_warm", "eviction_pass", "cpu_warm", "parallel_gpu_warm"] {
            XCTAssertTrue(config.contains("curve_\(condition)="), config)
        }
        XCTAssertTrue(config.contains("checksumValid=1") && config.contains("cacheAttribution=unverified"), config)
        XCTAssertEqual(row["scoreUnit"] as? String, "ns/load")
    }

    @MainActor
    func testVulkanRenderPathsUseEmbeddedMetalSurface() async throws {
        let window = try XCTUnwrap(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }
            .flatMap(\.windows).first { $0.isKeyWindow })
        let view = UIView(frame: window.bounds)
        let layer = CAMetalLayer()
        layer.frame = view.bounds
        layer.drawableSize = CGSize(width: 1280, height: 720)
        view.layer.addSublayer(layer)
        window.addSubview(view)
        defer { gpb_set_metal_layer(nil); view.removeFromSuperview() }
        try await Task.sleep(nanoseconds: 100_000_000)
        let pointer = try XCTUnwrap(gpb_list_gpus())
        defer { gpb_free(pointer) }
        let devices = try XCTUnwrap(JSONSerialization.jsonObject(with: Data(String(cString: pointer).utf8)) as? [[String: Any]])
        var requests = ["stream?particles=65536&api=vulkan&vsync=0", "gpu_burn?steps=16&api=vulkan&vsync=0"]
        if devices.first?["vulkanLiquid"] as? Bool == true { requests.append("cinematic_liquid?api=vulkan&vsync=0") }
        for request in requests {
            gpb_set_metal_layer(Unmanaged.passUnretained(layer).toOpaque())
            let output = try await embedded(request, translated: true)
            XCTAssertTrue(output.contains("MoltenVK"), output)
        }
        let rows = try savedRows().filter { $0["graphicsApi"] as? String == "Vulkan" && $0["headless"] as? Bool == false }
        XCTAssertGreaterThanOrEqual(rows.count, requests.count)
        for row in rows.suffix(requests.count) {
            XCTAssertEqual(row["resWidth"] as? Int, 1280)
            XCTAssertEqual(row["resHeight"] as? Int, 720)
            XCTAssertGreaterThan(row["score"] as? Double ?? 0, 0)
        }
    }

    func testGles2LegacyBurnPreservesShadersAndContract() async throws {
        let window = try XCTUnwrap(UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }
            .flatMap(\.windows).first { $0.isKeyWindow })
        let view = TestEaglView(frame: CGRect(x: 0, y: 0, width: 1280, height: 720))
        view.contentScaleFactor = 1
        let layer = view.layer as! CAEAGLLayer
        layer.contentsScale = 1
        layer.isOpaque = true
        layer.drawableProperties = [kEAGLDrawablePropertyRetainedBacking: false,
                                    kEAGLDrawablePropertyColorFormat: kEAGLColorFormatRGBA8]
        window.addSubview(view)
        defer { gpb_set_gles_layer(nil); view.removeFromSuperview() }
        try await Task.sleep(nanoseconds: 100_000_000)
        gpb_set_metal_layer(nil)
        gpb_set_gles_layer(Unmanaged.passUnretained(layer).toOpaque())
        #if targetEnvironment(simulator)
        let tiers = [16, 64] // The shared software-renderer safety cap is 64.
        #else
        let tiers = [16, 64, 256]
        #endif
        for steps in tiers {
            let output = try await embedded("gpu_burn?steps=\(steps)&api=gles2", seconds: 7)
            XCTAssertTrue(output.contains("iOS EAGL"), output)
            let row = try XCTUnwrap(try savedRows().last(where: { $0["graphicsApi"] as? String == "OpenGL ES 2.0" }))
            XCTAssertEqual(row["workloadVersion"] as? String, "gpu_burn_es2_legacy_v1_fixed_steps_\(steps)_ios_preview_sync_timing")
            XCTAssertGreaterThan(row["score"] as? Double ?? 0, 0)
            XCTAssertEqual(row["resWidth"] as? Int, 1280)
            XCTAssertEqual(row["resHeight"] as? Int, 720)
            XCTAssertTrue((row["workloadConfig"] as? String ?? "").contains("timingMode=synchronized_wall_clock"))
        }
    }
}
