// Parsed output of the native engine. The engine prints tab-separated protocol
// lines (CPU_*, CPUMEM_*) and periodic "[GPU Timing]" lines; the Windows,
// Android, iOS and macOS front ends read the same lines, so all of them show
// the same numbers.

import { tr } from './I18n';
import { formatBytes, formatNumber } from './Format';

export class CurvePoint {
  series: string;
  x: number;
  y: number;

  constructor(series: string, x: number, y: number) {
    this.series = series;
    this.x = x;
    this.y = y;
  }
}

/** The rates of one cache level, or of memory, for one mode of the sweep. */
export class MemLevel {
  mode: string = '';
  level: string = '';
  cacheBytes: number = 0;
  read: number = 0;
  write: number = 0;
  copy: number = 0;
  writeStream: number = 0;
  copyStream: number = 0;
  zeroFill: number = 0;
  measured: boolean = false;
  settled: boolean = false;
  /** The system reports no size for this cache; the capacity range comes from the curve. */
  fromCurve: boolean = false;
  capacityLow: number = 0;
  capacityHigh: number = 0;
}

export class MemCache {
  cpuClass: number;
  level: number;
  sizeBytes: number;
  sharedCores: number;

  constructor(cpuClass: number, level: number, sizeBytes: number, sharedCores: number) {
    this.cpuClass = cpuClass;
    this.level = level;
    this.sizeBytes = sizeBytes;
    this.sharedCores = sharedCores;
  }
}

/** A class of cores, such as the performance or the efficiency cores. */
export class MemClass {
  rank: number;
  mode: string;
  name: string;
  cores: number;

  constructor(rank: number, mode: string, name: string, cores: number) {
    this.rank = rank;
    this.mode = mode;
    this.name = name;
    this.cores = cores;
  }
}

export class StorageResult {
  measured: boolean = false;
  directIo: boolean = false;
  backing: string = '';
  skipped: string = '';
  seqRead: number = 0;
  seqWrite: number = 0;
  randRead: number = 0;
  randWrite: number = 0;
  randReadIops: number = 0;
  randWriteIops: number = 0;
}

export class CoreScore {
  core: number;
  score: number;
  unit: string;
  label: string;

  constructor(core: number, score: number, unit: string, label: string) {
    this.core = core;
    this.score = score;
    this.unit = unit;
    this.label = label;
  }
}

export class GpuSample {
  seconds: number;
  computeMs: number;
  renderMs: number;
  warmup: boolean;

  constructor(seconds: number, computeMs: number, renderMs: number, warmup: boolean) {
    this.seconds = seconds;
    this.computeMs = computeMs;
    this.renderMs = renderMs;
    this.warmup = warmup;
  }
}

/** "TAG\tkey=value\tkey=value" -> {key: value}. */
function protocolFields(line: string): Map<string, string> {
  const out: Map<string, string> = new Map<string, string>();
  const items: string[] = line.split('\t');
  for (let i = 1; i < items.length; i++) {
    const equals: number = items[i].indexOf('=');
    if (equals > 0) {
      out.set(items[i].substring(0, equals), items[i].substring(equals + 1));
    }
  }
  return out;
}

function numberOf(fields: Map<string, string>, key: string): number {
  const text: string | undefined = fields.get(key);
  if (text === undefined) {
    return 0;
  }
  const value: number = parseFloat(text);
  return isFinite(value) ? value : 0;
}

function textOf(fields: Map<string, string>, key: string): string {
  const text: string | undefined = fields.get(key);
  return text === undefined ? '' : text;
}

function flagOf(fields: Map<string, string>, key: string): boolean {
  return numberOf(fields, key) > 0.5;
}

function clampFraction(value: number): number {
  return Math.min(Math.max(value, 0), 1);
}

/** State of the CPU page. It lives outside the page, so a run survives a change of tab. */
@ObservedV2
export class CpuRunState {
  @Trace running: boolean = false;
  @Trace memoryTest: boolean = false;
  @Trace finished: boolean = false;
  @Trace failed: boolean = false;
  @Trace progress: number = 0;
  @Trace status: string = '';
  @Trace cpuLabel: string = '';

