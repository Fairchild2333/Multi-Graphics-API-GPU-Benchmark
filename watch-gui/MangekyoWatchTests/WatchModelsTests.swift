import XCTest
@testable import MangekyoWatch

final class WatchModelsTests: XCTestCase {
    func testCurvesAndUnverifiedMemoryStayVisible() throws {
        let row = WatchResult(id: "one", timestamp: "test", workload: "cpu_membw",
            workloadVersion: "cpu_membw_v1_watchos_preview_simulator",
            workloadConfig: "curve_single_read=4:12.1,8:11.9;curve_all_copy=4:20;single_memory_settled=0;pageFaultMeasured=1;storageMeasured=1;repeated=first;repeated=last",
            score: 0, scoreUnit: "GB/s", platform: "watchOS", deviceModel: "WatchSimulator", cpuName: "host", isSoftware: true)
        XCTAssertEqual(row.curves.count, 3)
        XCTAssertEqual(row.config["repeated"], "last")
        XCTAssertEqual(row.reviewReasons.count, 1)
        XCTAssertFalse(row.memoryVerified)
        let encoded = try JSONEncoder().encode(row)
        XCTAssertEqual(try JSONDecoder().decode(WatchResult.self, from: encoded).curves.count, 3)
    }
    func testMissingPageAndStorageDoNotBecomePassed() {
        let row = WatchResult(id: "two", timestamp: "test", workload: "cpu_membw", workloadVersion: "watchos_preview",
            workloadConfig: "all_memory_settled=1;pageFaultMeasured=0;storageMeasured=0", score: 10,
            scoreUnit: "GB/s", platform: "watchOS", deviceModel: "Watch", cpuName: "Watch CPU", isSoftware: false)
        XCTAssertEqual(row.reviewReasons.count, 2)
        XCTAssertTrue(row.memoryVerified)
    }
    func testSinglePlateauCannotValidateAnUnsettledAllCoreHeadline() {
        let row = WatchResult(id: "three", timestamp: "test", workload: "cpu_membw", workloadVersion: "watchos_preview",
            workloadConfig: "score=all_core_memory_read;single_memory_settled=1;all_memory_settled=0;pageFaultMeasured=1;storageMeasured=1",
            score: 100, scoreUnit: "GB/s", platform: "watchOS", deviceModel: "Watch", cpuName: "Watch CPU", isSoftware: false)
        XCTAssertFalse(row.memoryVerified)
        XCTAssertEqual(row.reviewReasons.count, 1)
    }
}
