// Charts drawn on an ArkUI Canvas, sharing the rules of the other front ends:
// one colour per series in a fixed order (blue, orange, aqua), 2 vp lines, a
// legend for two or more series, a hairline grid, and text in text colours.

import { formatBytes, formatNumber, formatTick } from '../common/Format';

/** A line of a chart. `slot` picks the colour and belongs to what the line shows. */
export class ChartSeries {
  label: string;
  slot: number;
  xs: number[] = [];
  ys: number[] = [];

  constructor(label: string, slot: number) {
    this.label = label;
    this.slot = slot;
  }

  add(x: number, y: number): void {
    this.xs.push(x);
    this.ys.push(y);
  }
}

/** A vertical reference mark on the x axis, such as a cache capacity. */
export class ChartMarker {
  x: number;
  label: string;

  constructor(x: number, label: string) {
    this.x = x;
    this.label = label;
  }
}

export class BarItem {
  label: string;
  value: number;

  constructor(label: string, value: number) {
    this.label = label;
    this.value = value;
  }
}

const LIGHT_SERIES: string[] = ['#2A78D6', '#EB6834', '#1BAF7A'];
const DARK_SERIES: string[] = ['#3987E5', '#D95926', '#199E70'];

/** Series colours in their fixed order; each pair is checked on a light and a dark surface. */
export function seriesColor(slot: number, dark: boolean): string {
  const index: number = Math.min(Math.max(slot, 0), 2);
  return dark ? DARK_SERIES[index] : LIGHT_SERIES[index];
}

function gridColor(dark: boolean): string {
  return dark ? 'rgba(255, 255, 255, 0.16)' : 'rgba(0, 0, 0, 0.12)';
}

function markColor(dark: boolean): string {
  return dark ? 'rgba(255, 255, 255, 0.42)' : 'rgba(0, 0, 0, 0.34)';
}

function labelColor(dark: boolean): string {
  return dark ? '#B4B4B4' : '#5F5F5F';
}

const LABEL_FONT: string = 'normal normal 10vp sans-serif';

/** The smallest of 1, 2, 5 x 10^k that is at least `raw`. */
function niceStep(raw: number): number {
  if (!isFinite(raw) || raw <= 0) {
    return 1;
  }
  const magnitude: number = Math.pow(10, Math.floor(Math.log10(raw)));
  const fraction: number = raw / magnitude;
  const step: number = fraction <= 1 ? 1 : (fraction <= 2 ? 2 : (fraction <= 5 ? 5 : 10));
  return step * magnitude;
}

/** Even steps from zero that cover `top`. */
function ticksFromZero(top: number): number[] {
  const step: number = niceStep(top / 4);
  const ticks: number[] = [];
  let value: number = 0;
  while (value <= top + step * 1e-6 && ticks.length < 12) {
    ticks.push(value);
    value += step;
  }
  return ticks;
}

function logTicks(low: number, high: number, multipliers: number[]): number[] {
  const out: number[] = [];
  let exponent: number = Math.floor(low) - 1;
  while (exponent <= Math.ceil(high)) {
    for (const multiplier of multipliers) {
      const value: number = multiplier * Math.pow(10, exponent);
      const position: number = Math.log10(value);
      if (position >= low - 1e-9 && position <= high + 1e-9) {
        out.push(value);
      }
    }
    exponent++;
  }
  out.sort((a: number, b: number) => a - b);
  return out;
}

/** Draws centred text that stays inside the canvas. */
function drawCentredLabel(context: CanvasRenderingContext2D, text: string, x: number, y: number,
  width: number): void {
  const half: number = context.measureText(text).width / 2;
  const clamped: number = Math.min(Math.max(x, half), Math.max(half, width - half));
  context.textAlign = 'center';
  context.textBaseline = 'top';
  context.fillText(text, clamped, y);
}

/**
 * Line chart. With `logBytes` both axes are logarithmic and x is a size in
 * bytes; otherwise x is seconds and the value axis starts at zero.
 */
@ComponentV2
export struct LineChart {
  @Param series: ChartSeries[] = [];
  @Param markers: ChartMarker[] = [];
  @Param logBytes: boolean = true;
  @Param dark: boolean = false;
  private settings: RenderingContextSettings = new RenderingContextSettings(true);
  private context: CanvasRenderingContext2D = new CanvasRenderingContext2D(this.settings);
  private ready: boolean = false;

