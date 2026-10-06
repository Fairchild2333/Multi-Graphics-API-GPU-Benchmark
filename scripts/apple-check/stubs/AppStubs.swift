// Stand-ins for the parts of the app that are not type-checked here.
import SwiftUI

struct GlassCard<Content: View>: View {
    var padding: CGFloat = 20
    var cornerRadius: CGFloat = 12
    @ViewBuilder var content: () -> Content

    var body: some View {
        content().padding(padding)
    }
}

struct AccentGlassCard<Content: View>: View {
    var cornerRadius: CGFloat = 12
    @ViewBuilder var content: () -> Content

    var body: some View {
        content().padding(16)
    }
}
