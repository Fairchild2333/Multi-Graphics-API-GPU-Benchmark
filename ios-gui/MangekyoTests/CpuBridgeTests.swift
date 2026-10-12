import XCTest

private final class OutputLines {
    var lines: [String] = []
}

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
}
