package com.mangekyo.benchmark.core

import org.junit.Assert.*
import org.junit.Test

class SuitePlanTest {
    private val full = SuiteCapabilities(true, true, 16777216, true, 16777216, true)
    @Test fun completeMatrixAndScopes() {
        assertEquals(29, supportedSuiteJobs(full, SuiteScope.ALL).size)
        val gpu = supportedSuiteJobs(full, SuiteScope.GPU)
        assertEquals(27, gpu.size)
        assertFalse(gpu.any { it.cpu })
        assertEquals(2, supportedSuiteJobs(SuiteCapabilities(), SuiteScope.CPU).size)
        assertTrue(supportedSuiteJobs(SuiteCapabilities(), SuiteScope.GPU).isEmpty())
        assertEquals(29, supportedSuiteJobs(full, SuiteScope.ALL).map { it.id }.toSet().size)
    }
    @Test fun bufferAndFeatureLimitsOmitUnsupportedJobs() {
        val restricted = full.copy(liquid = false, glesMaxParticles = 4194304)
        val jobs = supportedSuiteJobs(restricted, SuiteScope.ALL)
        assertEquals(26, jobs.size)
        assertFalse(jobs.any { it.api == "gles" && it.particles == 16777216 })
        assertFalse(jobs.any { it.workload == "cinematic_liquid" })
        val legacy = supportedSuiteJobs(SuiteCapabilities(gles2 = true), SuiteScope.ALL)
        assertEquals(5, legacy.size)
        assertEquals(3, legacy.count { it.api == "gles2" && it.workload == "gpu_burn" })
    }
    private fun record(api: String = "Vulkan", workload: String = "stream", particles: Int = 4194304,
        headless: Boolean = true, compute: Double = 10.0, render: Double = 0.0,
        total: Double = 10.0, frame: Double = 10.0, stable: Double = 20.0, drop: Double = 0.0) = HistoryEntry(
        id = "new", workload = workload, workloadVersion = "stream_v1_android_preview", graphicsApi = api,
        deviceName = "device", score = 20.0, scoreUnit = "GB/s", timestamp = "now", abi = "arm64",
        particleCount = particles, headless = headless, durationSec = 15.0,
        avgComputeMs = compute, avgRenderMs = render, avgTotalGpuMs = total, avgFrameTimeMs = frame,
        stableScore = stable, throughputDropPct = drop)
    @Test fun matchesJobAndRejectsWrongApiSizeOrShortRun() {
        val job = SuiteJob("vk-4m", "vulkan", "stream", 4194304, headless = true)
        assertTrue(record().matches(job))
        assertFalse(record(api = "OpenGL ES").matches(job))
        assertFalse(record(particles = 1048576).matches(job))
        assertFalse(record(headless = false).matches(job))
        assertFalse(record().copy(durationSec = 3.0).matches(job))
        assertFalse(record().copy(score = 0.0).matches(job))
    }
    @Test fun flagsTimingAndDriftButAllowsChangingLiquid() {
        assertTrue(record().measurementReviewReasons.isEmpty())
        assertFalse(record(compute = 0.002).measurementReviewReasons.isEmpty())
        assertFalse(record(total = 20.0).measurementReviewReasons.isEmpty())
        assertFalse(record(compute = 8.0, render = 8.0).measurementReviewReasons.isEmpty())
        assertFalse(record(drop = 8.0).measurementReviewReasons.isEmpty())
        assertFalse(record(stable = 0.0).measurementReviewReasons.isEmpty())
        assertTrue(record(workload = "cinematic_liquid", headless = false, stable = 0.0, drop = 8.0).measurementReviewReasons.isEmpty())
        assertTrue(record(workload = "cpu_membw", total = 20.0, stable = 0.0).measurementReviewReasons.isEmpty())
    }
    @Test fun memorySuiteRequiresVerifiedDataAndOneTimingContract() {
        val job = SuiteJob("vulkan-memory", "vulkan", "membw", headless = true)
        val verified = record(workload = "membw").copy(workloadVersion = "membw_v2_verified_android_preview",
            workloadConfig = "dataVerified=1;writeCopyChecks=2;timestampAnomalies=0")
        assertTrue(verified.matches(job))
        assertFalse(verified.copy(workloadVersion = "membw_v1_android_preview").matches(job))
        assertFalse(verified.copy(workloadConfig = "dataVerified=0;writeCopyChecks=2;timestampAnomalies=0").matches(job))
        assertFalse(verified.copy(workloadConfig = "dataVerified=1;writeCopyChecks=0;timestampAnomalies=0").matches(job))
        assertFalse(verified.copy(workloadConfig = "dataVerified=1;writeCopyChecks=2;timestampAnomalies=1").matches(job))
    }
}
