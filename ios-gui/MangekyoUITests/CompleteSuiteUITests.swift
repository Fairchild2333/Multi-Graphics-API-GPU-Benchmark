import XCTest

@MainActor
final class CompleteSuiteUITests: XCTestCase {
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
        button.tap()

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
