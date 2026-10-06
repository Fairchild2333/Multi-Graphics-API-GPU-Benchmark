// GPU page: the tests the HarmonyOS host can start and the result of the last
// run with its chart. A test with a picture runs full screen (GpuFullscreen);
// the memory sweep computes only and runs through the command-line entry.

import { AdaptivePanes } from '../components/AdaptivePanes';
import { Card } from '../components/Card';
import { ChartSeries, LineChart } from '../components/Charts';
import { gpuRun, MemoryLevelLine } from '../common/EngineOutput';
import { formatNumber } from '../common/Format';
import { localizeScoreLine, tr } from '../common/I18n';
import { appState, runController } from '../common/RunController';

class GpuWorkload {
  id: string;
  nameEn: string;
  nameZh: string;
  version: string;

  constructor(id: string, nameEn: string, nameZh: string, version: string) {
    this.id = id;
    this.nameEn = nameEn;
    this.nameZh = nameZh;
    this.version = version;
  }
}

// The versions are the ones the engine writes on HarmonyOS; see AppBase::CollectResult.
const GPU_WORKLOADS: GpuWorkload[] = [
  new GpuWorkload('stream', 'Stream / Particle', '粒子 —— 内存吞吐', 'stream_v1_ohos_preview'),
  new GpuWorkload('stream_heavy', 'Stream / Particle (1M)', '粒子 —— 内存吞吐（100 万）', 'stream_v1_ohos_preview'),
  new GpuWorkload('gpu_burn', 'GPU Burn (light 16)', 'GPU Burn（轻量 16 步）',
    'gpu_burn_v3_fixed_steps_16_kaleidoscope_ohos_preview'),
  new GpuWorkload('gpu_burn_medium', 'GPU Burn (medium 64)', 'GPU Burn（中等 64 步）',
    'gpu_burn_v3_fixed_steps_64_kaleidoscope_ohos_preview'),
  new GpuWorkload('gpu_burn_heavy', 'GPU Burn (heavy 256)', 'GPU Burn（重度 256 步）',
    'gpu_burn_v3_fixed_steps_256_kaleidoscope_ohos_preview'),
  new GpuWorkload('cinematic_liquid', 'Fluid — interactive pool', '流体 —— 互动水池',
    'cinematic_liquid_v2_physical_scene_v8_preview_ohos_preview'),
  new GpuWorkload('membw', 'Memory bandwidth — VRAM / cache', '显存带宽 —— 显存 / 缓存', 'membw_v1_ohos_preview'),
];

@Extend(Text)
function caption() {
  .fontSize(12)
  .fontColor($r('sys.color.ohos_id_color_text_secondary'))
}

@ComponentV2
export struct GpuPage {
  /** GPU time per frame over the measured part of the run. */
  /** The memory sweep: GB/s over the size of the data set, one line per kernel. */
  private memorySeries(): ChartSeries[] {
    const read: ChartSeries = new ChartSeries(tr('Read', '读'), 0);
    const write: ChartSeries = new ChartSeries(tr('Write', '写'), 1);
    const copy: ChartSeries = new ChartSeries(tr('Copy', '复制'), 2);
    for (const point of gpuRun.memoryPoints) {
      if (point.kernel === 'read') {
        read.add(point.bytes, point.gbps);
      } else if (point.kernel === 'write') {
        write.add(point.bytes, point.gbps);
      } else if (point.kernel === 'copy') {
        copy.add(point.bytes, point.gbps);
      }
    }
    const series: ChartSeries[] = [];
    for (const item of [read, write, copy]) {
      if (item.xs.length >= 2) {
        series.push(item);
      }
    }
    return series;
  }

  private memoryLevelText(level: MemoryLevelLine): string {
    const name: string = level.kernel === 'read' ? tr('Read', '读')
      : level.kernel === 'write' ? tr('Write', '写') : level.kernel === 'copy' ? tr('Copy', '复制') : level.kernel;
    return tr(`${name}: cache ${formatNumber(level.cacheGbps)} GB/s, memory ${formatNumber(level.memoryGbps)} GB/s`,
      `${name}：缓存 ${formatNumber(level.cacheGbps)} GB/s，显存 ${formatNumber(level.memoryGbps)} GB/s`);
  }

  private timingSeries(): ChartSeries[] {
    const compute: ChartSeries = new ChartSeries(tr('Compute', '计算'), 0);
    const render: ChartSeries = new ChartSeries(tr('Render', '渲染'), 1);
    let computeMax: number = 0;
    let renderMax: number = 0;
    for (const sample of gpuRun.samples) {
      if (sample.warmup) {
        continue;
      }
      compute.add(sample.seconds, sample.computeMs);
      render.add(sample.seconds, sample.renderMs);
      computeMax = Math.max(computeMax, sample.computeMs);
      renderMax = Math.max(renderMax, sample.renderMs);
    }
    // A pass that takes no measurable time is left out.
    const series: ChartSeries[] = [];
    if (compute.xs.length >= 2 && computeMax > 0.0005) {
      series.push(compute);
    }
    if (render.xs.length >= 2 && renderMax > 0.0005) {
      series.push(render);
    }
    return series;
  }

