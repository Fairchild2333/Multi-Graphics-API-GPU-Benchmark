// Starts tests in the native engine, collects what it prints and keeps the
// state the pages show. One controller for the whole app, driven by a timer in
// Index.ets.

import bridge from 'libmangekyo.so';
import { common } from '@kit.AbilityKit';
import { display, window } from '@kit.ArkUI';
import { deviceInfo } from '@kit.BasicServicesKit';
import { fileIo as fs } from '@kit.CoreFileKit';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { cpuRun, gpuRun } from './EngineOutput';
import { tr } from './I18n';

const LOG_DOMAIN: number = 0x0000;
const LOG_TAG: string = 'Mangekyo';

/** One line in the system log (`hdc hilog | grep Mangekyo`) for a script to wait on. */
export function logLine(message: string): void {
  hilog.info(LOG_DOMAIN, LOG_TAG, '%{public}s', message);
}

/** CPU tests that can be requested on the command line, and the mode each one selects. */
export const CPU_TEST_MODES: Map<string, string> = new Map<string, string>([
  ['cpu_membw', 'memory'],
  ['cpu_all', 'all'],
  ['cpu_per_core', 'per-core'],
  ['cpu_multi', 'multi'],
]);

export const GPU_TESTS: string[] =
  ['stream', 'stream_heavy', 'gpu_burn', 'gpu_burn_medium', 'gpu_burn_heavy', 'cinematic_liquid', 'membw'];

/** GPU tests without a picture; they run through the engine's command-line entry. */
export const GPU_COMMAND_LINE_TESTS: string[] = ['membw'];

@ObservedV2
export class AppState {
  /** True in dark mode; the charts pick their colours by it. */
  @Trace dark: boolean = false;
  /** False until the shaders are in place and the engine knows its folders. */
  @Trace engineReady: boolean = false;
  @Trace engineVersion: string = '';
  @Trace gpuRunning: boolean = false;
  @Trace gpuStatus: string = '';
  @Trace gpuWorkload: string = 'stream';
  @Trace cpuMode: string = 'all';
  /** The tab a command-line request wants to be shown, or -1. */
  @Trace requestedTab: number = -1;
  /** Grows whenever a run may have saved a result. */
  @Trace resultsRevision: number = 0;
  /**
   * Width of the app's window in vp, kept up to date by the shell. It is the
   * window that counts, not the device: a foldable that is opened, a tablet in
   * split screen and a freely resized window all change it while the app runs.
   */
  @Trace windowWidth: number = 360;
  @Trace windowHeight: number = 780;
  /** A GPU test has the whole screen: the shell shows the picture alone over its pages. */
  @Trace gpuFullscreen: boolean = false;
  /** True when the tab bar floats over the pages (the shell for HarmonyOS 6.1 and later). */
  @Trace floatingTabs: boolean = false;
  filesDir: string = '';
  debuggable: boolean = false;

  /** From 600 vp the tabs stand at the side instead of at the bottom. */
  get sideTabs(): boolean {
    return this.windowWidth >= 600;
  }

  /** From 840 vp a page shows its two halves beside each other. */
  get twoPane(): boolean {
    return this.windowWidth >= 840;
  }

  /** The margin between the content of a page and the edge of the window. */
  get margin(): number {
    return this.windowWidth >= 600 ? 24 : 16;
  }

  /** Room the pages leave at their end for a tab bar that floats over them. */
  get bottomInset(): number {
    return this.floatingTabs && !this.sideTabs ? 96 : 0;
  }
}

export const appState: AppState = new AppState();

class PendingRequest {
  test: string;
  seconds: number;
  /** True for a test named on the command line; its start and failure are logged for a script. */
  fromCommandLine: boolean;
  attempts: number = 0;

  constructor(test: string, seconds: number, fromCommandLine: boolean) {
    this.test = test;
    this.seconds = seconds;
    this.fromCommandLine = fromCommandLine;
  }
}

