package com.mangekyo.benchmark.ui.components

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.PathEffect
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.StrokeJoin
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.drawText
import androidx.compose.ui.text.rememberTextMeasurer
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.mangekyo.benchmark.core.formatBytes
import com.mangekyo.benchmark.core.formatNumber
import com.mangekyo.benchmark.core.formatTick
import kotlin.math.ceil
import kotlin.math.floor
import kotlin.math.ln
import kotlin.math.log10
import kotlin.math.pow

/**
 * Charts drawn with Compose, sharing the rules of the Windows GUI: one colour
 * per series in a fixed order (blue, orange, aqua), 2 dp lines, a legend for
 * two or more series, hairline grid, and text in text colours.
 */

data class ChartSeries(val label: String, val color: Color, val points: List<Pair<Double, Double>>)

/** A vertical reference mark on the x axis, such as a cache capacity. */
data class ChartMarker(val x: Double, val label: String)

/** Series colours in their fixed order, for the current theme. */
@Composable
fun seriesColors(): List<Color> =
    if (isSystemInDarkTheme()) listOf(Color(0xFF3987E5), Color(0xFFD95926), Color(0xFF199E70))
    else listOf(Color(0xFF2A78D6), Color(0xFFEB6834), Color(0xFF1BAF7A))

/** The smallest of 1, 2, 5 x 10^k that is at least [raw]. */
private fun niceStep(raw: Double): Double {
    if (raw <= 0.0) return 1.0
    val magnitude = 10.0.pow(floor(log10(raw)))
    val fraction = raw / magnitude
    return (if (fraction <= 1.0) 1.0 else if (fraction <= 2.0) 2.0 else if (fraction <= 5.0) 5.0 else 10.0) * magnitude
}

private fun log2(value: Double): Double = ln(value) / ln(2.0)

