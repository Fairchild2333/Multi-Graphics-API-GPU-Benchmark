// Stand-ins for the UIKit parts of the iOS GPU page: the view the engine
// draws into, and the switch to landscape while a test runs.
import QuartzCore
import SwiftUI

struct MetalBenchView: View {
    var onLayerReady: (CAMetalLayer) -> Void

    var body: some View { EmptyView() }
}

@MainActor
enum ScreenOrientation {
    static func setLandscape(_ landscape: Bool) {}
}