  // Compute test.
  @Trace coreScores: CoreScore[] = [];
  @Trace singleSummary: string = '';
  @Trace multiSummary: string = '';

  // Cache and memory sweep.
  @Trace classes: MemClass[] = [];
  @Trace caches: MemCache[] = [];
  @Trace levels: MemLevel[] = [];
  @Trace points: CurvePoint[] = [];
  @Trace allThreads: number = 0;
  @Trace pageMapping: string = '';
  @Trace storage: StorageResult | undefined = undefined;

  /** Tail of the raw output. */
  @Trace logTail: string = '';

  private coreLabels: Map<number, string> = new Map<number, string>();
  private log: string[] = [];

  begin(memory: boolean, preserveCompute: boolean = false): void {
    this.running = true;
    this.memoryTest = memory;
    this.finished = false;
    this.failed = false;
    this.progress = 0;
    this.status = tr('Starting', '正在启动');
    this.cpuLabel = '';
    if (!preserveCompute) this.coreScores = [];
    this.coreLabels.clear();
    if (!preserveCompute) { this.singleSummary = ''; this.multiSummary = ''; }
    this.classes = [];
    this.caches = [];
    this.levels = [];
    this.points = [];
    this.allThreads = 0;
    this.pageMapping = '';
    this.storage = undefined;
    this.log = [];
    this.logTail = '';
  }

  finish(exitCode: number): void {
    this.running = false;
    this.finished = true;
    this.failed = exitCode !== 0 && exitCode !== 130;
    if (exitCode === 130) {
      this.status = tr('Cancelled', '已取消');
    } else if (this.failed) {
      this.status = tr(`Failed (exit ${exitCode}); see the output below`, `失败（退出码 ${exitCode}），详见下方的输出`);
    } else {
      this.progress = 1;
      this.status = tr('Done', '完成');
    }
  }

  private defaultCoreLabel(core: number): string {
    return tr(`logical processor ${core + 1}`, `逻辑处理器 ${core + 1}`);
  }

  private cpuLabelOf(fields: Map<string, string>, last: string): string {
    const parts: string[] = [];
    if (fields.has('name')) {
      parts.push(textOf(fields, 'name'));
    }
    if (fields.has('logical_count')) {
      parts.push(tr(`${textOf(fields, 'logical_count')} logical`, `${textOf(fields, 'logical_count')} 个逻辑处理器`));
    }
    if (fields.has('physical_count')) {
      parts.push(tr(`${textOf(fields, 'physical_count')} physical`,
        `${textOf(fields, 'physical_count')} 个物理核心`));
    }
    if (last.length > 0) {
      parts.push(last);
    }
    return parts.join(' · ');
  }

  /** Lines are consumed in batches, so the log is published once a batch. */
  consumeAll(lines: string[]): void {
    for (const line of lines) {
      this.consume(line);
    }
    if (lines.length > 0) {
      if (this.log.length > 400) {
        this.log = this.log.slice(this.log.length - 300);
      }
      this.logTail = this.log.slice(Math.max(0, this.log.length - 40)).join('\n');
    }
  }

