// GlassCard.swift — Reusable Liquid Glass card (shared Apple UI contract).
// Liquid Glass on macOS 26+ / iOS 26+ / iOS 27+; older OS uses Material fallback.
// iOS app target does not exist yet; keep #available ready for the shared port.

import SwiftUI

struct GlassCard<Content: View>: View {
    var padding: CGFloat = 20
    var cornerRadius: CGFloat = 12
    @ViewBuilder var content: () -> Content

    var body: some View {
        content()
            .padding(padding)
            // Content that paints its own opaque background — `Table` most
            // visibly — otherwise squares off the card and cuts straight
            // through the glass edge and its shadow.
            .clipShape(RoundedRectangle(cornerRadius: cornerRadius))
            // Deliberately not `glassEffect`. Liquid Glass draws the card and
            // its shadow as a compositor-level backdrop effect rather than as
            // part of the view's own drawing, so the shadow is bounded and
            // updated by that layer: it looks sheared off at the edges, lags a
            // beat behind on page changes, and smears while scrolling. A
            // material fill plus an ordinary `.shadow` is drawn inline with the
            // content, so it tracks the card exactly.
            .background(
                RoundedRectangle(cornerRadius: cornerRadius)
                    .fill(.regularMaterial)
                    .shadow(color: .black.opacity(0.12), radius: 7, x: 0, y: 3)
            )
            .overlay(
                RoundedRectangle(cornerRadius: cornerRadius)
                    .strokeBorder(Color.primary.opacity(0.08), lineWidth: 1)
            )
    }
}

/// Accent-colored glass card for highlighted content (scores, etc.)
struct AccentGlassCard<Content: View>: View {
    var cornerRadius: CGFloat = 12
    @ViewBuilder var content: () -> Content

    var body: some View {
        content()
            .padding(16)
            .clipShape(RoundedRectangle(cornerRadius: cornerRadius))
            // Same reasoning as GlassCard: shadow drawn inline, not by a
            // compositor backdrop effect.
            .background(
                RoundedRectangle(cornerRadius: cornerRadius)
                    .fill(.regularMaterial)
                    .overlay(
                        RoundedRectangle(cornerRadius: cornerRadius)
                            .fill(Color.accentColor.opacity(0.14))
                    )
                    .shadow(color: .black.opacity(0.12), radius: 7, x: 0, y: 3)
            )
            .overlay(
                RoundedRectangle(cornerRadius: cornerRadius)
                    .strokeBorder(Color.accentColor.opacity(0.28), lineWidth: 1)
            )
    }
}
