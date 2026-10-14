// ScreenOrientation.swift — turns the app to landscape while a GPU test runs.

import SwiftUI
import UIKit

/// Tells the system which orientations the app allows right now. A GPU test
/// runs in landscape, as in the Android and HarmonyOS apps; everywhere else
/// the app follows the way the device is held.
@MainActor
final class AppDelegate: NSObject, UIApplicationDelegate {
    static var orientations: UIInterfaceOrientationMask = .allButUpsideDown

    func application(_ application: UIApplication,
                     supportedInterfaceOrientationsFor window: UIWindow?) -> UIInterfaceOrientationMask {
        AppDelegate.orientations
    }
}

@MainActor
enum ScreenOrientation {
    /// Landscape for a running test, or back to the way the device is held.
    /// An iPad that shows the app beside another one ignores the request.
    static func setLandscape(_ landscape: Bool) {
        setAllowed(landscape ? .landscape : .allButUpsideDown)
    }

    static func setSuitePresentation(rendering: Bool) {
        // Restore portrait on a phone when a GPU picture ends, even if its
        // orientation lock kept the preceding landscape geometry in place.
        setAllowed(rendering ? .landscape : UIDevice.current.userInterfaceIdiom == .phone
            ? .portrait : .allButUpsideDown)
    }

    private static func setAllowed(_ orientations: UIInterfaceOrientationMask) {
        AppDelegate.orientations = orientations
        for scene in UIApplication.shared.connectedScenes {
            guard let windowScene = scene as? UIWindowScene else { continue }
            windowScene.requestGeometryUpdate(.iOS(interfaceOrientations: AppDelegate.orientations))
            for window in windowScene.windows {
                window.rootViewController?.setNeedsUpdateOfSupportedInterfaceOrientations()
            }
        }
    }
}
