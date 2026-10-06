// GPU page: the tests the HarmonyOS host can start, the size and duration of
// the selected one (as in the desktop app) and the result of the last run with
// its chart. A test with a picture runs full screen (GpuFullscreen); the memory
// sweep computes only and runs through the command-line entry.

import { deviceInfo } from '@kit.BasicServicesKit';
import { AdaptivePanes } from '../components/AdaptivePanes';
import { Card } from '../components/Card';
import { PresetRow } from '../components/PresetRow';
import { ChartSeries, LineChart } from '../components/Charts';
import { gpuRun, MemoryLevelLine } from '../common/EngineOutput';
import { formatNumber } from '../common/Format';
import {
  DEFAULT_SECONDS, DEFAULT_STEPS, GPU_WORKLOADS, gpuRequest, gpuSavedAs, GpuTest, PARTICLE_LIMITS,
  PARTICLE_PRESETS, resolveChoice, SECOND_LIMITS, SECOND_PRESETS, STEP_LIMITS, STEP_PRESETS
} from '../common/GpuOptions';
import { localizeScoreLine, tr } from '../common/I18n';
import { appState, runController } from '../common/RunController';

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

  // The chosen size and duration; -1 while a typed number is outside its range.
  private particles(): number {
    return resolveChoice(appState.gpuParticleChoice, appState.gpuCustomParticles, PARTICLE_LIMITS);
  }

  private steps(): number {
    return resolveChoice(appState.gpuStepChoice, appState.gpuCustomSteps, STEP_LIMITS);
  }

  private seconds(): number {
    return resolveChoice(appState.gpuSecondChoice, appState.gpuCustomSeconds, SECOND_LIMITS);
  }

  private optionsValid(): boolean {
    if (appState.gpuWorkload === 'membw') {
      return true;
    }
    if (this.seconds() < 0) {
      return false;
    }
    if (appState.gpuWorkload === 'stream') {
      return this.particles() > 0;
    }
    if (appState.gpuWorkload === 'gpu_burn') {
      return this.steps() > 0;
    }
    return true;
  }

  /** A two-in-one shows the picture in the window of the app, as Windows and macOS do. */
  private windowed(): boolean {
    return deviceInfo.deviceType === '2in1';
  }

  private statusText(): string {
    // A custom value outside its range is why "Run" is off; that comes before
    // what the last run left here.
    if (!appState.gpuRunning && appState.engineReady && !this.optionsValid()) {
      return tr('The custom value is not in its range.', '自定义的数值不在允许的范围内。');
    }
    if (appState.gpuStatus.length > 0) {
      return appState.gpuStatus;
    }
    if (!appState.engineReady) {
      return tr('The native engine could not be loaded.', '无法加载原生引擎。');
    }
    return tr('Scores are ohos_preview; do not mix them with desktop scores.',
      '成绩属于 ohos_preview，不要和桌面版的成绩放在一起比较。');
  }

  /** The preview and the button that starts a run. */
  @Builder
  controls() {
    Text(tr('GPU Benchmark', 'GPU 跑分'))
      .fontSize(24)
      .fontWeight(FontWeight.Bold)
      .fontColor($r('sys.color.ohos_id_color_text_primary'))

    Card({ title: this.windowed() ? tr('Runs in this window', '在窗口内运行') : tr('Runs full screen', '全屏运行') }) {
      Text(this.windowed()
        ? tr('The picture of a test is shown in this window, as on Windows and macOS. It always renders ' +
          '1280 × 720 and is scaled to the window, so a score does not depend on the size of the window.',
          '测试画面显示在这个窗口里，和 Windows、macOS 版一样不强制全屏。渲染尺寸固定为 1280 × 720，再缩放到窗口，' +
            '所以成绩不随窗口大小变化。')
        : tr('A test takes over the screen in landscape. It always renders 1280 × 720, as the desktop version ' +
          'does, and the system scales the picture to the screen, so a score does not depend on the size or ' +
          'resolution of the display.',
          '测试开始后横屏占满屏幕。渲染尺寸固定为 1280 × 720，和桌面版相同，再由系统缩放到屏幕，所以成绩不随屏幕大小和分辨率变化。'))
        .caption()
      Text(tr('Engine: ', '引擎：') + appState.engineVersion)
        .caption()
    }

    Row({ space: 12 }) {
      Button(appState.gpuRunning ? tr('Stop', '停止') : tr('Run', '运行'))
        .enabled(appState.engineReady && (appState.gpuRunning || this.optionsValid()))
        .onClick(() => {
          if (appState.gpuRunning) {
            runController.stopGpu();
          } else {
            runController.runGpu(gpuRequest(appState.gpuWorkload, this.particles(), this.steps()), this.seconds());
          }
        })
      Text(this.statusText())
        .caption()
        .layoutWeight(1)
    }
    .width('100%')
  }

  /** The tests to choose from. */
  @Builder
  tests() {
    Card({ title: tr('Workloads', '测试项目') }) {
      Text(tr('A result is saved under the name shown below the test.', '成绩按测试名下面那一行的名字保存。'))
        .caption()
      ForEach(GPU_WORKLOADS, (workload: GpuTest) => {
        Column({ space: 2 }) {
          Text(tr(workload.nameEn, workload.nameZh))
            .fontSize(15)
            .fontWeight(FontWeight.Medium)
            .fontColor(appState.gpuWorkload === workload.id
              ? $r('sys.color.font_on_primary')
              : $r('sys.color.ohos_id_color_text_primary'))
          Text(gpuSavedAs(workload.id, this.steps() > 0 ? this.steps() : DEFAULT_STEPS,
            this.seconds() > 0 ? this.seconds() : DEFAULT_SECONDS))
            .fontSize(12)
            .fontColor(appState.gpuWorkload === workload.id
              ? $r('sys.color.font_on_secondary')
              : $r('sys.color.ohos_id_color_text_secondary'))
        }
        .alignItems(HorizontalAlign.Start)
        .width('100%')
        .padding(12)
        .borderRadius(12)
        .backgroundColor(appState.gpuWorkload === workload.id
          ? $r('sys.color.comp_background_emphasize')
          : $r('sys.color.ohos_id_color_background'))
        .onClick(() => {
          if (!appState.gpuRunning) {
            appState.gpuWorkload = workload.id;
          }
        })
      }, (workload: GpuTest) => workload.id)
    }

    // The size of the selected test and its duration; the memory sweep has neither.
    if (appState.gpuWorkload !== 'membw') {
      Card({ title: tr('Settings', '测试设置') }) {
        if (appState.gpuWorkload === 'stream') {
          PresetRow({
            title: tr('Particles', '粒子数'),
            presets: PARTICLE_PRESETS,
            choice: appState.gpuParticleChoice,
            custom: appState.gpuCustomParticles,
            hint: tr('multiple of 256', '256 的倍数'),
            valid: this.particles() > 0,
            active: !appState.gpuRunning,
            onChoice: (value: number) => {
              appState.gpuParticleChoice = value;
            },
            onCustom: (text: string) => {
              appState.gpuCustomParticles = text;
            }
          })
        }
        if (appState.gpuWorkload === 'gpu_burn') {
          PresetRow({
            title: tr('GPU Burn steps', 'GPU Burn 步数'),
            presets: STEP_PRESETS,
            choice: appState.gpuStepChoice,
            custom: appState.gpuCustomSteps,
            hint: '16–2048',
            valid: this.steps() > 0,
            active: !appState.gpuRunning,
            onChoice: (value: number) => {
              appState.gpuStepChoice = value;
            },
            onCustom: (text: string) => {
              appState.gpuCustomSteps = text;
            }
          })
        }
        PresetRow({
          title: tr('Duration', '运行时长'),
          presets: SECOND_PRESETS,
          choice: appState.gpuSecondChoice,
          custom: appState.gpuCustomSeconds,
          hint: tr('seconds, 1–3600', '秒，1–3600'),
          valid: this.seconds() > 0,
          active: !appState.gpuRunning,
          onChoice: (value: number) => {
            appState.gpuSecondChoice = value;
          },
          onCustom: (text: string) => {
            appState.gpuCustomSeconds = text;
          }
        })
      }
    }
  }

  /** The result of the last run, with its chart. */
  @Builder
  lastRun() {
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
        this.tests()
      },
      result: () => {
        this.lastRun()
      }
    })
  }
}