  private consume(line: string): void {
    this.log.push(line.split('\t').join(' '));
    const tabAt: number = line.indexOf('\t');
    const tag: string = tabAt < 0 ? line : line.substring(0, tabAt);
    if (!tag.startsWith('CPU')) {
      return;
    }
    const f: Map<string, string> = protocolFields(line);
    if (tag === 'CPU_META') {
      this.cpuLabel = this.cpuLabelOf(f, '');
    } else if (tag === 'CPU_TOPOLOGY') {
      if (f.has('core_index')) {
        const core: number = numberOf(f, 'core_index');
        const logical: string = f.has('logical_index') ? textOf(f, 'logical_index') : core.toString();
        const physical: number = f.has('physical_core') ? numberOf(f, 'physical_core') : core;
        let label: string = `LP ${logical} · core ${physical + 1}`;
        if (textOf(f, 'core_class').length > 0) {
          label += ' · ' + textOf(f, 'core_class');
        }
        this.coreLabels.set(core, label);
      }
    } else if (tag === 'CPU_PROGRESS') {
      if (f.has('overall_fraction')) {
        this.progress = clampFraction(numberOf(f, 'overall_fraction'));
      } else if (f.has('fraction')) {
        this.progress = clampFraction(numberOf(f, 'fraction'));
      }
      const core: number = f.has('core_index') ? numberOf(f, 'core_index') : -1;
      const phaseName: string = textOf(f, 'phase');
      let phase: string = tr('finishing', '收尾');
      if (phaseName === 'warmup') {
        phase = tr('warming up', '预热');
      } else if (phaseName === 'measure') {
        phase = tr('measuring', '测量');
      }
      if (textOf(f, 'mode') === 'multi' || core < 0) {
        this.status = tr('All cores', '全核') + ' · ' + phase;
      } else {
        const known: string | undefined = this.coreLabels.get(core);
        this.status = (known === undefined ? this.defaultCoreLabel(core) : known) + ' · ' + phase;
      }
    } else if (tag === 'CPU_ERROR') {
      this.status = f.has('message') ? textOf(f, 'message').split('_').join(' ')
        : tr('CPU engine error', 'CPU 引擎出错');
    } else if (tag === 'CPU_RESULT') {
      const kind: string = textOf(f, 'kind');
      const unit: string = textOf(f, 'unit');
      if (kind === 'core' && f.has('core_index')) {
        const core: number = numberOf(f, 'core_index');
        const known: string | undefined = this.coreLabels.get(core);
        this.coreScores.push(new CoreScore(core, numberOf(f, 'score'), unit,
          known === undefined ? this.defaultCoreLabel(core) : known));
      } else if (kind === 'summary') {
        const average: number = numberOf(f, 'average_score');
        this.singleSummary = formatNumber(average > 0 ? average : numberOf(f, 'score')) + ' ' + unit;
      } else if (kind === 'multi') {
        let text: string = formatNumber(numberOf(f, 'score')) + ' ' + unit;
        if (f.has('thread_count')) {
          text += tr(` (${textOf(f, 'thread_count')} threads)`, `（${textOf(f, 'thread_count')} 线程）`);
        }
        this.multiSummary = text;
      }
    } else if (tag === 'CPUMEM_META') {
      this.memoryTest = true;
      this.allThreads = numberOf(f, 'all_threads');
      this.cpuLabel = this.cpuLabelOf(f, textOf(f, 'vector'));
    } else if (tag === 'CPUMEM_CLASS') {
      this.classes.push(new MemClass(numberOf(f, 'rank'), textOf(f, 'mode'), textOf(f, 'name'),
        numberOf(f, 'cores')));
    } else if (tag === 'CPUMEM_CACHE') {
      this.caches.push(new MemCache(numberOf(f, 'class'), numberOf(f, 'level'), numberOf(f, 'size_bytes'),
        Math.max(1, numberOf(f, 'shared_cores'))));
    } else if (tag === 'CPUMEM_POINT') {
      const x: number = numberOf(f, 'block_bytes');
      const y: number = numberOf(f, 'median_gbps');
      if (x > 0 && y > 0) {
        this.points.push(new CurvePoint(textOf(f, 'mode') + '/' + textOf(f, 'kernel'), x, y));
      }
    } else if (tag === 'CPUMEM_PROGRESS') {
      if (f.has('fraction')) {
        this.progress = clampFraction(numberOf(f, 'fraction'));
      }
      const mode: string = textOf(f, 'mode');
      if (mode.startsWith('single')) {
        this.status = tr('One thread · sweeping data-set sizes', '单线程 · 扫描数据量');
      } else if (mode === 'all') {
        this.status = tr('All cores · sweeping data-set sizes', '全核 · 扫描数据量');
      } else if (mode === 'storage') {
        this.status = tr('Storage · reading and writing a test file', '存储 · 读写测试文件');
      } else {
        this.status = tr('Virtual memory · mapping pages', '虚拟内存 · 映射页面');
      }
    } else if (tag === 'CPUMEM_LEVEL') {
      const level: MemLevel = new MemLevel();
      level.mode = textOf(f, 'mode');
      level.level = textOf(f, 'level');
      level.cacheBytes = numberOf(f, 'cache_bytes');
      level.read = numberOf(f, 'read_gbps');
      level.write = numberOf(f, 'write_gbps');
      level.copy = numberOf(f, 'copy_gbps');
      level.writeStream = numberOf(f, 'write_nt_gbps');
      level.copyStream = numberOf(f, 'copy_nt_gbps');
      level.zeroFill = numberOf(f, 'zero_gbps');
      level.measured = flagOf(f, 'measured');
      level.settled = flagOf(f, 'settled');
      level.fromCurve = flagOf(f, 'from_curve');
      level.capacityLow = numberOf(f, 'capacity_low_bytes');
      level.capacityHigh = numberOf(f, 'capacity_high_bytes');
      this.levels.push(level);
    } else if (tag === 'CPUMEM_PAGEFAULT') {
      if (flagOf(f, 'measured')) {
        this.pageMapping = formatNumber(numberOf(f, 'gbps')) + ' GB/s · ' +
          Math.round(numberOf(f, 'ns_per_page')).toString() + ' ns / ' +
          formatBytes(numberOf(f, 'page_bytes')) + tr(' page', ' 页');
      }
    } else if (tag === 'CPUMEM_STORAGE') {
      const storage: StorageResult = new StorageResult();
      storage.measured = flagOf(f, 'measured');
      storage.directIo = flagOf(f, 'direct_io');
      storage.backing = textOf(f, 'backing');
      storage.skipped = textOf(f, 'skipped');
      storage.seqRead = numberOf(f, 'seq_read_mbps');
      storage.seqWrite = numberOf(f, 'seq_write_mbps');
      storage.randRead = numberOf(f, 'rand_read_mbps');
      storage.randWrite = numberOf(f, 'rand_write_mbps');
      storage.randReadIops = numberOf(f, 'rand_read_iops');
      storage.randWriteIops = numberOf(f, 'rand_write_iops');
      this.storage = storage;
    }
  }
}

