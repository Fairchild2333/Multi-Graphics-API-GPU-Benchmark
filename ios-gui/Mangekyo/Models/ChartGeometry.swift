// ChartGeometry.swift — Scales, ticks and positions of the charts.
//
// Pure layout: data and a size in, lines, bars and labels in points out. The
// SwiftUI views in BenchCharts.swift only draw the result. The rules match the
// Windows and Android apps: logarithmic axes for a sweep over data-set sizes,
// a value axis from zero otherwise, 1-2-5 tick steps.

import Foundation

struct PlotPoint: Hashable, Sendable {
    var x: Double
    var y: Double
}

/// A line of a chart. `slot` picks the colour and belongs to what the line
/// shows, so a line keeps its colour when another one is left out.
struct PlotSeries: Hashable, Sendable {
    var label: String
    var slot: Int
    var points: [PlotPoint]
}

/// A vertical reference mark on the x axis, such as a cache capacity.
struct PlotMarker: Hashable, Sendable {
    var x: Double
    var label: String
}

struct PlotSegment {
    var from: CGPoint
    var to: CGPoint
}

enum PlotAnchor {
    /// The text ends at the point, centred on it vertically.
    case trailing
    /// The text hangs below the point, centred on it horizontally.
    case top
}

struct PlotLabel {
    var text: String
    var at: CGPoint
    var anchor: PlotAnchor
}

struct PlotLine {
    var slot: Int
    var points: [CGPoint]
}

struct PlotBar {
    var rect: CGRect
    var radius: CGFloat
}

enum PlotMath {
    /// The smallest of 1, 2, 5 x 10^k that is at least `raw`.
    static func niceStep(_ raw: Double) -> Double {
        guard raw.isFinite, raw > 0 else { return 1 }
        let magnitude = pow(10.0, floor(log10(raw)))
        let fraction = raw / magnitude
        let step: Double = fraction <= 1 ? 1 : (fraction <= 2 ? 2 : (fraction <= 5 ? 5 : 10))
        return step * magnitude
    }

    /// Even steps from zero that cover `top`, at most about five of them.
    static func ticksFromZero(to top: Double) -> [Double] {
        let step = niceStep(top / 4.0)
        var ticks: [Double] = []
        var value = 0.0
        while value <= top + step * 1e-6, ticks.count < 12 {
            ticks.append(value)
            value += step
        }
        return ticks
    }
}

struct LinePlotGeometry {
    var grid: [PlotSegment] = []
    var ticks: [PlotSegment] = []
    var markerLines: [PlotSegment] = []
    var labels: [PlotLabel] = []
    var lines: [PlotLine] = []

