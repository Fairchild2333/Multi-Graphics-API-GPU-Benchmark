// CPU page: the compute test (per logical processor and all cores) and the
// cache / memory sweep. Both run in the native engine on a worker thread; the
// page shows the results and their charts as the engine prints them.

import { LengthMetrics } from '@kit.ArkUI';
import { AdaptivePanes } from '../components/AdaptivePanes';
import { Card } from '../components/Card';
import { BarItem, BarRows, ChartMarker, ChartSeries, ColumnChart, LineChart } from '../components/Charts';
import { CoreScore, cpuRun, MemClass, MemLevel, StorageResult } from '../common/EngineOutput';
import { formatBytes, formatNumber } from '../common/Format';
import { localizeEngineText, tr } from '../common/I18n';
import { appState, runController } from '../common/RunController';
import { suiteRun } from '../common/MobileSuite';

@Extend(Text)
function caption() {
  .fontSize(12)
  .fontColor($r('sys.color.ohos_id_color_text_secondary'))
}

class ModeChoice {
  id: string;
  en: string;
  zh: string;

  constructor(id: string, en: string, zh: string) {
    this.id = id;
    this.en = en;
    this.zh = zh;
  }
}

const CPU_MODES: ModeChoice[] = [
  new ModeChoice('per-core', 'Single-core', '单核'),
  new ModeChoice('multi', 'All-core', '全核'),
  new ModeChoice('all', 'Single + all-core', '单核 + 全核'),
  new ModeChoice('memory', 'Cache & memory', '缓存与内存'),
];

class LevelRow {
  name: string;
  read: string;
  write: string;
  copy: string;
  header: boolean = false;
  /** A remark that spans the row instead of figures. */
  note: boolean = false;

  constructor(name: string, read: string, write: string, copy: string) {
    this.name = name;
    this.read = read;
    this.write = write;
    this.copy = copy;
  }
}

function rate(value: number): string {
  return value > 0 ? formatNumber(value) : '-';
}

/** Read, write and copy rates of every cache level and of memory, for one mode of the sweep. */
@ComponentV2
struct LevelTable {
  @Param mode: string = 'single';

  private rows(): LevelRow[] {
    const rows: LevelRow[] = [];
    const header: LevelRow = new LevelRow(tr('Level and size', '层级与容量'), tr('read', '读'), tr('write', '写'),
      tr('copy', '复制'));
    header.header = true;
    rows.push(header);
    let anyFromCurve: boolean = false;
    for (const level of cpuRun.levels) {
      if (level.mode !== this.mode) {
        continue;
      }
      const memory: boolean = level.level === 'memory';
      let name: string = '';
      if (memory) {
        name = tr('Memory', '内存');
      } else if (level.fromCurve) {
        anyFromCurve = true;
        name = level.capacityHigh > 0
          ? `${level.level}  ${formatBytes(level.capacityLow)}–${formatBytes(level.capacityHigh)} *`
          : `${level.level}  > ${formatBytes(level.capacityLow)} *`;
      } else {
        name = `${level.level}  ${formatBytes(level.cacheBytes)}`;
      }
      if (level.measured) {
        rows.push(new LevelRow(name, rate(level.read), rate(level.write), rate(level.copy)));
      } else {
        rows.push(new LevelRow(name, '-', '-', '-'));
      }
      if (memory && level.writeStream > 0) {
        rows.push(new LevelRow(tr('  streaming stores', '  流式写入'), '-', rate(level.writeStream),
          rate(level.copyStream)));
      }
      if (memory && level.zeroFill > 0) {
        rows.push(new LevelRow(tr('  CLZERO zero fill', '  CLZERO 清零'), '-', rate(level.zeroFill), '-'));
      }
      if (memory && level.measured && !level.settled) {
        const remark: LevelRow = new LevelRow(
          tr('Not a settled memory figure: the sweep ended before the curve went flat.',
            '这不是稳定的内存数字：扫描在曲线变平之前就结束了。'), '', '', '');
        remark.note = true;
        rows.push(remark);
      }
    }
    if (anyFromCurve) {
      const remark: LevelRow = new LevelRow(
        tr('* The system reports no size for this cache; its capacity was read off the curve and lies in this range.',
          '* 系统没有报告这级缓存的容量；容量是从曲线上读出来的，落在这个范围内。'), '', '', '');
      remark.note = true;
      rows.push(remark);
    }
    return rows;
  }