const RATE_KEYS: string[] = [
  'RAM rate:', 'VRAM rate:', 'Memory rate:', 'Compute rate:', 'Burn rate:', 'Stress rate:',
  'Fill rate:', 'Render rate:', 'Vol rate:', 'Fluid rate:', 'Liquid rate:',
];

/** The number that follows `key` in `line`, or NaN. */
function numberAfter(line: string, key: string): number {
  const at: number = line.indexOf(key);
  if (at < 0) {
    return NaN;
  }
  return parseFloat(line.substring(at + key.length).trim());
}

function collapseSpaces(text: string): string {
  const words: string[] = [];
  for (const word of text.split(' ')) {
    if (word.length > 0) {
      words.push(word);
    }
  }
  return words.join(' ');
}

/** State of the last GPU run: its timing samples and the score lines of the summary. */
/** One measurement of the VRAM / cache bandwidth sweep. */
export class MemoryPoint {
  kernel: string;
  bytes: number;
  gbps: number;

  constructor(kernel: string, bytes: number, gbps: number) {
    this.kernel = kernel;
    this.bytes = bytes;
    this.gbps = gbps;
  }
}

/** What one kernel of that sweep reached in the cache and in memory, GB/s. */
export class MemoryLevelLine {
  kernel: string;
  cacheGbps: number;
  memoryGbps: number;

  constructor(kernel: string, cacheGbps: number, memoryGbps: number) {
    this.kernel = kernel;
    this.cacheGbps = cacheGbps;
    this.memoryGbps = memoryGbps;
  }
}

/** The value of `key=` in a tab-separated line of the engine, or '' when it is not there. */
function tabField(line: string, key: string): string {
  for (const part of line.split('\t')) {
    if (part.startsWith(key + '=')) {
      return part.substring(key.length + 1);
    }
  }
  return '';
}

