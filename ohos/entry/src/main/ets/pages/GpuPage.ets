// GPU page: the tests the HarmonyOS host can start, the size and duration of
// the selected one (as in the desktop app) and the result of the last run with
// its chart. A test with a picture runs full screen (GpuFullscreen); the memory
// sweep computes only and runs through the command-line entry.

import { LengthMetrics } from '@kit.ArkUI';
import { deviceInfo } from '@kit.BasicServicesKit';
import { AdaptivePanes } from '../components/AdaptivePanes';
import { Card } from '../components/Card';
import { PresetRow } from '../components/PresetRow';
import { ChartSeries, LineChart } from '../components/Charts';
import { cpuRun, gpuRun, MemoryLevelLine } from '../common/EngineOutput';
import { formatNumber } from '../common/Format';
import {
  API_GLES, API_GLES2, API_VULKAN, DEFAULT_SECONDS, DEFAULT_STEPS, GPU_WORKLOADS, gpuRequest, gpuSavedAs,
  GpuTest, gpuTestsOf, PARTICLE_LIMITS,
  PARTICLE_PRESETS, resolveChoice, SECOND_LIMITS, SECOND_PRESETS, STEP_LIMITS, STEP_PRESETS
} from '../common/GpuOptions';
import { localizeScoreLine, tr } from '../common/I18n';
import { appState, runController } from '../common/RunController';
import { suiteRun } from '../common/MobileSuite';

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

  /** OpenGL ES 3.1 has two of the tests, OpenGL ES 2.0 a GPU Burn of its own. */
  private testOnApi(id: string): boolean {
    return gpuTestsOf(appState.gpuApi).indexOf(id) >= 0;
  }

  /** What a test that the selected API lacks needs instead. */
  private neededApi(id: string): string {
    return gpuTestsOf(API_GLES).indexOf(id) >= 0 ? tr('Vulkan or OpenGL ES 3.1', '需要 Vulkan 或 OpenGL ES 3.1')
      : tr('Vulkan only', '仅 Vulkan');
  }

  @Builder
  apiButton(label: string, api: string) {
    Button(label)
      .fontSize(13)
      .height(32)
      .fontColor(appState.gpuApi === api ? $r('sys.color.font_on_primary')
        : $r('sys.color.ohos_id_color_text_primary'))
      .backgroundColor(appState.gpuApi === api ? $r('sys.color.comp_background_emphasize')
        : $r('sys.color.ohos_id_color_button_normal'))
      .enabled(!appState.gpuRunning)
      .onClick(() => {
        appState.gpuApi = api;
      })
  }

  /** A two-in-one shows the picture in the window of the app, as Windows and macOS do. */
  private windowed(): boolean {
    return deviceInfo.deviceType === '2in1';
  }

  private statusText(): string {
    // A custom value outside its range is why "Run" is off; that comes before
    // what the last run left here.
    if (!appState.gpuRunning && appState.engineReady && !this.testOnApi(appState.gpuWorkload)) {
      return tr('This test is not available under the selected API. OpenGL ES 3.1 has the particle test and ' +
        'GPU Burn; OpenGL ES 2.0 has a GPU Burn of its own.',
        '所选的图形 API 下没有这项测试。OpenGL ES 3.1 有粒子测试和 GPU Burn；OpenGL ES 2.0 只有它自己的 GPU Burn。');
    }
    if (!appState.gpuRunning && appState.engineReady && !this.optionsValid()) {
      return tr('The custom value is not in its range.', '自定义的数值不在允许的范围内。');
    }
    if (appState.gpuStatus.length > 0) {
      return appState.gpuStatus;
    }
    if (!appState.engineReady) {
      return tr('The native engine could not be loaded.', '无法加载原生引擎。');
    }
    return '';
  }

  /** The preview and the button that starts a run. */
  @Builder
  controls() {
    Text(tr('GPU Benchmark', 'GPU 测试'))
      .fontSize(24)
      .fontWeight(FontWeight.Bold)
      .fontColor($r('sys.color.ohos_id_color_text_primary'))

    Card({ title: this.windowed() ? tr('Runs in this window', '在窗口内运行') : tr('Runs full screen', '全屏运行') }) {
      Text(this.windowed()
        ? tr('The picture of a test is shown in this window, as on Windows and macOS. It always renders ' +
          '1280 × 720.',
          '测试画面显示在这个窗口里，和 Windows、macOS 版一样不强制全屏。渲染尺寸固定为 1280 × 720。')
        : tr('A test takes over the screen in landscape. It always renders 1280 × 720.',
          '测试开始后横屏占满屏幕。渲染尺寸固定为 1280 × 720。'))
        .caption()
      Text(tr('Engine: ', '引擎：') + appState.engineVersion)
        .caption()
    }

    Card({ title: tr('API', '图形 API') }) {
      Flex({ wrap: FlexWrap.Wrap, space: { main: LengthMetrics.vp(8), cross: LengthMetrics.vp(8) } }) {
        this.apiButton('Vulkan', API_VULKAN)
        this.apiButton('OpenGL ES 3.1', API_GLES)
        this.apiButton('OpenGL ES 2.0', API_GLES2)
      }
      Text(tr('OpenGL ES 3.1 has the particle test and GPU Burn. OpenGL ES 2.0 has a GPU Burn of its own, ' +
        'saved in its own group. The other tests need Vulkan.',
        'OpenGL ES 3.1 有粒子测试和 GPU Burn。OpenGL ES 2.0 只有它自己的 GPU Burn，成绩单独成组。其余测试需要 Vulkan。'))
        .caption()
    }

    Column({ space: 12 }) {
      Button(tr('Run all GPU tests', '一键测试 GPU 全套'))
        .width('100%')
        .enabled(appState.engineReady && !appState.gpuRunning && !cpuRun.running && !suiteRun.running)
        .onClick(() => { runController.startSuite('gpu'); })
      Button(tr('Test all items', '一键测试全部项目'))
        .width('100%')
        .enabled(appState.engineReady && !appState.gpuRunning && !cpuRun.running && !suiteRun.running)
        .onClick(() => { runController.startSuite('all'); })
      if (suiteRun.status.length > 0) { Text(suiteRun.status).caption() }
      if (suiteRun.running) {
        Text(suiteRun.currentLabel).caption()
        Button(tr('Stop suite', '停止全套测试')).onClick(() => { runController.stopSuite(); })
      }
      Button(appState.gpuRunning ? tr('Stop', '停止') : tr('Run', '运行'))
        .enabled(appState.engineReady &&
          (appState.gpuRunning || (this.optionsValid() && this.testOnApi(appState.gpuWorkload))))
        .onClick(() => {
          if (appState.gpuRunning) {
            runController.stopGpu();
          } else {
            runController.runGpu(gpuRequest(appState.gpuWorkload, this.particles(), this.steps(),
              appState.gpuHeadless, appState.gpuApi), this.seconds());
          }
        })
      Text(this.statusText())
        .caption()
    }
    .width('100%')
    .alignItems(HorizontalAlign.Start)
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
            this.seconds() > 0 ? this.seconds() : DEFAULT_SECONDS, appState.gpuApi,
            appState.esParticleWorkGroupSize))
            .fontSize(12)
            .fontColor(appState.gpuWorkload === workload.id
              ? $r('sys.color.font_on_secondary')
              : $r('sys.color.ohos_id_color_text_secondary'))
          Text(this.neededApi(workload.id))
            .fontSize(12)
            .fontColor(appState.gpuWorkload === workload.id
              ? $r('sys.color.font_on_primary')
              : $r('sys.color.ohos_id_color_warning'))
            .visibility(this.testOnApi(workload.id) ? Visibility.None : Visibility.Visible)
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
          Button('Headless')
            .fontSize(13)
            .height(32)
            .fontColor(appState.gpuHeadless ? $r('sys.color.font_on_primary')
              : $r('sys.color.ohos_id_color_text_primary'))
            .backgroundColor(appState.gpuHeadless ? $r('sys.color.comp_background_emphasize')
              : $r('sys.color.ohos_id_color_button_normal'))
            .enabled(!appState.gpuRunning)
            .onClick(() => {
              appState.gpuHeadless = !appState.gpuHeadless;
            })
          Text(tr('Headless computes only: nothing is drawn and no picture is shown.',
            'Headless 只做计算：不渲染，也不显示画面。'))
            .caption()
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
        if (gpuRun.workGroupSize > 0) {
          Text(tr(`This device's OpenGL ES driver allows compute work groups of ${gpuRun.workGroupSize} only, so ` +
            `the particle test ran in groups of ${gpuRun.workGroupSize} instead of 256. The result is saved in a ` +
            `group of its own (its name has _wg${gpuRun.workGroupSize}) and is not one of the results in groups ` +
            'of 256: on a GPU that runs both it read the same at 65,536 particles and 2-3% higher at a million ' +
            'and more.',
            `这台设备的 OpenGL ES 驱动只支持 ${gpuRun.workGroupSize} 线程的工作组，粒子测试改用 ` +
            `${gpuRun.workGroupSize} 分组运行（标准是 256）。成绩单独保存为一组（名字带 _wg${gpuRun.workGroupSize}），` +
            '不和 256 分组的成绩混在一起：在两种都能跑的 GPU 上，65,536 粒子时两者相同，100 万以上时它高 2–3%。'))
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
