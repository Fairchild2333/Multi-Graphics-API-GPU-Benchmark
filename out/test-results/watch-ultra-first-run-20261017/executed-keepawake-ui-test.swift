import XCTest

final class WatchFlowTests: XCTestCase {
    @MainActor
    func testStandardProfileCompletes() {
        let app = XCUIApplication()
        app.launchArguments = ["-AppleLanguages", "(zh-Hans)", "-AppleLocale", "zh_CN"]
        app.launch()
        XCTAssertTrue(app.buttons["watch-run-all"].waitForExistence(timeout: 15))
        app.buttons["watch-run-all"].tap()
        let status = app.staticTexts["watch-suite-status"]
        // A physical Watch can background the app seconds after the screen sleeps.
        // Touch only the noninteractive stage label to keep this UI run awake.
        let deadline = Date().addingTimeInterval(420)
        let stage = app.staticTexts.matching(NSPredicate(format:
            "label IN %@", ["算力：单线程＋全核心", "缓存／内存／页面／存储",
                           "Compute: single + all cores", "Cache / memory / pages / storage"])).firstMatch
        while !status.exists && Date() < deadline {
            if stage.exists && stage.isHittable { stage.tap() }
            Thread.sleep(forTimeInterval: 1.5)
        }
        XCTAssertTrue(status.exists)
        XCTAssertTrue(status.label.contains("2/2"), status.label)
        XCTAssertTrue(["passed", "review"].contains(status.value as? String ?? ""))
        let capture = XCTAttachment(screenshot: app.screenshot())
        capture.name = "watch-standard-completed"; capture.lifetime = .keepAlways; add(capture)
    }
    @MainActor
    func testOneButtonCompletesAndHistorySurvivesRelaunch() {
        let app = XCUIApplication()
        app.launchArguments = ["--watch-smoke", "-AppleLanguages", "(zh-Hans)", "-AppleLocale", "zh_CN"]
        app.launch()
        let start = app.buttons["watch-run-all"]
        XCTAssertTrue(start.waitForExistence(timeout: 20))
        XCTAssertTrue(start.label.contains("CPU"))
        start.tap()
        let status = app.staticTexts["watch-suite-status"]
        XCTAssertTrue(status.waitForExistence(timeout: 150))
        XCTAssertTrue(status.label.contains("2/2"), status.label)
        let completed = XCTAttachment(screenshot: app.screenshot())
        completed.name = "watch-completed"; completed.lifetime = .keepAlways; add(completed)
        let history = app.buttons["watch-history"]
        if !history.isHittable { app.swipeUp() }
        XCTAssertTrue(history.waitForExistence(timeout: 10)); history.tap()
        let memory = app.buttons["watch-result-cpu_membw"]
        XCTAssertTrue(memory.waitForExistence(timeout: 10))
        memory.tap()
        let curves = app.buttons["watch-open-curves"]
        XCTAssertTrue(curves.waitForExistence(timeout: 10)); curves.tap()
        XCTAssertTrue(app.staticTexts["watch-curve-count"].waitForExistence(timeout: 10))
        let capture = XCTAttachment(screenshot: app.screenshot())
        capture.name = "watch-memory-curves"; capture.lifetime = .keepAlways; add(capture)
        app.terminate(); app.launch()
        XCTAssertTrue(app.staticTexts["watch-suite-status"].waitForExistence(timeout: 15))
        XCTAssertTrue(app.staticTexts["watch-suite-status"].label.contains("2/2"))
    }
    @MainActor
    func testCancelDoesNotClaimCompletion() {
        let app = XCUIApplication()
        app.launch()
        XCTAssertTrue(app.buttons["watch-run-all"].waitForExistence(timeout: 15))
        app.buttons["watch-run-all"].tap()
        let stop = app.buttons["watch-stop"]
        XCTAssertTrue(stop.waitForExistence(timeout: 10)); stop.tap()
        let status = app.staticTexts["watch-suite-status"]
        XCTAssertTrue(status.waitForExistence(timeout: 20))
        XCTAssertEqual(status.value as? String, "cancelled")
        XCTAssertFalse(status.label.contains("2/2"))
    }
}