  build() {
    Column({ space: 4 }) {
      ForEach(this.rows(), (row: LevelRow) => {
        if (row.note) {
          Text(row.name)
            .caption()
        } else {
          Row({ space: 8 }) {
            Text(row.name)
              .fontSize(13)
              .fontColor(row.header ? $r('sys.color.ohos_id_color_text_secondary')
                : $r('sys.color.ohos_id_color_text_primary'))
              .maxLines(1)
              .layoutWeight(1)
            ForEach([row.read, row.write, row.copy], (cell: string) => {
              Text(cell)
                .fontSize(13)
                .fontColor(row.header ? $r('sys.color.ohos_id_color_text_secondary')
                  : $r('sys.color.ohos_id_color_text_primary'))
                .textAlign(TextAlign.End)
                .maxLines(1)
                .width(56)
            }, (cell: string, index: number) => index.toString() + cell)
          }
          .width('100%')
        }
      }, (row: LevelRow, index: number) => index.toString() + row.name + row.read + row.write + row.copy)
    }
    .alignItems(HorizontalAlign.Start)
    .width('100%')
  }
}

/** The table and the curves of one mode of the sweep. */
@ComponentV2
struct SweepCard {
  @Param title: string = '';
  @Param mode: string = 'single';
  /** Rank of the core class whose caches are marked, or -1 for no marks. */
  @Param cacheClass: number = 0;
  @Param threads: number = 1;

  private curves(): ChartSeries[] {
    const read: ChartSeries = new ChartSeries(tr('Read', '读'), 0);
    const write: ChartSeries = new ChartSeries(tr('Write', '写'), 1);
    const copy: ChartSeries = new ChartSeries(tr('Copy', '复制'), 2);
    for (const point of cpuRun.points) {
      if (point.series === this.mode + '/read') {
        read.add(point.x, point.y);
      } else if (point.series === this.mode + '/write') {
        write.add(point.x, point.y);
      } else if (point.series === this.mode + '/copy') {
        copy.add(point.x, point.y);
      }
    }
    return [read, write, copy];
  }

  private hasCurve(): boolean {
    for (const item of this.curves()) {
      if (item.xs.length >= 2) {
        return true;
      }
    }
    return false;
  }

  private hasLevels(): boolean {
    for (const level of cpuRun.levels) {
      if (level.mode === this.mode) {
        return true;
      }
    }
    return false;
  }

  private marks(): ChartMarker[] {
    const marks: ChartMarker[] = [];
    for (const cache of cpuRun.caches) {
      if (cache.cpuClass !== this.cacheClass) {
        continue;
      }
      // A cache shared by several of the running threads counts per thread.
      const sharers: number = Math.max(1, Math.min(this.threads, cache.sharedCores));
      marks.push(new ChartMarker(cache.sizeBytes / sharers,
        sharers > 1 ? `L${cache.level}÷${sharers}` : `L${cache.level}`));
    }
    return marks;
  }

  build() {
    if (this.hasLevels() || this.hasCurve()) {
      Card({ title: this.title }) {
        if (this.hasLevels()) {
          LevelTable({ mode: this.mode })
        }
        if (this.hasCurve()) {
          Text(tr('GB/s by data-set size per thread, log scales. Dashed lines mark the cache sizes the system reports.',
            '带宽（GB/s）随每个线程的数据量变化，双对数坐标。虚线标出系统报告的各级缓存容量。'))
            .caption()
          LineChart({ series: this.curves(), markers: this.marks(), logBytes: true, dark: appState.dark })
        }
      }
    }
  }
}

@ComponentV2
export struct CpuPage {
  @Local formal: boolean = true;

  private memoryMode(): boolean {
    return appState.cpuMode === 'memory';
  }

  private canRun(): boolean {
    return appState.engineReady && !cpuRun.running && !appState.gpuRunning && !suiteRun.running;
  }

  private classes(): MemClass[] {
    return cpuRun.classes.length > 0 ? cpuRun.classes : [new MemClass(0, 'single', '', 0)];
  }

  private classTitle(cpuClass: MemClass): string {
    if (this.classes().length <= 1) {
      return tr('One thread, GB/s', '单线程，GB/s');
    }
    return tr(`Core class ${cpuClass.rank} · ${cpuClass.name} · ${cpuClass.cores} cores — one thread, GB/s`,
      `核心类别 ${cpuClass.rank} · ${cpuClass.name} · ${cpuClass.cores} 个核心 —— 单线程，GB/s`);
  }

  private sortedScores(): CoreScore[] {
    const scores: CoreScore[] = cpuRun.coreScores.slice();
    scores.sort((a: CoreScore, b: CoreScore) => a.core - b.core);
    return scores;
  }

  private requestsText(storage: StorageResult): string {
    let text: string = tr(
      `${formatNumber(storage.randReadIops)} read and ${formatNumber(storage.randWriteIops)} write requests per second`,
      `每秒 ${formatNumber(storage.randReadIops)} 次读请求、${formatNumber(storage.randWriteIops)} 次写请求`);
    if (!storage.directIo) {
      text += tr('. Buffered I/O was used, so the file cache may inflate these.', '。走了系统文件缓存，数字可能偏高。');
    }
    return text;
  }