  @Monitor('series', 'markers', 'logBytes', 'dark')
  onInputChange(monitor: IMonitor): void {
    this.draw();
  }

  private plotted(): ChartSeries[] {
    return this.series.filter((item: ChartSeries) => item.xs.length >= 2);
  }

  private tx(value: number): number {
    return this.logBytes ? Math.log2(value) : value;
  }

  private ty(value: number): number {
    return this.logBytes ? Math.log10(value) : value;
  }

  private draw(): void {
    if (!this.ready) {
      return;
    }
    const context: CanvasRenderingContext2D = this.context;
    const width: number = context.width;
    const height: number = context.height;
    context.clearRect(0, 0, width, height);

    const plotted: ChartSeries[] = this.plotted();
    let xLo: number = Infinity;
    let xHi: number = -Infinity;
    let yLo: number = Infinity;
    let yHi: number = -Infinity;
    for (const item of plotted) {
      for (let i = 0; i < item.xs.length; i++) {
        const x: number = item.xs[i];
        const y: number = item.ys[i];
        if (!isFinite(x) || !isFinite(y) || (this.logBytes && (x <= 0 || y <= 0))) {
          continue;
        }
        xLo = Math.min(xLo, x);
        xHi = Math.max(xHi, x);
        yLo = Math.min(yLo, y);
        yHi = Math.max(yHi, y);
      }
    }
    if (!(xHi > xLo) || !(yHi > 0)) {
      return;
    }

    const lxLo: number = this.tx(xLo);
    const lxHi: number = this.tx(xHi);
    const rawLo: number = this.logBytes ? this.ty(yLo) : 0;
    const rawHi: number = this.ty(yHi);
    const pad: number = this.logBytes ? Math.max(0.05, (rawHi - rawLo) * 0.08) : rawHi * 0.08;
    const lyLo: number = this.logBytes ? rawLo - pad : 0;
    const lyHi: number = rawHi + pad;

    const left: number = 40;
    const right: number = 10;
    const top: number = this.markers.length > 0 ? 20 : 6;
    const bottom: number = 20;
    const plotW: number = width - left - right;
    const plotH: number = height - top - bottom;
    if (plotW <= 0 || plotH <= 0 || !(lxHi > lxLo) || !(lyHi > lyLo)) {
      return;
    }
    const baseline: number = top + plotH;
    const px = (value: number): number => left + (this.tx(value) - lxLo) / (lxHi - lxLo) * plotW;
    const py = (value: number): number => top + (lyHi - this.ty(value)) / (lyHi - lyLo) * plotH;

    context.font = LABEL_FONT;
    context.lineWidth = 1;
    context.setLineDash([]);

    // Value axis: 1-2-5 steps on a log axis, even steps from zero otherwise.
    let yTicks: number[] = [];
    if (this.logBytes) {
      yTicks = logTicks(lyLo, lyHi, [1, 2, 5]);
      if (yTicks.length > 6) {
        yTicks = logTicks(lyLo, lyHi, [1]);
      }
      if (yTicks.length < 2) {
        yTicks = logTicks(lyLo, lyHi, [1, 1.5, 2, 3, 5, 7]);
      }
    } else {
      yTicks = ticksFromZero(lyHi);
    }
    context.strokeStyle = gridColor(this.dark);
    context.fillStyle = labelColor(this.dark);
    for (const tick of yTicks) {
      const y: number = (!this.logBytes && tick === 0) ? baseline : py(tick);
      context.beginPath();
      context.moveTo(left, y);
      context.lineTo(left + plotW, y);
      context.stroke();
      context.textAlign = 'right';
      context.textBaseline = 'middle';
      context.fillText(formatTick(tick), left - 4, y);
    }

    // X axis: sizes at powers of four or more, or seconds at even steps.
    const xTicks: number[] = [];
    if (this.logBytes) {
      // Width of a step of four on the axis.
      const perStep: number = plotW / (lxHi - lxLo) * 2;
      const factor: number = perStep >= 46 ? 4 : (perStep * 2 >= 46 ? 16 : 64);
      let bytes: number = 1024;
      while (bytes <= xHi * 1.0001 && xTicks.length < 40) {
        if (bytes >= xLo * 0.9999) {
          xTicks.push(bytes);
        }
        bytes *= factor;
      }
    } else {
      const step: number = niceStep((xHi - xLo) / Math.max(2, plotW / 56));
      let value: number = Math.ceil(xLo / step - 1e-9) * step;
      while (value <= xHi + step * 1e-6 && xTicks.length < 40) {
        xTicks.push(value);
        value += step;
      }
    }
    context.beginPath();
    context.moveTo(left, baseline);
    context.lineTo(left + plotW, baseline);
    context.stroke();
    for (const tick of xTicks) {
      const x: number = px(tick);
      context.beginPath();
      context.moveTo(x, baseline);
      context.lineTo(x, baseline + 3);
      context.stroke();
      drawCentredLabel(context, this.logBytes ? formatBytes(tick) : formatTick(tick) + ' s', x, baseline + 5,
        width);
    }

    // Reference marks: a dashed hairline with its label above the plot.
    for (const marker of this.markers) {
      if (marker.x < xLo || marker.x > xHi || (this.logBytes && marker.x <= 0)) {
        continue;
      }
      const x: number = px(marker.x);
      context.strokeStyle = markColor(this.dark);
      context.setLineDash([4, 4]);
      context.beginPath();
      context.moveTo(x, top);
      context.lineTo(x, baseline);
      context.stroke();
      context.setLineDash([]);
      drawCentredLabel(context, marker.label, x, 2, width);
    }

    for (const item of plotted) {
      // Points in the order of x.
      const order: number[] = [];
      for (let i = 0; i < item.xs.length; i++) {
        const x: number = item.xs[i];
        const y: number = item.ys[i];
        if (isFinite(x) && isFinite(y) && (!this.logBytes || (x > 0 && y > 0))) {
          order.push(i);
        }
      }
      order.sort((a: number, b: number) => item.xs[a] - item.xs[b]);
      if (order.length < 2) {
        continue;
      }
      const color: string = seriesColor(item.slot, this.dark);
      context.strokeStyle = color;
      context.lineWidth = 2;
      context.lineJoin = 'round';
      context.lineCap = 'round';
      context.beginPath();
      let lastX: number = 0;
      let lastY: number = 0;
      for (let n = 0; n < order.length; n++) {
        lastX = px(item.xs[order[n]]);
        lastY = py(item.ys[order[n]]);
        if (n === 0) {
          context.moveTo(lastX, lastY);
        } else {
          context.lineTo(lastX, lastY);
        }
      }
      context.stroke();
      context.fillStyle = color;
      context.beginPath();
      context.arc(lastX, lastY, 3.5, 0, Math.PI * 2);
      context.fill();
      context.lineWidth = 1;
    }
  }