@ObservedV2
export class GpuRunState {
  @Trace active: boolean = false;
  @Trace samples: GpuSample[] = [];
  @Trace scoreLine: string = '';
  @Trace fpsLine: string = '';
  /** The frame rate was held at the display's refresh rate, so the score may be too low. */
  @Trace displayLimited: boolean = false;
  /**
   * The size of the compute work groups when the particle test did not run in
   * its own (256): the driver allows no more than this. 0 otherwise. Such a
   * result is in a score group of its own.
   */
  @Trace workGroupSize: number = 0;
  /** The VRAM / cache bandwidth sweep: its points, and the two levels of every kernel. */
  @Trace memoryPoints: MemoryPoint[] = [];
  @Trace memoryLevels: MemoryLevelLine[] = [];

  begin(): void {
    this.active = true;
    this.samples = [];
    this.scoreLine = '';
    this.fpsLine = '';
    this.displayLimited = false;
    this.workGroupSize = 0;
    this.memoryPoints = [];
    this.memoryLevels = [];
  }

  /** The "Avg FPS" of the run, or NaN before it is known. */
  averageFps(): number {
    return numberAfter(this.fpsLine, 'Avg FPS:');
  }

  consumeAll(lines: string[]): void {
    for (const line of lines) {
      this.consume(line);
    }
  }

  private consume(line: string): void {
    if (line.startsWith('[GPU Timing] Compute:') || line.startsWith('[Timing] Compute:')) {
      const seconds: number = numberAfter(line, '| T:');
      const compute: number = numberAfter(line, 'Compute:');
      const render: number = numberAfter(line, 'Render:');
      this.samples.push(new GpuSample(
        // An engine without the "T:" field prints one line a second.
        isFinite(seconds) ? seconds : this.samples.length + 1,
        isFinite(compute) ? compute : 0,
        isFinite(render) ? render : 0,
        line.indexOf('(warm-up)') >= 0));
      return;
    }
    if (line.startsWith('MEMBW_POINT')) {
      const bytes: number = Number(tabField(line, 'dataset_bytes'));
      const rate: number = Number(tabField(line, 'median_gbps'));
      if (isFinite(bytes) && isFinite(rate) && bytes > 0) {
        this.memoryPoints.push(new MemoryPoint(tabField(line, 'kernel'), bytes, rate));
      }
      return;
    }
    if (line.startsWith('MEMBW_SUMMARY')) {
      const kernel: string = tabField(line, 'kernel');
      const memory: number = Number(tabField(line, 'memory_gbps'));
      const cache: number = Number(tabField(line, 'cache_gbps'));
      this.memoryLevels.push(new MemoryLevelLine(kernel, isFinite(cache) ? cache : 0, isFinite(memory) ? memory : 0));
      // The score of this test is how fast the GPU reads its memory.
      if (kernel === 'read' && this.scoreLine.length === 0) {
        this.scoreLine = 'Memory rate: ' + tabField(line, 'memory_gbps') + ' GB/s';
      }
      return;
    }
    const trimmed: string = line.trim();
    // The engine's own verdict: frames had to wait for the display.
    if (trimmed.startsWith('>> Display-bound')) {
      this.displayLimited = true;
    }
    // The engine ran the particle test in smaller work groups than its own.
    const workGroups: string = '>> Work groups of ';
    if (trimmed.startsWith(workGroups)) {
      const size: number = parseInt(trimmed.substring(workGroups.length), 10);
      this.workGroupSize = isNaN(size) ? 0 : size;
    }
    if (this.fpsLine.length === 0 && trimmed.startsWith('Avg FPS:')) {
      this.fpsLine = collapseSpaces(trimmed);
      return;
    }
    if (this.scoreLine.length > 0) {
      return;
    }
    for (const key of RATE_KEYS) {
      if (trimmed.startsWith(key)) {
        this.scoreLine = collapseSpaces(trimmed);
        return;
      }
    }
  }
}

export const cpuRun: CpuRunState = new CpuRunState();
export const gpuRun: GpuRunState = new GpuRunState();