export class RunController {
  /** The page that started the current run: 'gpu', 'cpu' or ''. */
  private owner: string = '';
  private pending: PendingRequest | undefined = undefined;
  /** True while a GPU test without a picture runs through the command-line entry. */
  private gpuCommandLine: boolean = false;
  /** The window of the app, set by EntryAbility; a GPU test turns it to landscape and hides the system bars. */
  mainWindow: window.Window | undefined = undefined;

  /**
   * Copies the SPIR-V shaders out of the app package, because the engine
   * reads them as files, and tells the engine where they and the results are.
   */
  prepare(context: common.UIAbilityContext): void {
    const filesDir: string = context.filesDir;
    const shaderDir: string = filesDir + '/shaders';
    appState.filesDir = filesDir;
    try {
      if (!fs.accessSync(shaderDir)) {
        fs.mkdirSync(shaderDir);
      }
      const names: string[] = context.resourceManager.getRawFileListSync('shaders');
      for (const name of names) {
        const data: Uint8Array = context.resourceManager.getRawFileContentSync('shaders/' + name);
        const file: fs.File = fs.openSync(shaderDir + '/' + name,
          fs.OpenMode.CREATE | fs.OpenMode.READ_WRITE | fs.OpenMode.TRUNC);
        fs.writeSync(file.fd, data.buffer.slice(data.byteOffset, data.byteOffset + data.byteLength));
        fs.closeSync(file);
      }
      logLine(`shaders copied: ${names.length}`);
    } catch (e) {
      // Without shaders the GPU tests fail with a clear engine error; the CPU tests still run.
      logLine('shader copy failed: ' + JSON.stringify(e));
    }
    try {
      bridge.initPaths(shaderDir, filesDir);
      appState.engineVersion = bridge.engineVersion();
      appState.engineReady = true;
    } catch (e) {
      logLine('engine not available: ' + JSON.stringify(e));
    }
  }

  /**
   * A test named on the command line:
   * hdc shell aa start -a EntryAbility -b com.mangekyo.benchmark --ps run cpu_membw
   * Debug builds only: in a release build any other app could start a run this way.
   */
  request(test: string, seconds: number): void {
    if (!appState.debuggable) {
      logLine(`AUTORUN refused test=${test} reason=release_build`);
      return;
    }
    if (!CPU_TEST_MODES.has(test) && GPU_TESTS.indexOf(test) < 0) {
      logLine(`AUTORUN refused test=${test} reason=unknown_test`);
      return;
    }
    this.pending = new PendingRequest(test, seconds, true);
    appState.requestedTab = CPU_TEST_MODES.has(test) ? 1 : 0;
    logLine(`AUTORUN accepted test=${test}`);
  }

  get cpuBusy(): boolean {
    return cpuRun.running;
  }

  startCpu(mode: string, seconds: number): void {
    if (!appState.engineReady || cpuRun.running || appState.gpuRunning) {
      return;
    }
    const memory: boolean = mode === 'memory';
    const args: string[] = memory
      ? ['--cpu-membw', '--cpu-membw-storage-dir', appState.filesDir]
      : ['--cpu-benchmark', mode, '--cpu-time', seconds.toString(), '--cpu-warmup', '0.2'];
    bridge.pollOutput();  // drop lines left over from an earlier run
    this.owner = 'cpu';
    cpuRun.begin(memory);
    if (!bridge.startCli(args)) {
      cpuRun.finish(-1);
      const error: string = bridge.lastError();
      cpuRun.status = error.length > 0 ? error : tr('Start failed', '启动失败');
    }
  }