    /// With `logBytes` both axes are logarithmic and x is a size in bytes;
    /// otherwise x is seconds and the value axis starts at zero.
    static func make(series: [PlotSeries], markers: [PlotMarker], size: CGSize,
                     logBytes: Bool) -> LinePlotGeometry? {
        func usable(_ point: PlotPoint) -> Bool {
            point.x.isFinite && point.y.isFinite && (!logBytes || (point.x > 0 && point.y > 0))
        }
        let plotted = series
            .map { PlotSeries(label: $0.label, slot: $0.slot, points: $0.points.filter(usable)) }
            .filter { $0.points.count >= 2 }
        let all = plotted.flatMap { $0.points }
        guard let xLo = all.map({ $0.x }).min(), let xHi = all.map({ $0.x }).max(),
              let yLo = all.map({ $0.y }).min(), let yHi = all.map({ $0.y }).max(),
              xHi > xLo, yHi > 0 else { return nil }

        func tx(_ value: Double) -> Double { logBytes ? log2(value) : value }
        func ty(_ value: Double) -> Double { logBytes ? log10(value) : value }
        let lxLo = tx(xLo)
        let lxHi = tx(xHi)
        let rawLo = logBytes ? ty(yLo) : 0.0
        let rawHi = ty(yHi)
        let pad = logBytes ? max(0.05, (rawHi - rawLo) * 0.08) : rawHi * 0.08
        let lyLo = logBytes ? rawLo - pad : 0.0
        let lyHi = rawHi + pad

        let left: CGFloat = 40
        let right: CGFloat = 10
        let top: CGFloat = markers.isEmpty ? 6 : 20
        let bottom: CGFloat = 20
        let plotW = size.width - left - right
        let plotH = size.height - top - bottom
        guard plotW > 0, plotH > 0, lxHi > lxLo, lyHi > lyLo else { return nil }
        func px(_ value: Double) -> CGFloat { left + CGFloat((tx(value) - lxLo) / (lxHi - lxLo)) * plotW }
        func py(_ value: Double) -> CGFloat { top + CGFloat((lyHi - ty(value)) / (lyHi - lyLo)) * plotH }

        var geometry = LinePlotGeometry()

        // Value axis: 1-2-5 steps on a log axis, even steps from zero otherwise.
        var yTicks: [Double] = []
        if logBytes {
            func ticks(_ multipliers: [Double]) -> [Double] {
                var out: [Double] = []
                var exponent = Int(floor(lyLo)) - 1
                while exponent <= Int(ceil(lyHi)) {
                    for multiplier in multipliers {
                        let value = multiplier * pow(10.0, Double(exponent))
                        let position = log10(value)
                        if position >= lyLo - 1e-9, position <= lyHi + 1e-9 { out.append(value) }
                    }
                    exponent += 1
                }
                return out.sorted()
            }
            yTicks = ticks([1, 2, 5])
            if yTicks.count > 6 { yTicks = ticks([1]) }
            if yTicks.count < 2 { yTicks = ticks([1, 1.5, 2, 3, 5, 7]) }
        } else {
            yTicks = PlotMath.ticksFromZero(to: lyHi)
        }
        for tick in yTicks {
            let y = (!logBytes && tick == 0) ? top + plotH : py(tick)
            geometry.grid.append(PlotSegment(from: CGPoint(x: left, y: y), to: CGPoint(x: left + plotW, y: y)))
            geometry.labels.append(PlotLabel(text: EngineFormat.tick(tick), at: CGPoint(x: left - 4, y: y),
                                             anchor: .trailing))
        }

        // X axis: sizes at powers of four or more, or seconds at even steps.
        var xTicks: [Double] = []
        if logBytes {
            // Width of a step of four on the axis.
            let perStep = plotW / CGFloat(lxHi - lxLo) * 2
            let factor: Double = perStep >= 46 ? 4 : (perStep * 2 >= 46 ? 16 : 64)
            var bytes = 1024.0
            while bytes <= xHi * 1.0001, xTicks.count < 40 {
                if bytes >= xLo * 0.9999 { xTicks.append(bytes) }
                bytes *= factor
            }
        } else {
            let step = PlotMath.niceStep((xHi - xLo) / max(2.0, Double(plotW / 56)))
            var value = (xLo / step - 1e-9).rounded(.up) * step
            while value <= xHi + step * 1e-6, xTicks.count < 40 {
                xTicks.append(value)
                value += step
            }
        }
        let baseline = top + plotH
        geometry.grid.append(PlotSegment(from: CGPoint(x: left, y: baseline),
                                         to: CGPoint(x: left + plotW, y: baseline)))
        for tick in xTicks {
            let x = px(tick)
            geometry.ticks.append(PlotSegment(from: CGPoint(x: x, y: baseline), to: CGPoint(x: x, y: baseline + 3)))
            let text = logBytes ? EngineFormat.bytes(tick) : "\(EngineFormat.tick(tick)) s"
            geometry.labels.append(PlotLabel(text: text, at: CGPoint(x: x, y: baseline + 5), anchor: .top))
        }

        // Reference marks: a dashed hairline with its label above the plot.
        for marker in markers where marker.x >= xLo && marker.x <= xHi && (!logBytes || marker.x > 0) {
            let x = px(marker.x)
            geometry.markerLines.append(PlotSegment(from: CGPoint(x: x, y: top), to: CGPoint(x: x, y: baseline)))
            geometry.labels.append(PlotLabel(text: marker.label, at: CGPoint(x: x, y: 2), anchor: .top))
        }

        for item in plotted {
            let ordered = item.points.sorted { $0.x < $1.x }
            geometry.lines.append(PlotLine(slot: item.slot,
                                           points: ordered.map { CGPoint(x: px($0.x), y: py($0.y)) }))
        }
        return geometry
    }
}

/// Vertical bars on a value axis from zero, for many numbered items.
struct ColumnPlotGeometry {
    var grid: [PlotSegment] = []
    var labels: [PlotLabel] = []
    var bars: [PlotBar] = []

    static func make(values: [Double], labels: [String], size: CGSize) -> ColumnPlotGeometry? {
        let finite = values.map { $0.isFinite && $0 > 0 ? $0 : 0 }
        guard let maxValue = finite.max(), maxValue > 0 else { return nil }
        let left: CGFloat = 40
        let right: CGFloat = 8
        let top: CGFloat = 6
        let bottom: CGFloat = 20
        let plotW = size.width - left - right
        let plotH = size.height - top - bottom
        guard plotW > 0, plotH > 0 else { return nil }

        var geometry = ColumnPlotGeometry()
        let axisMax = maxValue * 1.08
        let baseline = top + plotH
        for tick in PlotMath.ticksFromZero(to: axisMax) {
            let y = top + CGFloat(1.0 - tick / axisMax) * plotH
            geometry.grid.append(PlotSegment(from: CGPoint(x: left, y: y), to: CGPoint(x: left + plotW, y: y)))
            geometry.labels.append(PlotLabel(text: EngineFormat.tick(tick), at: CGPoint(x: left - 4, y: y),
                                             anchor: .trailing))
        }

        let slot = plotW / CGFloat(finite.count)
        // Bars stay at least 2 points apart.
        let barWidth = min(max(slot - 2, 2), 22)
        let labelEvery = max(1, Int((22 / slot).rounded(.up)))
        for (index, value) in finite.enumerated() {
            let center = left + (CGFloat(index) + 0.5) * slot
            let height = max(1, CGFloat(value / axisMax) * plotH)
            geometry.bars.append(PlotBar(
                rect: CGRect(x: center - barWidth / 2, y: baseline - height, width: barWidth, height: height),
                radius: min(4, barWidth / 2)))
            if index % labelEvery == 0, index < labels.count {
                geometry.labels.append(PlotLabel(text: labels[index], at: CGPoint(x: center, y: baseline + 4),
                                                 anchor: .top))
            }
        }
        return geometry
    }
}
