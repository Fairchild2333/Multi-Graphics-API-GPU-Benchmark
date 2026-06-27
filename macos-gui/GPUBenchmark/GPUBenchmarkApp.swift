// GPUBenchmarkApp.swift — App entry point.

import SwiftUI

@main
struct GPUBenchmarkApp: App {
    @StateObject private var engine = BenchEngine()
    @AppStorage("appTheme") private var appTheme: String = "system"

    var body: some Scene {
        WindowGroup {
            ContentView()
                .environmentObject(engine)
                .preferredColorScheme(colorScheme)
                .frame(minWidth: 900, minHeight: 600)
        }
        .windowStyle(.automatic)
        .defaultSize(width: 1100, height: 750)
    }

    private var colorScheme: ColorScheme? {
        switch appTheme {
        case "light": return .light
        case "dark":  return .dark
        default:      return nil // follow system
        }
    }
}