  private statusText(): string {
    if (appState.gpuStatus.length > 0) {
      return appState.gpuStatus;
    }
    if (!appState.engineReady) {
      return tr('The native engine could not be loaded.', '无法加载原生引擎。');
    }
    return tr('Preview run: 3 s of Particle or GPU Burn. Scores are ohos_preview; do not mix them with desktop scores.',
      '预览运行：粒子测试或 GPU Burn 跑 3 秒。成绩属于 ohos_preview，不要和桌面版的成绩放在一起比较。');
  }

  /** The preview and the button that starts a run. */
  @Builder
  controls() {
    Text(tr('GPU Benchmark', 'GPU 跑分'))
      .fontSize(24)
      .fontWeight(FontWeight.Bold)
      .fontColor($r('sys.color.ohos_id_color_text_primary'))

    Card({ title: tr('Runs full screen', '全屏运行') }) {
      Text(tr('A test takes over the screen in landscape. It always renders 1280 × 720, as the desktop version ' +
        'does, and the system scales the picture to the screen, so a score does not depend on the size or ' +
        'resolution of the display.',
        '测试开始后横屏占满屏幕。渲染尺寸固定为 1280 × 720，和桌面版相同，再由系统缩放到屏幕，所以成绩不随屏幕大小和分辨率变化。'))
        .caption()
      Text(tr('Engine: ', '引擎：') + appState.engineVersion)
        .caption()
    }

    Row({ space: 12 }) {
      Button(appState.gpuRunning ? tr('Stop', '停止') : tr('Run', '运行'))
        .enabled(appState.engineReady)
        .onClick(() => {
          if (appState.gpuRunning) {
            runController.stopGpu();
          } else {
            runController.runGpu(appState.gpuWorkload, 3);
          }
        })
      Text(this.statusText())
        .caption()
        .layoutWeight(1)
    }
    .width('100%')
  }

  /** The tests to choose from and the result of the last run. */
  @Builder
  results() {
    Card({ title: tr('Workloads', '测试项目') }) {
      Text(tr('Results carry the _ohos_preview suffix (a 3 s run inside the app, not the 15 s of the desktop version).',
        '成绩的版本带 _ohos_preview 后缀（App 内运行 3 秒，不是桌面版的 15 秒）。'))
        .caption()
      ForEach(GPU_WORKLOADS, (workload: GpuWorkload) => {
        Column({ space: 2 }) {
          Text(tr(workload.nameEn, workload.nameZh))
            .fontSize(15)
            .fontWeight(FontWeight.Medium)
            .fontColor($r('sys.color.ohos_id_color_text_primary'))
          Text(workload.version)
            .caption()
        }
        .alignItems(HorizontalAlign.Start)
        .width('100%')
        .padding(12)
        .borderRadius(12)
        .backgroundColor(appState.gpuWorkload === workload.id
          ? $r('sys.color.ohos_id_color_component_activated')
          : $r('sys.color.ohos_id_color_background'))
        .onClick(() => {
          if (!appState.gpuRunning) {
            appState.gpuWorkload = workload.id;
          }
        })
      }, (workload: GpuWorkload) => workload.id)
    }

    if (gpuRun.scoreLine.length > 0 || gpuRun.fpsLine.length > 0 || gpuRun.samples.length >= 2 ||
      gpuRun.memoryPoints.length > 0) {
      Card({ title: tr('Last run', '最近一次运行') }) {
        if (gpuRun.scoreLine.length > 0) {
          Text(localizeScoreLine(gpuRun.scoreLine))
            .fontSize(16)
            .fontWeight(FontWeight.Medium)
            .fontColor($r('sys.color.ohos_id_color_text_primary'))
        }
        if (gpuRun.fpsLine.length > 0) {
          Text(localizeScoreLine(gpuRun.fpsLine))
            .fontSize(14)
            .fontColor($r('sys.color.ohos_id_color_text_primary'))
        }
        if (gpuRun.displayLimited) {
          Text(tr('The frame rate was held at the refresh rate of the display, so the GPU was not kept busy and ' +
            'this score may be too low. Run a heavier test.',
            '帧率被屏幕刷新率限制住了，GPU 没有跑满，这个成绩可能偏低、不准确。请换负载更高的测试。'))
            .fontSize(12)
            .fontColor($r('sys.color.ohos_id_color_warning'))
        }
        if (gpuRun.memoryPoints.length > 0) {
          ForEach(gpuRun.memoryLevels, (level: MemoryLevelLine) => {
            Text(this.memoryLevelText(level))
              .fontSize(14)
              .fontColor($r('sys.color.ohos_id_color_text_primary'))
          }, (level: MemoryLevelLine) => level.kernel)
          Text(tr('GB/s over the size of the data set', 'GB/s 随数据量变化'))
            .caption()
          LineChart({ series: this.memorySeries(), logBytes: true, dark: appState.dark })
        }
        if (this.timingSeries().length > 0) {
          Text(tr('GPU time per frame (ms) over the run, after warm-up',
            '每帧 GPU 耗时（ms）随运行时间变化，不含预热'))
            .caption()
          LineChart({ series: this.timingSeries(), logBytes: false, dark: appState.dark })
        }
      }
    }
  }

  build() {
    AdaptivePanes({
      primary: () => {
        this.controls()
      },
      secondary: () => {
        this.results()
      }
    })
  }
}
