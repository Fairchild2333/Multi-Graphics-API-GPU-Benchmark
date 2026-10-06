// History page: every saved result, newest first.

import { Card } from '../components/Card';
import { formatNumber } from '../common/Format';
import { tr, workloadTitle } from '../common/I18n';
import { clearResults, loadResults, ResultEntry, resultsPath } from '../common/ResultsStore';
import { appState } from '../common/RunController';

@Extend(Text)
function caption() {
  .fontSize(12)
  .fontColor($r('sys.color.ohos_id_color_text_secondary'))
}

@ComponentV2
export struct HistoryPage {
  /** Read again whenever a run may have saved a result. */
  @Computed
  get entries(): ResultEntry[] {
    return appState.resultsRevision >= 0 ? loadResults(appState.filesDir) : [];
  }

  private scoreText(entry: ResultEntry): string {
    if (entry.score <= 0) {
      return '—';
    }
    return formatNumber(entry.score) + (entry.scoreUnit.length > 0 ? ' ' + entry.scoreUnit : '');
  }

  build() {
    Scroll() {
      Column({ space: 12 }) {
        Row() {
          Text(tr('History', '历史'))
            .fontSize(24)
            .fontWeight(FontWeight.Bold)
            .fontColor($r('sys.color.ohos_id_color_text_primary'))
            .layoutWeight(1)
          Button(tr('Clear all', '全部清除'))
            .fontSize(13)
            .height(32)
            .enabled(this.entries.length > 0)
            .onClick(() => {
              clearResults(appState.filesDir);
              appState.resultsRevision++;
            })
        }
        .width('100%')

        if (this.entries.length === 0) {
          Text(tr('No results yet', '还没有成绩'))
            .fontSize(16)
            .fontWeight(FontWeight.Medium)
            .fontColor($r('sys.color.ohos_id_color_text_primary'))
          Text(tr('Scores appear here after the engine writes results.json into the app\'s files. They are ' +
            'grouped by test version; do not compare them with desktop scores.',
            '引擎把成绩写入 App 文件目录下的 results.json 后，会显示在这里。成绩按测试版本分组，不要和桌面版的成绩放在一起比较。'))
            .caption()
          Text(resultsPath(appState.filesDir))
            .caption()
        } else {
          ForEach(this.entries, (entry: ResultEntry) => {
            Card() {
              Text(workloadTitle(entry.workload) + ' · ' + entry.graphicsApi)
                .fontSize(15)
                .fontWeight(FontWeight.Medium)
                .fontColor($r('sys.color.ohos_id_color_text_primary'))
              Text(entry.deviceName)
                .fontSize(13)
                .fontColor($r('sys.color.ohos_id_color_text_primary'))
              Text(entry.workloadVersion)
                .caption()
              Text(this.scoreText(entry))
                .fontSize(15)
                .fontWeight(FontWeight.Medium)
                .fontColor($r('sys.color.ohos_id_color_text_primary'))
              Text(entry.timestamp)
                .caption()
            }
          }, (entry: ResultEntry, index: number) => index.toString() + entry.id + entry.timestamp)
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
