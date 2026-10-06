// BenchCharts.swift — Charts drawn with SwiftUI Canvas.
//
// They share the rules of the Windows and Android apps: one colour per series
// in a fixed order (blue, orange, aqua), 2 pt lines, a legend for two or more
// series, a hairline grid, and text in text colours. The positions come from
// ChartGeometry.swift; this file only draws.

import SwiftUI

enum SeriesPalette {
    /// Series colours in their fixed order. Each pair is checked for contrast
    /// and colour-blind separation on a light and on a dark surface.
    static func color(_ slot: Int, dark: Bool) -> Color {
        let light: [UInt32] = [0x2A78D6, 0xEB6834, 0x1BAF7A]
        let night: [UInt32] = [0x3987E5, 0xD95926, 0x199E70]
        let hex = (dark ? night : light)[min(max(slot, 0), 2)]
        return Color(red: Double((hex >> 16) & 0xFF) / 255.0,
                     green: Double((hex >> 8) & 0xFF) / 255.0,
                     blue: Double(hex & 0xFF) / 255.0)
    }
}

enum PlotPainter {
    static var gridColor: Color { Color.primary.opacity(0.12) }
    static var markColor: Color { Color.primary.opacity(0.34) }

    static func stroke(_ segments: [PlotSegment], in context: GraphicsContext, color: Color,
                       dash: [CGFloat] = []) {
        guard !segments.isEmpty else { return }
        var path = Path()
        for segment in segments {
            path.move(to: segment.from)
            path.addLine(to: segment.to)
        }
        context.stroke(path, with: .color(color), style: StrokeStyle(lineWidth: 1, dash: dash))
    }

    static func labels(_ labels: [PlotLabel], in context: GraphicsContext, width: CGFloat) {
        for label in labels {
            let text = context.resolve(
                Text(label.text).font(.system(size: 10)).foregroundColor(.secondary))
            switch label.anchor {
            case .trailing:
                context.draw(text, at: label.at, anchor: .trailing)
            case .top:
                // Keep the text inside the chart.
                let half = text.measure(in: CGSize(width: 400, height: 40)).width / 2
                let x = min(max(label.at.x, half), max(half, width - half))
                context.draw(text, at: CGPoint(x: x, y: label.at.y), anchor: .top)
            }
        }
    }

    static func draw(_ geometry: LinePlotGeometry, in context: GraphicsContext, width: CGFloat, dark: Bool) {
        stroke(geometry.grid, in: context, color: gridColor)
        stroke(geometry.ticks, in: context, color: gridColor)
        stroke(geometry.markerLines, in: context, color: markColor, dash: [4, 4])
        labels(geometry.labels, in: context, width: width)
        for line in geometry.lines {
            guard let first = line.points.first, let last = line.points.last else { continue }
            var path = Path()
            path.move(to: first)
            for point in line.points.dropFirst() {
                path.addLine(to: point)
            }
            let color = SeriesPalette.color(line.slot, dark: dark)
            context.stroke(path, with: .color(color),
                           style: StrokeStyle(lineWidth: 2, lineCap: .round, lineJoin: .round))
            context.fill(Path(ellipseIn: CGRect(x: last.x - 3.5, y: last.y - 3.5, width: 7, height: 7)),
                         with: .color(color))
        }
    }

    static func draw(_ geometry: ColumnPlotGeometry, in context: GraphicsContext, width: CGFloat, dark: Bool) {
        stroke(geometry.grid, in: context, color: gridColor)
        labels(geometry.labels, in: context, width: width)
        let color = SeriesPalette.color(0, dark: dark)
        for bar in geometry.bars {
            context.fill(Path(roundedRect: bar.rect, cornerRadius: bar.radius), with: .color(color))
        }
    }
}

struct PlotLegend: View {
    let series: [PlotSeries]
    let dark: Bool

    var body: some View {
        HStack(spacing: 14) {
            ForEach(series.indices, id: \.self) { index in
                HStack(spacing: 6) {
                    RoundedRectangle(cornerRadius: 2)
                        .fill(SeriesPalette.color(series[index].slot, dark: dark))
                        .frame(width: 14, height: 3)
                    Text(series[index].label)
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
            }
        }
    }
}

/// Line chart. With `logBytes` both axes are logarithmic and x is a size in
/// bytes; otherwise x is seconds and the value axis starts at zero.
struct LinePlotView: View {
    let series: [PlotSeries]
    var markers: [PlotMarker] = []
    var logBytes = true

    @Environment(\.colorScheme) private var colorScheme

    var body: some View {
        let plotted = series.filter { $0.points.count >= 2 }
        let marks = markers
        let logScale = logBytes
        let dark = colorScheme == .dark
        if !plotted.isEmpty {
            VStack(alignment: .leading, spacing: 4) {
                if plotted.count >= 2 {
                    PlotLegend(series: plotted, dark: dark)
                }
                Canvas { context, size in
                    guard let geometry = LinePlotGeometry.make(series: plotted, markers: marks, size: size,
                                                               logBytes: logScale) else { return }
                    PlotPainter.draw(geometry, in: context, width: size.width, dark: dark)
                }
                .frame(maxWidth: .infinity)
                .frame(height: marks.isEmpty ? 190 : 206)
            }
        }
    }
}

/// Vertical bars in one hue on a value axis, for many numbered items.
struct ColumnPlotView: View {
    let values: [Double]
    let labels: [String]

    @Environment(\.colorScheme) private var colorScheme

    var body: some View {
        let heights = values
        let names = labels
        let dark = colorScheme == .dark
        Canvas { context, size in
            guard let geometry = ColumnPlotGeometry.make(values: heights, labels: names, size: size) else { return }
            PlotPainter.draw(geometry, in: context, width: size.width, dark: dark)
        }
        .frame(maxWidth: .infinity)
        .frame(height: 190)
    }
}

struct BarItem: Identifiable, Hashable {
    var id: Int
    var label: String
    var value: Double
}

/// Horizontal bars in one hue for a few named items, with the value at the bar end.
struct BarRowsView: View {
    let items: [BarItem]
    let unit: String

    @Environment(\.colorScheme) private var colorScheme

    var body: some View {
        let maxValue = items.map { $0.value }.max() ?? 0
        let color = SeriesPalette.color(0, dark: colorScheme == .dark)
        if maxValue > 0 {
            VStack(alignment: .leading, spacing: 8) {
                ForEach(items) { item in
                    BarRow(label: item.label,
                           valueText: EngineFormat.number(item.value) + " " + unit,
                           fraction: CGFloat(max(item.value, 0) / maxValue),
                           color: color)
                }
            }
        }
    }
}

private struct BarRow: View {
    let label: String
    let valueText: String
    let fraction: CGFloat
    let color: Color

    var body: some View {
        VStack(alignment: .leading, spacing: 2) {
            Text(label)
                .font(.caption)
                .foregroundStyle(.secondary)
                .lineLimit(1)
            GeometryReader { proxy in
                HStack(spacing: 6) {
                    // The value text needs room, so the longest bar takes 68% of the row.
                    RoundedRectangle(cornerRadius: 4)
                        .fill(color)
                        .frame(width: max(2, proxy.size.width * 0.68 * fraction), height: 14)
                    Text(valueText)
                        .font(.caption)
                        .fontWeight(.medium)
                        .lineLimit(1)
                    Spacer(minLength: 0)
                }
                .frame(height: 16)
            }
            .frame(height: 16)
        }
    }
}
