// MangekyoApp.swift — iOS App entry point.
// Deployment floor: iOS 16.0. Liquid Glass on iOS 26+.

import SwiftUI

@main
struct MangekyoApp: App {
    @StateObject private var engine = BenchEngine()
    @AppStorage("appTheme") private var appTheme: String = "system"
    @Environment(\.scenePhase) private var scenePhase

    var body: some Scene {
        WindowGroup {
            ContentView()
                .environmentObject(engine)
                .preferredColorScheme(colorScheme)
        }
        .onChange(of: scenePhase) { newPhase in
            switch newPhase {
            case .background:
                // iOS: cancel running benchmark when entering background
                // to avoid computing dirty scores or wasting battery.
                engine.cancelIfRunning()
            case .active:
                // Returning from background: refresh state but don't
                // auto-restart cancelled runs.
                break
            default:
                break
            }
        }
    }

    private var colorScheme: ColorScheme? {
        switch appTheme {
        case "light": return .light
        case "dark":  return .dark
        default:      return nil // follow system
        }
    }
}
