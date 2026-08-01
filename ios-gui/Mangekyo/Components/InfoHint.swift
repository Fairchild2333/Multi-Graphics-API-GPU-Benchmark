// InfoHint.swift — Option explanations for iOS.
//
// The macOS app puts these on a hover tooltip. iOS has no hover, so the same
// text sits behind a tappable "i" that presents a popover.

import SwiftUI

struct InfoHint: View {
    let text: String
    @State private var showing = false

    var body: some View {
        Button {
            showing = true
        } label: {
            Image(systemName: "info.circle")
                .foregroundStyle(.secondary)
        }
        .buttonStyle(.plain)
        .accessibilityLabel(Text(text))
        .popover(isPresented: $showing) {
            Text(text)
                .font(.callout)
                .padding(16)
                .frame(maxWidth: 320)
                .modifier(CompactPopover())
        }
    }
}

/// A switch with its explanation behind an adjacent "i".
struct HintedToggle: View {
    let title: String
    let hint: String
    @Binding var isOn: Bool

    var body: some View {
        Toggle(isOn: $isOn) {
            HStack(spacing: 6) {
                Text(title)
                InfoHint(text: hint)
            }
        }
    }
}

/// Keep the popover a popover on iPhone. The deployment floor is iOS 16.0 and
/// the adaptation modifier arrived in 16.4, so older systems get the default
/// sheet presentation instead.
private struct CompactPopover: ViewModifier {
    func body(content: Content) -> some View {
        if #available(iOS 16.4, *) {
            content.presentationCompactAdaptation(.popover)
        } else {
            content
        }
    }
}
