// Stand-in for the Metal preview surface of the iOS run page.
import QuartzCore
import SwiftUI

struct MetalBenchView: View {
    var onLayerReady: (CAMetalLayer) -> Void

    var body: some View { EmptyView() }
}
