// GPU page: the preview surface the engine draws into, the two tests the
// HarmonyOS host can start, and the result of the last run with its chart.

import { AdaptivePanes } from '../components/AdaptivePanes';
import { Card } from '../components/Card';
import { ChartSeries, LineChart } from '../components/Charts';
import { gpuRun } from '../common/EngineOutput';
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
  new GpuWorkload('gpu_burn', 'GPU Burn (light 16)', 'GPU Burn（轻量 16 步）',
    'gpu_burn_v3_fixed_steps_16_kaleidoscope_ohos_preview'),
];

@Extend(Text)
function caption() {
  .fontSize(12)
  .fontColor($r('sys.color.ohos_id_color_text_secondary'))
}

@ComponentV2
export struct GpuPage {
  /** GPU time per frame over the measured part of the run. */
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

    Card({ title: tr('Preview surface', '预览画面') }) {
      XComponent({ id: 'mangekyo_surface', type: XComponentType.SURFACE, libraryname: 'mangekyo' })
        .width('100%')
        // A fixed shape, so the picture is the same on a phone, an opened
        // foldable and a tablet.
        .aspectRatio(16 / 9)
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
            runController.startGpu(appState.gpuWorkload, 3);
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

    if (gpuRun.scoreLine.length > 0 || gpuRun.fpsLine.length > 0 || gpuRun.samples.length >= 2) {
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
