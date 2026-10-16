import XCTest

@MainActor
final class CompleteSuiteUITests: XCTestCase {
    func testCompleteSuiteSurvivesLandscapeTransition() throws {
        #if targetEnvironment(simulator)
        continueAfterFailure = false
        let app = XCUIApplication()
        app.launchArguments = ["--ios-allow-translated-api"]
        app.launch()
        defer { app.terminate() }
        let button = app.buttons["complete-suite-all"]
        XCTAssertTrue(button.waitForExistence(timeout: 30))
        let ready = XCTNSPredicateExpectation(predicate: NSPredicate(format: "enabled == true"), object: button)
        XCTAssertEqual(XCTWaiter.wait(for: [ready], timeout: 30), .completed)
        button.tap()

        // A Max-sized phone changes horizontal size class during this turn.
        // The first rendered job lasts 15 seconds, so it must still own the
        // landscape surface rather than dismissing and cancelling the suite.
        Thread.sleep(forTimeInterval: 5)
        let attachment = XCTAttachment(screenshot: app.screenshot())
        attachment.name = "Complete suite survives phone landscape transition"
        attachment.lifetime = .keepAlways
        add(attachment)
        XCTAssertEqual(app.state, .runningForeground)
        XCTAssertGreaterThan(app.frame.width, app.frame.height)
        if button.exists { XCTAssertFalse(button.isHittable) }

        app.coordinate(withNormalizedOffset: CGVector(dx: 0.5, dy: 0.5)).tap()
        let status = app.staticTexts["complete-suite-status"]
        XCTAssertTrue(status.waitForExistence(timeout: 20))
        let stopped = XCTNSPredicateExpectation(predicate: NSPredicate(format: "value == 'cancelled'"), object: status)
        XCTAssertEqual(XCTWaiter.wait(for: [stopped], timeout: 20), .completed)
        #else
        throw XCTSkip("The short navigation regression runs in the simulator; physical acceptance runs the complete suite.")
        #endif
    }

    func testOneButtonRunsCompleteSuite() throws {
        continueAfterFailure = false
        let app = XCUIApplication()
        // Enable the already-supported translated API gate. Do not use the
        // automatic suite launch or a targeted-job argument: tap the real UI.
        app.launchArguments = ["--ios-allow-translated-api"]
        app.launch()
        let button = app.buttons["complete-suite-all"]
        XCTAssertTrue(button.waitForExistence(timeout: 30))
        let ready = NSPredicate(format: "enabled == true")
        let readyExpectation = XCTNSPredicateExpectation(predicate: ready, object: button)
        XCTAssertEqual(XCTWaiter.wait(for: [readyExpectation], timeout: 30), .completed)
        print("ONE_BUTTON_READY label=\(button.label)")
        let before = XCTAttachment(screenshot: app.screenshot())
        before.name = "Before tapping the complete-suite button"
        before.lifetime = .keepAlways
        add(before)
        button.tap()
        print("ONE_BUTTON_TAPPED")

        // Leave the benchmark foreground and avoid repeatedly querying
        // accessibility during measured CPU windows. The host also archives
        // the app's native manifest and each saved result independently.
        let deadline = Date().addingTimeInterval(2400)
        let status = app.staticTexts["complete-suite-status"]
        while Date() < deadline {
            Thread.sleep(forTimeInterval: 20)
            if status.exists, let value = status.value as? String,
               ["passed", "review", "failed", "cancelled"].contains(value) {
                #if targetEnvironment(simulator)
                // Simulator acceptance checks execution and saved results.
                // Variability reviews remain visible in the native manifest;
                // simulated performance is not a physical-device score.
                XCTAssertTrue(["passed", "review"].contains(value), "Complete suite failed or was cancelled")
                #else
                XCTAssertEqual(value, "passed", "Complete suite has an unresolved job")
                #endif
                let attachment = XCTAttachment(screenshot: app.screenshot())
                attachment.name = "One-button complete suite"
                attachment.lifetime = .keepAlways
                add(attachment)
                return
            }
        }
        XCTFail("Complete suite did not finish within the device acceptance window")
    }
}