  startGpu(workloadId: string, seconds: number): boolean {
    if (!appState.engineReady || cpuRun.running) {
      return false;
    }
    bridge.pollOutput();  // drop lines left over from an earlier run
    this.owner = 'gpu';
    gpuRun.begin();
    const ok: boolean = bridge.startWorkload(workloadId, seconds);
    if (ok) {
      appState.gpuStatus = tr(`Running ${workloadId} (${Math.round(seconds)} s preview)`,
        `正在运行 ${workloadId}（${Math.round(seconds)} 秒预览）`);
    } else {
      gpuRun.active = false;
      const error: string = bridge.lastError();
      appState.gpuStatus = error.length > 0 ? error : tr('Start failed', '启动失败');
    }
    appState.gpuRunning = bridge.isRunning();
    return ok;
  }

  /**
   * Starts a GPU test from the page. A test takes the whole screen: the shell
   * first shows the picture alone, and the test starts once that surface
   * exists (startPending), which takes a moment because the screen may turn.
   */
  runGpu(workloadId: string, seconds: number): void {
    if (!appState.engineReady || cpuRun.running || appState.gpuRunning || this.pending !== undefined) {
      return;
    }
    if (GPU_COMMAND_LINE_TESTS.indexOf(workloadId) >= 0) {
      this.startGpuCommandLine(workloadId);
      return;
    }
    this.pending = new PendingRequest(workloadId, seconds, false);
    this.enterFullscreen();
  }

  /** The VRAM / cache bandwidth sweep computes only: no picture, no full screen. */
  private startGpuCommandLine(workloadId: string): boolean {
    bridge.pollOutput();  // drop lines left over from an earlier run
    this.owner = 'gpu';
    gpuRun.begin();
    const ok: boolean = bridge.startCli(['--workload', workloadId, '--api', 'vulkan', '--gpu', '0']);
    this.gpuCommandLine = ok;
    if (ok) {
      appState.gpuStatus = tr(`Running ${workloadId}; this takes a minute or two`, `正在运行 ${workloadId}，需要一两分钟`);
    } else {
      gpuRun.active = false;
      const error: string = bridge.lastError();
      appState.gpuStatus = error.length > 0 ? error : tr('Start failed', '启动失败');
    }
    appState.gpuRunning = ok;
    return ok;
  }

  stopGpu(): void {
    if (this.gpuCommandLine) {
      return;   // the sweep has no stop; it ends by itself
    }
    bridge.stopWorkload();
    appState.gpuStatus = tr('Stop requested', '已请求停止');
    // Nothing runs yet: give the screen back at once.
    if (!bridge.isRunning()) {
      this.pending = undefined;
      this.leaveFullscreen();
    }
  }

  private enterFullscreen(): void {
    if (appState.gpuFullscreen) {
      return;
    }
    appState.gpuFullscreen = true;
    appState.gpuStatus = tr('Starting…', '正在启动…');
    this.applyWindowMode(true);
  }

  private leaveFullscreen(): void {
    if (!appState.gpuFullscreen) {
      return;
    }
    appState.gpuFullscreen = false;
    this.applyWindowMode(false);
  }

  /**
   * Gives the window to a test, or back: landscape, no system bars, screen
   * kept on. Only phones and tablets turn; a 2-in-1 keeps its window frame.
   * The SDK marks the turning and the full-screen layout as unavailable on
   * the other device types, so they are not called there.
   */
  private applyWindowMode(test: boolean): void {
    const win: window.Window | undefined = this.mainWindow;
    if (win === undefined) {
      return;
    }
    const type: string = deviceInfo.deviceType;
    const handheld: boolean = type === 'phone' || type === 'tablet';
    try {
      if (handheld) {
        win.setPreferredOrientation(test ? window.Orientation.AUTO_ROTATION_LANDSCAPE
          : window.Orientation.UNSPECIFIED).catch(() => {
        });
      }
      if (type !== '2in1') {
        win.setWindowLayoutFullScreen(test).catch(() => {
        });
        win.setWindowSystemBarEnable(test ? [] : ['status', 'navigation']).catch(() => {
        });
      }
      win.setWindowKeepScreenOn(test).catch(() => {
      });
    } catch (e) {
      logLine('window mode not applied: ' + JSON.stringify(e));
    }
  }