  build() {
    Column({ space: 4 }) {
      if (this.plotted().length >= 2) {
        Row({ space: 14 }) {
          ForEach(this.plotted(), (item: ChartSeries) => {
            Row({ space: 6 }) {
              Row()
                .width(14)
                .height(3)
                .borderRadius(2)
                .backgroundColor(seriesColor(item.slot, this.dark))
              Text(item.label)
                .fontSize(12)
                .fontColor($r('sys.color.ohos_id_color_text_secondary'))
            }
          }, (item: ChartSeries) => item.label)
        }
      }
      Canvas(this.context)
        .width('100%')
        .height(this.markers.length > 0 ? 206 : 190)
        .onReady(() => {
          this.ready = true;
          this.draw();
        })
    }
    .alignItems(HorizontalAlign.Start)
    .width('100%')
  }
}

/** Vertical bars in one hue on a value axis from zero, for many numbered items. */
@ComponentV2
export struct ColumnChart {
  @Param values: number[] = [];
  @Param labels: string[] = [];
  @Param dark: boolean = false;
  private settings: RenderingContextSettings = new RenderingContextSettings(true);
  private context: CanvasRenderingContext2D = new CanvasRenderingContext2D(this.settings);
  private ready: boolean = false;

  @Monitor('values', 'labels', 'dark')
  onInputChange(monitor: IMonitor): void {
    this.draw();
  }

