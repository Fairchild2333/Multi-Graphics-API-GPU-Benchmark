// GPUBenchmarkApp.swift — App entry point.

import SwiftUI

@main
struct GPUBenchmarkApp: App {
    @StateObject private var engine = BenchEngine()
    @AppStorage("appTheme") private var appTheme: String = "system"

    var body: some Scene {
        WindowGroup {
            root
        }
        .windowStyle(.automatic)
    }

    private var root: some View {
        ContentView()
            .environmentObject(engine)
            .preferredColorScheme(colorScheme)
            // Wide enough that the option rows still fit beside a 200 pt
            // sidebar at the smallest allowed window size.
            .frame(minWidth: 980, minHeight: 620)
    }

    private var colorScheme: ColorScheme? {
        switch appTheme {
        case "light": return .light
        case "dark":  return .dark
        default:      return nil // follow system
        }
    }
}