  /** Called a few times a second. */
  tick(): void {
    if (!appState.engineReady) {
      return;
    }
    const lines: string[] = bridge.pollOutput();
    if (this.owner === 'cpu') {
      cpuRun.consumeAll(lines);
      if (cpuRun.running && !bridge.isCliRunning()) {
        cpuRun.consumeAll(bridge.pollOutput());
        const exitCode: number = bridge.cliExitCode();
        cpuRun.finish(exitCode);
        appState.resultsRevision++;
        logLine(`RUN finished kind=cpu exit=${exitCode}`);
      }
    } else if (this.owner === 'gpu') {
      gpuRun.consumeAll(lines);
      const running: boolean = bridge.isRunning() || (this.gpuCommandLine && bridge.isCliRunning());
      appState.gpuRunning = running;
      if (gpuRun.active && !running) {
        this.gpuCommandLine = false;
        gpuRun.consumeAll(bridge.pollOutput());
        gpuRun.active = false;
        appState.resultsRevision++;
        const error: string = bridge.lastError();
        appState.gpuStatus = error.length > 0 ? error : tr('Done', '完成');
        logLine('RUN finished kind=gpu score=' + (gpuRun.scoreLine.length > 0 ? gpuRun.scoreLine : 'none') +
          (error.length > 0 ? ' error=' + error : ''));
        this.leaveFullscreen();
        // Besides the engine's own verdict: a frame rate that sits on the
        // refresh rate of the display was held there by it.
        try {
          const refresh: number = display.getDefaultDisplaySync().refreshRate;
          const fps: number = gpuRun.averageFps();
          if (isFinite(fps) && refresh > 1 && Math.abs(fps - refresh) < refresh * 0.04) {
            gpuRun.displayLimited = true;
          }
        } catch (e) {
          logLine('refresh rate not available: ' + JSON.stringify(e));
        }
      }
    }
    this.startPending();
  }

  private startPending(): void {
    const wanted: PendingRequest | undefined = this.pending;
    if (wanted === undefined) {
      return;
    }
    const cpuMode: string | undefined = CPU_TEST_MODES.get(wanted.test);
    if (cpuMode !== undefined) {
      this.pending = undefined;
      if (cpuRun.running || appState.gpuRunning) {
        logLine(`AUTORUN refused test=${wanted.test} reason=busy`);
        return;
      }
      appState.cpuMode = cpuMode;
      this.startCpu(cpuMode, wanted.seconds > 0 ? wanted.seconds : 1);
      logLine(`AUTORUN started test=${wanted.test}`);
      return;
    }
    // A GPU test needs the preview surface, which appears a moment after the page: wait up to 10 s.
    appState.gpuWorkload = wanted.test;
    if (GPU_COMMAND_LINE_TESTS.indexOf(wanted.test) >= 0) {
      this.pending = undefined;
      const started: boolean = this.startGpuCommandLine(wanted.test);
      if (wanted.fromCommandLine) {
        logLine(started ? `AUTORUN started test=${wanted.test}`
          : `AUTORUN failed test=${wanted.test} reason=${bridge.lastError()}`);
      }
      return;
    }
    this.enterFullscreen();
    wanted.attempts++;
    if (bridge.hasSurface() && this.startGpu(wanted.test, wanted.seconds > 0 ? wanted.seconds : 3)) {
      this.pending = undefined;
      if (wanted.fromCommandLine) {
        logLine(`AUTORUN started test=${wanted.test}`);
      }
    } else if (wanted.attempts > 50) {
      this.pending = undefined;
      this.leaveFullscreen();
      if (wanted.fromCommandLine) {
        logLine(`AUTORUN failed test=${wanted.test} reason=${bridge.lastError()}`);
      }
    }
  }
}

export const runController: RunController = new RunController();
