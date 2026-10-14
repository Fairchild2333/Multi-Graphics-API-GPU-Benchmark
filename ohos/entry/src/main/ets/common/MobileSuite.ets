import bridge from 'libmangekyo.so';
import { fileIo as fs } from '@kit.CoreFileKit';
import { hilog } from '@kit.PerformanceAnalysisKit';
import { cpuRun, gpuRun } from './EngineOutput';
import { gpuCommandLine, gpuRequest, API_VULKAN } from './GpuOptions';
import { loadResults, ResultEntry, ResultJob } from './ResultsStore';
import { tr } from './I18n';

export interface SuiteCapabilities {
  vulkan: boolean; liquid: boolean; vulkanMaxParticles: number;
  gles31: boolean; glesMaxParticles: number; gles2: boolean;
}
export class SuiteJob implements ResultJob {
  id: string; api: string; workload: string; particles: number; steps: number; headless: boolean;
  constructor(id: string, api: string, workload: string, particles: number = 0, steps: number = 0, headless: boolean = false) {
    this.id = id; this.api = api; this.workload = workload; this.particles = particles; this.steps = steps; this.headless = headless;
  }
  cpu(): boolean { return this.api === 'cpu'; }
  request(): string { return gpuRequest(this.workload, this.particles, this.steps, this.headless, this.api); }
  label(): string {
    if (this.cpu()) return this.workload === 'cpu_all' ? tr('CPU compute — single + all-core', 'CPU 算力——单核与全核')
      : tr('CPU cache, memory and storage', 'CPU 缓存、内存与存储');
    const api: string = this.api === 'gles' ? 'OpenGL ES 3.1' : this.api === 'gles2' ? 'OpenGL ES 2.0' : 'Vulkan';
    const test: string = this.workload === 'stream' ? tr(`Particles ${this.particles}`, `粒子 ${this.particles}`) +
      (this.headless ? tr(' (compute only)', '（纯计算）') : '') : this.workload === 'gpu_burn' ? `GPU Burn ${this.steps}`
      : this.workload === 'membw' ? tr('GPU memory', 'GPU 内存') : tr('Fluid', '流体');
    return api + ' · ' + test;
  }
}
export function supportedSuiteJobs(c: SuiteCapabilities, scope: string): SuiteJob[] {
  const jobs: SuiteJob[] = [];
  if (scope !== 'cpu') for (const api of ['vulkan', 'gles', 'gles2']) {
    const available: boolean = api === 'vulkan' ? c.vulkan : api === 'gles' ? c.gles31 : c.gles2;
    if (!available) continue;
    const maximum: number = api === 'vulkan' ? c.vulkanMaxParticles : c.glesMaxParticles;
    if (api !== 'gles2') for (const headless of [false, true]) for (const count of [65536, 1048576, 4194304, 16777216]) {
      if (count <= maximum) jobs.push(new SuiteJob(`${api}-stream-${count}-${headless}`, api, 'stream', count, 0, headless));
    }
    for (const steps of [16, 64, 256]) jobs.push(new SuiteJob(`${api}-burn-${steps}`, api, 'gpu_burn', 0, steps));
    if (api === 'vulkan') {
      if (c.liquid) jobs.push(new SuiteJob('vulkan-liquid', api, 'cinematic_liquid'));
      jobs.push(new SuiteJob('vulkan-memory', api, 'membw', 0, 0, true));
    }
  }
  if (scope !== 'gpu') {
    jobs.push(new SuiteJob('cpu-compute', 'cpu', 'cpu_all', 0, 0, true));
    jobs.push(new SuiteJob('cpu-memory', 'cpu', 'cpu_membw', 0, 0, true));
  }
  return jobs;
}
interface SuiteAttempt { id: string; status: string; error: string; reviewReasons: string[]; resultIDs: string[]; }
interface SuiteManifest { scope: string; total: number; finished: number; reviews: number; failures: number; cancelled: boolean; cooldownPolicy: string; plannedJobIDs: string[]; jobs: SuiteAttempt[]; }

@ObservedV2
export class SuiteState {
  @Trace running: boolean = false;
  @Trace status: string = '';
  @Trace currentLabel: string = '';
  @Trace total: number = 0;
  @Trace finished: number = 0;
  @Trace reviews: number = 0;
  @Trace failures: number = 0;
  @Trace cancelled: boolean = false;
}
export const suiteRun: SuiteState = new SuiteState();

/** Application-wide state machine advanced by the existing controller timer. */
export class MobileSuiteRunner {
  private jobs: SuiteJob[] = [];
  private attempts: SuiteAttempt[] = [];
  private selected: string = 'all';
  private index: number = 0;
  private phase: string = '';
  private filesDir: string = '';
  private folder: string = '';
  private before: string[] = [];
  private deadline: number = 0;
  private readyAt: number = 0;
  private stopDeadline: number = 0;
  private usesCli: boolean = false;
  private error: string = '';
  private fullscreen: (show: boolean, job: SuiteJob) => void = () => {};
  private keepScreen: (on: boolean) => void = () => {};