@OptIn(ExperimentalLayoutApi::class)
@Composable
fun ChartLegend(series: List<ChartSeries>, modifier: Modifier = Modifier) {
    FlowRow(modifier = modifier, horizontalArrangement = Arrangement.spacedBy(14.dp)) {
        series.forEach { item ->
            Row(verticalAlignment = Alignment.CenterVertically) {
                Box(
                    Modifier
                        .size(width = 14.dp, height = 3.dp)
                        .background(item.color, RoundedCornerShape(2.dp)),
                )
                Spacer(Modifier.width(6.dp))
                Text(
                    item.label,
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }
    }
}

/**
 * Line chart. With [logBytes] both axes are logarithmic and x is a size in
 * bytes; otherwise x is seconds and the value axis starts at zero.
 */
@Composable
fun LineChart(
    series: List<ChartSeries>,
    modifier: Modifier = Modifier,
    logBytes: Boolean = true,
    markers: List<ChartMarker> = emptyList(),
) {
    val plotted = series.filter { it.points.size >= 2 }
    if (plotted.isEmpty()) return
    val measurer = rememberTextMeasurer()
    val labelStyle = TextStyle(color = MaterialTheme.colorScheme.onSurfaceVariant, fontSize = 10.sp)
    val gridColor = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.12f)
    val markColor = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.34f)
    val surface = MaterialTheme.colorScheme.surfaceContainerLow

    Column(modifier = modifier) {
        if (plotted.size >= 2) ChartLegend(plotted, Modifier.padding(bottom = 4.dp))
        Canvas(
            modifier = Modifier
                .fillMaxWidth()
                .height(if (markers.isEmpty()) 190.dp else 206.dp),
        ) {
            val all = plotted.flatMap { it.points }
            val xLo = all.minOf { it.first }
            val xHi = all.maxOf { it.first }
            val yLo = all.minOf { it.second }
            val yHi = all.maxOf { it.second }
            if (xHi <= xLo || yHi <= 0.0) return@Canvas

            fun tx(v: Double) = if (logBytes) log2(v) else v
            fun ty(v: Double) = if (logBytes) log10(v) else v
            val lxLo = tx(xLo)
            val lxHi = tx(xHi)
            var lyLo = if (logBytes) ty(yLo) else 0.0
            var lyHi = ty(yHi)
            val pad = if (logBytes) maxOf(0.05, (lyHi - lyLo) * 0.08) else lyHi * 0.08
            if (logBytes) lyLo -= pad
            lyHi += pad

            val left = 40.dp.toPx()
            val right = 10.dp.toPx()
            val top = (if (markers.isEmpty()) 6.dp else 20.dp).toPx()
            val bottom = 20.dp.toPx()
            val plotW = size.width - left - right
            val plotH = size.height - top - bottom
            if (plotW <= 0f || plotH <= 0f) return@Canvas
            fun px(v: Double) = left + ((tx(v) - lxLo) / (lxHi - lxLo)).toFloat() * plotW
            fun py(v: Double) = top + ((lyHi - ty(v)) / (lyHi - lyLo)).toFloat() * plotH

            // Value axis: 1-2-5 steps on a log axis, even steps from zero otherwise.
            val yTicks = ArrayList<Double>()
            if (logBytes) {
                fun ticks(multipliers: List<Double>): List<Double> {
                    val out = ArrayList<Double>()
                    var exponent = floor(lyLo).toInt() - 1
                    while (exponent <= ceil(lyHi).toInt()) {
                        for (m in multipliers) {
                            val value = m * 10.0.pow(exponent)
                            val lv = log10(value)
                            if (lv >= lyLo - 1e-9 && lv <= lyHi + 1e-9) out.add(value)
                        }
                        exponent++
                    }
                    return out.sorted()
                }
                var chosen = ticks(listOf(1.0, 2.0, 5.0))
                if (chosen.size > 6) chosen = ticks(listOf(1.0))
                if (chosen.size < 2) chosen = ticks(listOf(1.0, 1.5, 2.0, 3.0, 5.0, 7.0))
                yTicks.addAll(chosen)
            } else {
                val step = niceStep(lyHi / 4.0)
                var value = 0.0
                while (value <= lyHi + step * 1e-6) {
                    yTicks.add(value)
                    value += step
                }
            }
            for (tick in yTicks) {
                val y = if (!logBytes && tick == 0.0) top + plotH else py(tick)
                drawLine(gridColor, Offset(left, y), Offset(left + plotW, y), strokeWidth = 1f)
                val layout = measurer.measure(formatTick(tick), labelStyle)
                drawText(layout, topLeft = Offset(left - layout.size.width - 4.dp.toPx(), y - layout.size.height / 2f))
            }

            // X axis: sizes at powers of four or sixteen, or seconds at even steps.
            val xTicks = ArrayList<Double>()
            if (logBytes) {
                val perStep = plotW / (lxHi - lxLo).toFloat() * 2f
                val factor = if (perStep >= 46.dp.toPx()) 4.0 else if (perStep * 2f >= 46.dp.toPx()) 16.0 else 64.0
                var bytes = 1024.0
                while (bytes <= xHi * 1.0001) {
                    if (bytes >= xLo * 0.9999) xTicks.add(bytes)
                    bytes *= factor
                }
            } else {
                val step = niceStep((xHi - xLo) / maxOf(2.0, (plotW / 56.dp.toPx()).toDouble()))
                var value = ceil(xLo / step - 1e-9) * step
                while (value <= xHi + step * 1e-6) {
                    xTicks.add(value)
                    value += step
                }
            }
            drawLine(gridColor, Offset(left, top + plotH), Offset(left + plotW, top + plotH), strokeWidth = 1f)
            for (tick in xTicks) {
                val x = px(tick)
                drawLine(gridColor, Offset(x, top + plotH), Offset(x, top + plotH + 3.dp.toPx()), strokeWidth = 1f)
                val text = if (logBytes) formatBytes(tick) else "${formatTick(tick)} s"
                val layout = measurer.measure(text, labelStyle)
                val labelX = (x - layout.size.width / 2f).coerceIn(0f, size.width - layout.size.width)
                drawText(layout, topLeft = Offset(labelX, top + plotH + 5.dp.toPx()))
            }

            // Reference marks: a dashed hairline with its label above the plot.
            for (marker in markers) {
                if (marker.x < xLo || marker.x > xHi) continue
                val x = px(marker.x)
                drawLine(
                    markColor, Offset(x, top), Offset(x, top + plotH), strokeWidth = 1f,
                    pathEffect = PathEffect.dashPathEffect(floatArrayOf(6f, 6f)),
                )
                val layout = measurer.measure(marker.label, labelStyle)
                val labelX = (x - layout.size.width / 2f).coerceIn(0f, size.width - layout.size.width)
                drawText(layout, topLeft = Offset(labelX, 0f))
            }

            for (item in plotted) {
                val path = Path()
                item.points.sortedBy { it.first }.forEachIndexed { index, point ->
                    val x = px(point.first)
                    val y = py(maxOf(point.second, if (logBytes) yLo else 0.0))
                    if (index == 0) path.moveTo(x, y) else path.lineTo(x, y)
                }
                drawPath(
                    path, item.color,
                    style = Stroke(width = 2.dp.toPx(), cap = StrokeCap.Round, join = StrokeJoin.Round),
                )
                val last = item.points.maxByOrNull { it.first } ?: continue
                val end = Offset(px(last.first), py(maxOf(last.second, if (logBytes) yLo else 0.0)))
                drawCircle(surface, radius = 5.dp.toPx(), center = end)
                drawCircle(item.color, radius = 3.5.dp.toPx(), center = end)
            }
        }
    }
}