  private draw(): void {
    if (!this.ready) {
      return;
    }
    const context: CanvasRenderingContext2D = this.context;
    const width: number = context.width;
    const height: number = context.height;
    context.clearRect(0, 0, width, height);

    let maxValue: number = 0;
    for (const value of this.values) {
      if (isFinite(value) && value > maxValue) {
        maxValue = value;
      }
    }
    const left: number = 40;
    const right: number = 8;
    const top: number = 6;
    const bottom: number = 20;
    const plotW: number = width - left - right;
    const plotH: number = height - top - bottom;
    if (maxValue <= 0 || plotW <= 0 || plotH <= 0) {
      return;
    }
    const baseline: number = top + plotH;
    const axisMax: number = maxValue * 1.08;

    context.font = LABEL_FONT;
    context.lineWidth = 1;
    context.setLineDash([]);
    context.strokeStyle = gridColor(this.dark);
    context.fillStyle = labelColor(this.dark);
    for (const tick of ticksFromZero(axisMax)) {
      const y: number = top + (1 - tick / axisMax) * plotH;
      context.beginPath();
      context.moveTo(left, y);
      context.lineTo(left + plotW, y);
      context.stroke();
      context.textAlign = 'right';
      context.textBaseline = 'middle';
      context.fillText(formatTick(tick), left - 4, y);
    }

    const slot: number = plotW / this.values.length;
    // Bars stay at least 2 vp apart.
    const barWidth: number = Math.min(Math.max(slot - 2, 2), 22);
    const labelEvery: number = Math.max(1, Math.ceil(22 / slot));
    const radius: number = Math.min(4, barWidth / 2);
    for (let i = 0; i < this.values.length; i++) {
      const value: number = isFinite(this.values[i]) && this.values[i] > 0 ? this.values[i] : 0;
      const centre: number = left + (i + 0.5) * slot;
      const barHeight: number = Math.max(1, value / axisMax * plotH);
      const x: number = centre - barWidth / 2;
      const y: number = baseline - barHeight;
      const corner: number = Math.min(radius, barHeight);
      // Rounded at the data end only; the bar stands on the baseline.
      context.fillStyle = seriesColor(0, this.dark);
      context.beginPath();
      context.moveTo(x, baseline);
      context.lineTo(x, y + corner);
      context.arcTo(x, y, x + corner, y, corner);
      context.lineTo(x + barWidth - corner, y);
      context.arcTo(x + barWidth, y, x + barWidth, y + corner, corner);
      context.lineTo(x + barWidth, baseline);
      context.closePath();
      context.fill();
      if (i % labelEvery === 0 && i < this.labels.length) {
        context.fillStyle = labelColor(this.dark);
        drawCentredLabel(context, this.labels[i], centre, baseline + 4, width);
      }
    }
  }

  build() {
    Canvas(this.context)
      .width('100%')
      .height(190)
      .onReady(() => {
        this.ready = true;
        this.draw();
      })
  }
}

/** Horizontal bars in one hue for a few named items, with the value at the bar end. */
@ComponentV2
export struct BarRows {
  @Param items: BarItem[] = [];
  @Param unit: string = '';
  @Param dark: boolean = false;

  private maxValue(): number {
    let maxValue: number = 0;
    for (const item of this.items) {
      if (isFinite(item.value) && item.value > maxValue) {
        maxValue = item.value;
      }
    }
    return maxValue;
  }

  /** The value text needs room, so the longest bar takes 68% of the row. */
  private barPercent(value: number): string {
    const maxValue: number = this.maxValue();
    const fraction: number = maxValue > 0 ? Math.max(value, 0) / maxValue : 0;
    return Math.max(1, fraction * 68).toFixed(1) + '%';
  }

  build() {
    Column({ space: 8 }) {
      ForEach(this.items, (item: BarItem) => {
        Column({ space: 2 }) {
          Text(item.label)
            .fontSize(12)
            .fontColor($r('sys.color.ohos_id_color_text_secondary'))
            .maxLines(1)
            .textOverflow({ overflow: TextOverflow.Ellipsis })
          Row({ space: 6 }) {
            Row()
              .width(this.barPercent(item.value))
              .height(14)
              .borderRadius(4)
              .backgroundColor(seriesColor(0, this.dark))
            Text(formatNumber(item.value) + ' ' + this.unit)
              .fontSize(12)
              .fontWeight(FontWeight.Medium)
              .fontColor($r('sys.color.ohos_id_color_text_primary'))
              .maxLines(1)
          }
          .width('100%')
        }
        .alignItems(HorizontalAlign.Start)
        .width('100%')
      }, (item: BarItem, index: number) => index.toString() + item.label + item.value.toString())
    }
    .width('100%')
  }
}