  /** The choice of test, the button that starts it and its progress. */
  @Builder
  controls() {
    Text(tr('CPU Benchmark', 'CPU 测试'))
      .fontSize(24)
      .fontWeight(FontWeight.Bold)
      .fontColor($r('sys.color.ohos_id_color_text_primary'))

    Card({ title: this.memoryMode() ? tr('Cache & memory bandwidth', '缓存与内存带宽')
      : tr('CPU compute throughput', 'CPU 计算吞吐') }) {
      if (cpuRun.cpuLabel.length > 0) {
        Text(cpuRun.cpuLabel)
          .caption()
      }
      Flex({ wrap: FlexWrap.Wrap, space: { main: LengthMetrics.vp(8), cross: LengthMetrics.vp(8) } }) {
        ForEach(CPU_MODES, (choice: ModeChoice) => {
          Button(tr(choice.en, choice.zh))
            .fontSize(13)
            .height(32)
            .fontColor(appState.cpuMode === choice.id ? Color.White
              : $r('sys.color.ohos_id_color_text_primary'))
            .backgroundColor(appState.cpuMode === choice.id ? $r('sys.color.ohos_id_color_emphasize')
              : $r('sys.color.ohos_id_color_button_normal'))
            .enabled(!cpuRun.running)
            .onClick(() => {
              appState.cpuMode = choice.id;
            })
        }, (choice: ModeChoice) => choice.id)
      }
      if (!this.memoryMode()) {
        Row({ space: 8 }) {
          Button(tr('Quick 1 s', '快速 1 秒'))
            .fontSize(13)
            .height(32)
            .fontColor(!this.formal ? Color.White : $r('sys.color.ohos_id_color_text_primary'))
            .backgroundColor(!this.formal ? $r('sys.color.ohos_id_color_emphasize')
              : $r('sys.color.ohos_id_color_button_normal'))
            .enabled(!cpuRun.running)
            .onClick(() => {
              this.formal = false;
            })
          Button(tr('Formal 15 s', '正式 15 秒'))
            .fontSize(13)
            .height(32)
            .fontColor(this.formal ? Color.White : $r('sys.color.ohos_id_color_text_primary'))
            .backgroundColor(this.formal ? $r('sys.color.ohos_id_color_emphasize')
              : $r('sys.color.ohos_id_color_button_normal'))
            .enabled(!cpuRun.running)
            .onClick(() => {
              this.formal = true;
            })
        }
      }
      Text(this.memoryMode()
        ? tr('Sweeps the data-set size on one thread of every core type, then on all cores, and reads L1, L2, ' +
          'the shared cache and memory off the curve. It ends with a storage test that writes a temporary ' +
          'file of up to 1 GiB. Stopping cancels unfinished tests.',
          '先用每类核心的一个线程、再用全部核心扫描数据量，从曲线上读出 L1、L2、共享缓存和内存各自的带宽。' +
            '最后是存储测试，会写一个最大 1 GiB 的临时文件。停止会取消尚未完成的测试。')
        : tr('The single-core test runs once on every logical processor; the formal preset takes 15 s for ' +
          'each. Stopping cancels unfinished tests.',
          '单核测试会在每个逻辑处理器上各跑一次；正式预设每次 15 秒。停止会取消尚未完成的测试。'))
        .caption()
      Button(tr('Run all CPU tests', '一键测试 CPU 全套'))
        .enabled(this.canRun())
        .onClick(() => { runController.startSuite('cpu'); })
      if (suiteRun.status.length > 0) { Text(suiteRun.status).caption() }
      if (suiteRun.running) { Button(tr('Stop suite', '停止全套测试')).onClick(() => { runController.stopSuite(); }) }
      Button(tr('Run', '运行'))
        .enabled(this.canRun())
        .onClick(() => {
          runController.startCpu(appState.cpuMode, this.formal ? 15 : 1);
        })
      if (cpuRun.running && !suiteRun.running) {
        Button(tr('Stop', '停止')).onClick(() => { runController.stopCpu(); })
      }
    }

    if (cpuRun.running || cpuRun.finished) {
      Card() {
        Row() {
          Text(cpuRun.status)
            .fontSize(14)
            .fontWeight(FontWeight.Medium)
            .fontColor($r('sys.color.ohos_id_color_text_primary'))
            .layoutWeight(1)
          Text(Math.round(cpuRun.progress * 100).toString() + '%')
            .caption()
        }
        .width('100%')
        Progress({ value: cpuRun.progress * 100, total: 100, type: ProgressType.Linear })
          .width('100%')
      }
    }
  }