/** Horizontal bars in one hue for a few named items, with the value at the bar end. */
@Composable
fun BarRows(items: List<Pair<String, Double>>, unit: String, modifier: Modifier = Modifier) {
    val maxValue = items.maxOfOrNull { it.second } ?: return
    if (maxValue <= 0.0) return
    val color = seriesColors()[0]
    Column(modifier = modifier, verticalArrangement = Arrangement.spacedBy(8.dp)) {
        items.forEach { (label, value) ->
            Column {
                Text(
                    label,
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                )
                Row(verticalAlignment = Alignment.CenterVertically) {
                    // The value text needs room, so the longest bar takes 68% of the row.
                    val fraction = (value / maxValue * 0.68).toFloat().coerceIn(0.01f, 0.68f)
                    Box(
                        Modifier
                            .weight(fraction)
                            .height(14.dp)
                            .background(color, RoundedCornerShape(4.dp)),
                    )
                    Text(
                        "${formatNumber(value)} $unit",
                        style = MaterialTheme.typography.labelMedium,
                        modifier = Modifier
                            .weight(1f - fraction)
                            .padding(start = 6.dp),
                        maxLines = 1,
                    )
                }
            }
        }
    }
}

/** Vertical bars in one hue on a value axis, for many numbered items. */
@Composable
fun ColumnChart(items: List<Pair<String, Double>>, modifier: Modifier = Modifier) {
    val maxValue = items.maxOfOrNull { it.second } ?: return
    if (maxValue <= 0.0) return
    val measurer = rememberTextMeasurer()
    val labelStyle = TextStyle(color = MaterialTheme.colorScheme.onSurfaceVariant, fontSize = 10.sp)
    val gridColor = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.12f)
    val color = seriesColors()[0]
    Canvas(
        modifier = modifier
            .fillMaxWidth()
            .height(190.dp),
    ) {
        val left = 40.dp.toPx()
        val right = 8.dp.toPx()
        val top = 6.dp.toPx()
        val bottom = 20.dp.toPx()
        val plotW = size.width - left - right
        val plotH = size.height - top - bottom
        if (plotW <= 0f || plotH <= 0f) return@Canvas
        val axisMax = maxValue * 1.08
        val step = niceStep(axisMax / 4.0)
        var tick = 0.0
        while (tick <= axisMax + step * 1e-6) {
            val y = top + (1.0 - tick / axisMax).toFloat() * plotH
            drawLine(gridColor, Offset(left, y), Offset(left + plotW, y), strokeWidth = 1f)
            val layout = measurer.measure(formatTick(tick), labelStyle)
            drawText(layout, topLeft = Offset(left - layout.size.width - 4.dp.toPx(), y - layout.size.height / 2f))
            tick += step
        }
        val slot = plotW / items.size
        // Bars stay at least 2 px apart.
        val barWidth = (slot - 2f).coerceIn(2f, 22.dp.toPx())
        val labelEvery = maxOf(1, ceil(22.dp.toPx() / slot).toInt())
        items.forEachIndexed { index, (label, value) ->
            val center = left + (index + 0.5f) * slot
            val height = maxOf(1f, (value / axisMax).toFloat() * plotH)
            val radius = minOf(4.dp.toPx(), barWidth / 2f)
            drawRoundRect(
                color,
                topLeft = Offset(center - barWidth / 2f, top + plotH - height),
                size = Size(barWidth, height),
                cornerRadius = CornerRadius(radius, radius),
            )
            if (index % labelEvery == 0) {
                val layout = measurer.measure(label, labelStyle)
                drawText(layout, topLeft = Offset(center - layout.size.width / 2f, top + plotH + 4.dp.toPx()))
            }
        }
    }
}
