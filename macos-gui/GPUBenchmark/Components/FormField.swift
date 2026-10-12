// FormField.swift — Shared option-row layout for the benchmark pages.
//
// SwiftUI wraps AppKit pop-up buttons, and those keep their intrinsic width:
// a plain `.frame(width:)` leaves the control centred inside the reserved
// space, so labels and controls in the same row start at different offsets.
// Every field here pins both the label and the control to the field's leading
// edge and lets a trailing spacer absorb the slack instead of the controls.

import SwiftUI

enum FormMetrics {
    /// Gap between a field label and its control.
    static let labelSpacing: CGFloat = 6
    /// Gap between fields on the same row.
    static let fieldSpacing: CGFloat = 16
    /// Gap between stacked rows inside one card.
    static let rowSpacing: CGFloat = 18
}

/// A label above a control, both flush with the field's leading edge.
struct FormField<Content: View>: View {
    let title: String
    let width: CGFloat
    @ViewBuilder var content: () -> Content

    var body: some View {
        VStack(alignment: .leading, spacing: FormMetrics.labelSpacing) {
            Text(title)
                .font(.subheadline)
                .foregroundStyle(.secondary)
                .lineLimit(1)
                .truncationMode(.tail)
                .frame(maxWidth: .infinity, alignment: .leading)
            content()
                .labelsHidden()
                .controlSize(.regular)
                .frame(maxWidth: .infinity, alignment: .leading)
        }
        .frame(width: width, alignment: .leading)
    }
}

/// A row of `FormField`s, laid out left to right at their natural widths.
///
/// The row scrolls sideways rather than reporting a minimum width. Fixed-width
/// controls otherwise raise the hosting view's minimum size, which forces the
/// whole window wider on pages that carry a lot of them — the window then jumps
/// between pages and the sidebar looks like it is being squeezed. With the
/// scroll view in place, every page has the same window minimum.
struct FormRow<Content: View>: View {
    var spacing: CGFloat = FormMetrics.fieldSpacing
    @ViewBuilder var content: () -> Content

    var body: some View {
        ScrollView(.horizontal, showsIndicators: false) {
            HStack(alignment: .top, spacing: spacing) {
                content()
            }
            // Keeps focus rings and control shadows from being clipped.
            .padding(.vertical, 2)
            .padding(.trailing, 2)
        }
        .frame(maxWidth: .infinity, alignment: .leading)
    }
}

/// Small "i" affordance carrying an option's explanation, mirroring the info
/// glyphs beside the Windows GUI's checkboxes.
struct InfoHint: View {
    let text: String

    var body: some View {
        Image(systemName: "info.circle")
            .font(.callout)
            .foregroundStyle(.secondary)
            .help(text)
            .accessibilityLabel(Text(text))
    }
}

/// A checkbox with its explanation on both the label and an adjacent "i".
struct HintedToggle: View {
    let title: String
    let hint: String
    @Binding var isOn: Bool

    var body: some View {
        HStack(spacing: 5) {
            Toggle(title, isOn: $isOn)
                .toggleStyle(.checkbox)
                .help(hint)
            InfoHint(text: hint)
        }
    }
}

/// Card-width explanatory banner used above option rows.
struct FormBanner: View {
    let text: String

    var body: some View {
        Text(text)
            .font(.caption)
            .foregroundStyle(.secondary)
            .fixedSize(horizontal: false, vertical: true)
            .padding(8)
            .frame(maxWidth: .infinity, alignment: .leading)
            .background(RoundedRectangle(cornerRadius: 8).fill(Color.accentColor.opacity(0.08)))
    }
}
