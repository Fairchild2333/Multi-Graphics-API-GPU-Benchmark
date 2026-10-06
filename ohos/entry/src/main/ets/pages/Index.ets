// The app shell: GPU, CPU, History and Charts, the same four pages as the
// other front ends.

import { tr } from '../common/I18n';
import { appState, runController } from '../common/RunController';
import { ChartsPage } from './ChartsPage';
import { CpuPage } from './CpuPage';
import { GpuPage } from './GpuPage';
import { HistoryPage } from './HistoryPage';

@Entry
@ComponentV2
struct Index {
  private tabs: TabsController = new TabsController();
  private timer: number = -1;

  aboutToAppear(): void {
    // Collect engine output and finished runs a few times a second.
    this.timer = setInterval(() => {
      runController.tick();
      if (appState.requestedTab >= 0) {
        this.tabs.changeIndex(appState.requestedTab);
        appState.requestedTab = -1;
      }
    }, 200);
  }

  aboutToDisappear(): void {
    clearInterval(this.timer);
  }

  build() {
    Tabs({ barPosition: BarPosition.End, controller: this.tabs }) {
      TabContent() {
        GpuPage()
      }
      .tabBar('GPU')

      TabContent() {
        CpuPage()
      }
      .tabBar('CPU')

      TabContent() {
        HistoryPage()
      }
      .tabBar(tr('History', '历史'))

      TabContent() {
        ChartsPage()
      }
      .tabBar(tr('Charts', '图表'))
    }
    .width('100%')
    .height('100%')
    .backgroundColor($r('sys.color.ohos_id_color_background'))
  }
}
