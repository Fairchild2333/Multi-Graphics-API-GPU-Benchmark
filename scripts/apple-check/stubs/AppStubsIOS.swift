// Stand-ins for the UIKit parts of the iOS GPU page: the view the engine
// draws into, and the switch to landscape while a test runs.
import QuartzCore
import SwiftUI
import Foundation

#if os(Linux)
extension ProcessInfo {
    enum ThermalState: Int { case nominal, fair, serious, critical }
    var thermalState: ThermalState { .nominal }
}
#endif

struct MetalBenchView: View {
    var showGles = false
    var onLayerReady: (CAMetalLayer, CAEAGLLayer) -> Void

    var body: some View { EmptyView() }
}

@MainActor
enum ScreenOrientation {
    static func setLandscape(_ landscape: Bool) {}
}