  /** What the last run measured. */
  @Builder
  results() {
    if (cpuRun.memoryTest) {
      ForEach(this.classes(), (cpuClass: MemClass) => {
        SweepCard({ title: this.classTitle(cpuClass), mode: cpuClass.mode, cacheClass: cpuClass.rank, threads: 1 })
        // The key carries the name and the core count: the card shown before a
        // run stands for "class 0" too, and with a key of rank and mode alone
        // it was kept for the real class 0 and showed "· · 0 cores".
      }, (cpuClass: MemClass) => `${cpuClass.rank}/${cpuClass.mode}/${cpuClass.name}/${cpuClass.cores}`)
      SweepCard({
        title: tr(`All cores — ${cpuRun.allThreads} threads, GB/s (sizes are per thread)`,
          `全核 —— ${cpuRun.allThreads} 个线程，GB/s（数据量按每个线程计）`),
        mode: 'all',
        // With mixed cores the threads of the all-core run see different caches.
        cacheClass: this.classes().length > 1 ? -1 : this.classes()[0].rank,
        threads: Math.max(1, cpuRun.allThreads)
      })
      if (cpuRun.pageMapping.length > 0 || cpuRun.storage !== undefined) {
        Card({ title: tr('Virtual memory and storage', '虚拟内存与存储') }) {
          if (cpuRun.pageMapping.length > 0) {
            Text(tr('Mapping new pages: ', '映射新页面：') + cpuRun.pageMapping)
              .fontSize(13)
              .fontColor($r('sys.color.ohos_id_color_text_primary'))
          }
          if (cpuRun.storage !== undefined) {
            if (cpuRun.storage.backing.length > 0) {
              Text(tr('The system pages to: ', '系统换页到：') + localizeEngineText(cpuRun.storage.backing))
                .caption()
            }
            if (cpuRun.storage.measured) {
              Text(tr('Storage, sequential 1 MiB (MB/s)', '存储，顺序 1 MiB（MB/s）'))
                .caption()
              BarRows({
                items: [new BarItem(tr('Read', '读'), cpuRun.storage.seqRead),
                  new BarItem(tr('Write', '写'), cpuRun.storage.seqWrite)],
                unit: 'MB/s',
                dark: appState.dark
              })
              Text(tr('Storage, random 4 KiB, one request at a time (MB/s)', '存储，随机 4 KiB，每次一个请求（MB/s）'))
                .caption()
              BarRows({
                items: [new BarItem(tr('Read', '读'), cpuRun.storage.randRead),
                  new BarItem(tr('Write', '写'), cpuRun.storage.randWrite)],
                unit: 'MB/s',
                dark: appState.dark
              })
              Text(this.requestsText(cpuRun.storage))
                .caption()
            } else {
              Text(tr('Storage not measured', '存储未测量') + (cpuRun.storage.skipped.length > 0
                ? tr(': ', '：') + localizeEngineText(cpuRun.storage.skipped) : ''))
                .caption()
            }
          }
        }
      }
    }
    if (cpuRun.coreScores.length > 0 || cpuRun.multiSummary.length > 0 || !cpuRun.memoryTest) {
      if (cpuRun.singleSummary.length > 0 || cpuRun.multiSummary.length > 0) {
        Card({ title: tr('Summary', '摘要') }) {
          if (cpuRun.singleSummary.length > 0) {
            Text(tr('Single-core average', '单核平均'))
              .caption()
            Text(cpuRun.singleSummary)
              .fontSize(20)
              .fontWeight(FontWeight.Medium)
              .fontColor($r('sys.color.ohos_id_color_text_primary'))
          }
          if (cpuRun.multiSummary.length > 0) {
            Text(tr('All-core', '全核'))
              .caption()
            Text(cpuRun.multiSummary)
              .fontSize(20)
              .fontWeight(FontWeight.Medium)
              .fontColor($r('sys.color.ohos_id_color_text_primary'))
          }
        }
      }
      if (cpuRun.coreScores.length >= 2) {
        Card({
          title: tr('Score of every logical processor', '各逻辑处理器的成绩') + ` (${cpuRun.coreScores[0].unit})`
        }) {
          ColumnChart({
            values: this.sortedScores().map((score: CoreScore) => score.score),
            labels: this.sortedScores().map((score: CoreScore) => (score.core + 1).toString()),
            dark: appState.dark
          })
          ForEach(this.sortedScores(), (score: CoreScore) => {
            Text(`#${score.core + 1}  ${score.label}  ·  ${formatNumber(score.score)} ${score.unit}`)
              .caption()
          }, (score: CoreScore) => score.core.toString() + score.score.toString())
        }
      }
    }

    if (cpuRun.logTail.length > 0) {
      Card({ title: tr('Raw output (last lines)', '原始输出（最后几行）') }) {
        Text(cpuRun.logTail)
          .fontSize(11)
          .fontColor($r('sys.color.ohos_id_color_text_secondary'))
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