  start(filesDir: string, scope: string, fullscreen: (show: boolean, job: SuiteJob) => void, keepScreen: (on: boolean) => void): void {
    const empty: SuiteCapabilities = { vulkan: false, liquid: false, vulkanMaxParticles: 0, gles31: false, glesMaxParticles: 0, gles2: false };
    let caps: SuiteCapabilities = empty;
    if (scope !== 'cpu') {
      try { caps = JSON.parse(bridge.suiteCapabilities()) as SuiteCapabilities; } catch (e) { caps = empty; }
    }
    this.jobs = supportedSuiteJobs(caps, scope);
    if (this.jobs.length === 0) { suiteRun.status = tr('No supported GPU jobs', '没有支持的 GPU 项目'); return; }
    this.filesDir = filesDir; this.selected = scope; this.fullscreen = fullscreen; this.keepScreen = keepScreen;
    this.folder = filesDir + '/suite-runs/' + Date.now().toString();
    if (!fs.accessSync(filesDir + '/suite-runs')) fs.mkdirSync(filesDir + '/suite-runs');
    fs.mkdirSync(this.folder);
    this.index = 0; this.attempts = []; this.phase = 'prepare';
    suiteRun.running = true; suiteRun.cancelled = false; suiteRun.total = this.jobs.length;
    suiteRun.finished = 0; suiteRun.reviews = 0; suiteRun.failures = 0;
    this.keepScreen(true); this.log(`SUITE_PLAN\ttotal=${this.jobs.length}\tscope=${scope}\tfolder=${this.folder}`); this.save();
  }
  cancel(): void {
    if (!suiteRun.running) return;
    suiteRun.cancelled = true; suiteRun.status = tr('Cancelling…', '正在取消…'); bridge.stopWorkload();
  }
  private log(line: string): void {
    if (line.startsWith('SUITE_')) hilog.info(0, 'Mangekyo', '%{public}s', line);
    const file: fs.File = fs.openSync(this.folder + '/run.log', fs.OpenMode.CREATE | fs.OpenMode.READ_WRITE | fs.OpenMode.APPEND);
    try { fs.writeSync(file.fd, line + '\n'); } finally { fs.closeSync(file); }
  }
  private save(): void {
    const manifest: SuiteManifest = { scope: this.selected, total: suiteRun.total, finished: suiteRun.finished,
      reviews: suiteRun.reviews, failures: suiteRun.failures, cancelled: suiteRun.cancelled,
      cooldownPolicy: 'fixed_30s_pause_unverified_temperature', plannedJobIDs: this.jobs.map((job: SuiteJob) => job.id), jobs: this.attempts };
    const file: fs.File = fs.openSync(this.folder + '/manifest.json', fs.OpenMode.CREATE | fs.OpenMode.READ_WRITE | fs.OpenMode.TRUNC);
    try { fs.writeSync(file.fd, JSON.stringify(manifest)); } finally { fs.closeSync(file); }
  }
  private finish(): void {
    const job: SuiteJob = this.jobs[Math.min(this.index, this.jobs.length - 1)];
    this.fullscreen(false, job); this.keepScreen(false); gpuRun.active = false;
    if (!bridge.isCliRunning()) cpuRun.running = false;
    suiteRun.running = false;
    suiteRun.status = suiteRun.cancelled ? tr(`Cancelled · ${suiteRun.finished}/${suiteRun.total}`, `已取消 · ${suiteRun.finished}/${suiteRun.total}`)
      : tr(`Completed ${suiteRun.finished}/${suiteRun.total} · review ${suiteRun.reviews} · failed ${suiteRun.failures}`,
        `完成 ${suiteRun.finished}/${suiteRun.total} · 待复核 ${suiteRun.reviews} · 失败 ${suiteRun.failures}`);
    this.log(`SUITE_END\tfinished=${suiteRun.finished}\ttotal=${suiteRun.total}\treview=${suiteRun.reviews}\tfailed=${suiteRun.failures}\tcancelled=${suiteRun.cancelled}`);
    this.save();
  }
  tick(): void {
    if (!suiteRun.running) return;
    const job: SuiteJob = this.jobs[this.index];
    const now: number = Date.now();
    if (suiteRun.cancelled && this.phase !== 'running') { this.finish(); return; }
    if (this.phase === 'cooling') {
      suiteRun.status = tr(`Resting · ${Math.max(0, Math.ceil((this.deadline - now) / 1000))} s`, `休息中 · ${Math.max(0, Math.ceil((this.deadline - now) / 1000))} 秒`);
      if (now >= this.deadline) this.phase = 'prepare';
      return;
    }
    if (this.phase === 'prepare') {
      suiteRun.currentLabel = job.label(); suiteRun.status = tr(`Running ${this.index + 1}/${suiteRun.total}`, `运行中 ${this.index + 1}/${suiteRun.total}`);
      this.before = loadResults(this.filesDir).map((r: ResultEntry) => r.id);
      this.error = ''; this.stopDeadline = 0; bridge.pollOutput();
      this.usesCli = job.cpu() || job.workload === 'membw' || (job.headless && job.api === API_VULKAN);
      if (job.cpu()) cpuRun.begin(job.workload === 'cpu_membw', this.index > 0 && this.jobs[this.index - 1].cpu());
      else gpuRun.begin();
      this.fullscreen(!job.cpu() && !job.headless && job.workload !== 'membw', job);
      this.deadline = now + 15000; this.readyAt = now + 600;
      this.phase = !job.cpu() && !job.headless && job.workload !== 'membw' ? 'settling' : 'starting';
      this.log(`SUITE_RUN\tindex=${this.index + 1}\ttotal=${suiteRun.total}\tjob=${job.id}`);
    }
    if (this.phase === 'settling') {
      if (now < this.readyAt) return;
      this.phase = 'starting';
    }
    if (this.phase === 'starting') {
      let ok: boolean = false;
      if (this.usesCli) {
        const args: string[] = !job.cpu() ? gpuCommandLine(job.request(), 15) : job.workload === 'cpu_membw'
          ? ['--cpu-membw', '--cpu-membw-storage-dir', this.filesDir]
          : ['--cpu-benchmark', 'all', '--cpu-time', '15', '--cpu-warmup', '0.2'];
        ok = bridge.startCli(args);
      } else if (job.headless || bridge.hasSurface()) ok = bridge.startWorkload(job.request(), 15);
      if (ok) { this.phase = 'running'; this.deadline = now + (job.cpu() || job.workload === 'membw' ? 7200000 : 180000); }
      else if (now >= this.deadline) { this.error = bridge.lastError() || 'start timeout'; this.phase = 'finished'; }
      else return;
    }
    if (this.phase === 'running') {
      const lines: string[] = bridge.pollOutput();
      for (const line of lines) this.log(line);
      if (job.cpu()) cpuRun.consumeAll(lines); else gpuRun.consumeAll(lines);
      if (bridge.isRunning() || bridge.isCliRunning()) {
        if (suiteRun.cancelled || now >= this.deadline) {
          bridge.stopWorkload();
          if (!suiteRun.cancelled) this.error = 'worker timeout';
          if (this.stopDeadline === 0) this.stopDeadline = now + 30000;
          if (now >= this.stopDeadline) {
            this.log('SUITE_ERROR\treason=worker did not stop; remaining jobs aborted');
            suiteRun.cancelled = true; this.finish();
            suiteRun.status = tr('Worker did not stop; queue aborted', '当前测试未结束，已停止后续队列');
          }
        }
        return;
      }
      this.phase = 'finished';
    }
    if (this.phase === 'finished') {
      const lines: string[] = bridge.pollOutput();
      for (const line of lines) this.log(line);
      if (job.cpu()) cpuRun.consumeAll(lines); else gpuRun.consumeAll(lines);
      if (job.cpu()) cpuRun.finish(suiteRun.cancelled ? 130 : bridge.cliExitCode());
      gpuRun.active = false; this.fullscreen(false, job);
      if (suiteRun.cancelled) { this.finish(); return; }
      if (this.error.length === 0) this.error = this.usesCli && bridge.cliExitCode() !== 0
        ? 'worker exit ' + bridge.cliExitCode().toString() : bridge.lastError();
      const saved: ResultEntry[] = loadResults(this.filesDir).filter((r: ResultEntry) => this.before.indexOf(r.id) < 0 && r.matches(job));
      if (saved.length === 0 && this.error.length === 0) this.error = 'no matching result saved';
      const reasons: string[] = [];
      for (const r of saved) for (const reason of r.reviewReasons()) if (reasons.indexOf(reason) < 0) reasons.push(reason);
      const verdict: string = this.error.length > 0 ? 'failed' : reasons.length > 0 ? 'review' : 'success';
      if (verdict === 'failed') suiteRun.failures++; else if (verdict === 'review') suiteRun.reviews++;
      suiteRun.finished++;
      const attempt: SuiteAttempt = { id: job.id, status: verdict, error: this.error, reviewReasons: reasons, resultIDs: saved.map((r: ResultEntry) => r.id) };
      this.attempts.push(attempt); this.log(`SUITE_RESULT\tstatus=${verdict}\tjob=${job.id}\treason=${this.error} ${reasons.join('; ')}`); this.save();
      this.index++;
      if (this.index === this.jobs.length) { this.finish(); return; }
      // Retail HarmonyOS may expose no usable thermal telemetry. Record a
      // fixed pause; do not claim this proves nominal temperature.
      this.keepScreen(true); this.deadline = now + 30000; this.phase = 'cooling';
    }
  }
}
