// Starts tests in the native engine, collects what it prints and keeps the
// state the pages show. One controller for the whole app, driven by a timer in
// Index.ets.

import bridge from 'libmangekyo.so';
import { common } from '@kit.AbilityKit';
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

export const GPU_TESTS: string[] = ['stream', 'gpu_burn'];

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
  filesDir: string = '';
  debuggable: boolean = false;
}

export const appState: AppState = new AppState();

class PendingRequest {
  test: string;
  seconds: number;
  attempts: number = 0;

  constructor(test: string, seconds: number) {
    this.test = test;
    this.seconds = seconds;
  }
}

export class RunController {
  /** The page that started the current run: 'gpu', 'cpu' or ''. */
  private owner: string = '';
  private pending: PendingRequest | undefined = undefined;

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
    this.pending = new PendingRequest(test, seconds);
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

  stopGpu(): void {
    bridge.stopWorkload();
    appState.gpuStatus = tr('Stop requested', '已请求停止');
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
      const running: boolean = bridge.isRunning();
      appState.gpuRunning = running;
      if (gpuRun.active && !running) {
        gpuRun.consumeAll(bridge.pollOutput());
        gpuRun.active = false;
        appState.resultsRevision++;
        const error: string = bridge.lastError();
        appState.gpuStatus = error.length > 0 ? error : tr('Done', '完成');
        logLine('RUN finished kind=gpu score=' + (gpuRun.scoreLine.length > 0 ? gpuRun.scoreLine : 'none') +
          (error.length > 0 ? ' error=' + error : ''));
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
    wanted.attempts++;
    if (bridge.hasSurface() && this.startGpu(wanted.test, wanted.seconds > 0 ? wanted.seconds : 3)) {
      this.pending = undefined;
      logLine(`AUTORUN started test=${wanted.test}`);
    } else if (wanted.attempts > 50) {
      this.pending = undefined;
      logLine(`AUTORUN failed test=${wanted.test} reason=${bridge.lastError()}`);
    }
  }
}

export const runController: RunController = new RunController();
