// Charts page: one chart per test with the best saved score of every device
// and API. Results of different versions of a test are not comparable, so a
// chart uses the version of the newest result only.

import { Card } from '../components/Card';
import { BarItem, BarRows } from '../components/Charts';
import { tr, workloadTitle } from '../common/I18n';
import { loadResults, ResultEntry } from '../common/ResultsStore';
import { appState } from '../common/RunController';

@Extend(Text)
function caption() {
  .fontSize(12)
  .fontColor($r('sys.color.ohos_id_color_text_secondary'))
}

class WorkloadChart {
  workload: string;
  version: string;
  unit: string;
  cpu: boolean;
  bars: BarItem[] = [];

  constructor(workload: string, version: string, unit: string, cpu: boolean) {
    this.workload = workload;
    this.version = version;
    this.unit = unit;
    this.cpu = cpu;
  }
}

const KNOWN_ORDER: string[] = ['stream', 'gpu_burn', 'cinematic_liquid', 'membw', 'cpu_single_core',
  'cpu_multi_core', 'cpu_membw'];

function isCpuResult(entry: ResultEntry): boolean {
  return entry.graphicsApi === 'CPU' || entry.workload.startsWith('cpu_');
}

function buildCharts(entries: ResultEntry[]): WorkloadChart[] {
  const groups: Map<string, ResultEntry[]> = new Map<string, ResultEntry[]>();
  for (const entry of entries) {
    if (entry.score <= 0) {
      continue;
    }
    const group: ResultEntry[] | undefined = groups.get(entry.workload);
    if (group === undefined) {
      groups.set(entry.workload, [entry]);
    } else {
      group.push(entry);
    }
  }
  const order: string[] = KNOWN_ORDER.slice();
  const others: string[] = [];
  groups.forEach((value: ResultEntry[], key: string) => {
    if (KNOWN_ORDER.indexOf(key) < 0) {
      others.push(key);
    }
  });
  others.sort();
  for (const key of others) {
    order.push(key);
  }

  const charts: WorkloadChart[] = [];
  for (const workload of order) {
    const rows: ResultEntry[] | undefined = groups.get(workload);
    if (rows === undefined || rows.length === 0) {
      continue;
    }
    let newest: ResultEntry = rows[0];
    for (const row of rows) {
      if (row.timestamp > newest.timestamp) {
        newest = row;
      }
    }
    const cpu: boolean = isCpuResult(newest);
    // Best score per device (and API) under the newest version.
    const best: Map<string, number> = new Map<string, number>();
    for (const row of rows) {
      if (row.workloadVersion !== newest.workloadVersion) {
        continue;
      }
      const label: string = cpu ? (row.cpuName.length > 0 ? row.cpuName : row.deviceName)
        : row.deviceName + ' — ' + row.graphicsApi;
      const known: number | undefined = best.get(label);
      if (known === undefined || row.score > known) {
        best.set(label, row.score);
      }
    }
    const labels: string[] = [];
    best.forEach((value: number, key: string) => {
      labels.push(key);
    });
    labels.sort();
    const chart: WorkloadChart = new WorkloadChart(workload, newest.workloadVersion, newest.scoreUnit, cpu);
    for (const label of labels) {
      chart.bars.push(new BarItem(label, best.get(label) ?? 0));
    }
    if (chart.bars.length > 0) {
      charts.push(chart);
    }
  }
  return charts;
}

@ComponentV2
export struct ChartsPage {
  /** Built again whenever a run may have saved a result. */
  @Computed
  get charts(): WorkloadChart[] {
    return appState.resultsRevision >= 0 ? buildCharts(loadResults(appState.filesDir)) : [];
  }

  private subtitle(chart: WorkloadChart): string {
    return chart.unit + (chart.cpu ? tr(', best saved score per device', '，每个设备保存过的最好成绩')
      : tr(', best saved score per device and API', '，每个设备和 API 保存过的最好成绩')) +
      (chart.version.length > 0 ? '  ·  ' + chart.version : '');
  }

  build() {
    Scroll() {
      Column({ space: 12 }) {
        Text(tr('Charts', '图表'))
          .fontSize(24)
          .fontWeight(FontWeight.Bold)
          .fontColor($r('sys.color.ohos_id_color_text_primary'))

        if (this.charts.length === 0) {
          Text(tr('No results yet', '还没有成绩'))
            .fontSize(16)
            .fontWeight(FontWeight.Medium)
            .fontColor($r('sys.color.ohos_id_color_text_primary'))
          Text(tr('Run a test on the GPU or CPU page. Each test then gets a chart with the best score of every ' +
            'device here.', '先在 GPU 页或 CPU 页运行一项测试，这里会为每项测试画一张图，显示各设备的最好成绩。'))
            .caption()
        } else {
          ForEach(this.charts, (chart: WorkloadChart) => {
            Card({ title: workloadTitle(chart.workload) }) {
              Text(this.subtitle(chart))
                .caption()
              BarRows({ items: chart.bars, unit: chart.unit, dark: appState.dark })
            }
          }, (chart: WorkloadChart) => chart.workload + chart.version + chart.bars.length.toString())
          Text(tr('Each chart uses only the results of the newest version of its test. Every run is listed on ' +
            'the History page.', '每张图只使用该测试最新版本的成绩；每一次运行都列在历史页里。'))
            .caption()
        }
      }
      .alignItems(HorizontalAlign.Start)
      .padding(16)
      .width('100%')
    }
    .width('100%')
    .height('100%')
    .align(Alignment.Top)
    .scrollBar(BarState.Off)
  }
}
