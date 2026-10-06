// The app shell: GPU, CPU, History and Charts, the same four pages as the
// other front ends. The tabs stand at the bottom of a narrow window and move
// to the side from 600 vp (an opened foldable, a tablet, a 2-in-1, a TV).
//
// This shell runs on every system. On HarmonyOS 6.1 and later EntryAbility
// loads IndexHds.ets instead.

import { tr } from '../common/I18n';
import { appState } from '../common/RunController';
import { startShellTimer, tabItem } from '../common/Shell';
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
    appState.floatingTabs = false;
    this.timer = startShellTimer(this.tabs);
  }

  aboutToDisappear(): void {
    clearInterval(this.timer);
  }

  build() {
    Tabs({ barPosition: appState.sideTabs ? BarPosition.Start : BarPosition.End, controller: this.tabs }) {
      TabContent() {
        GpuPage()
      }
      .tabBar(tabItem($r('sys.symbol.gauge_with_dots_needle_33percent'), 'GPU'))

      TabContent() {
        CpuPage()
      }
      .tabBar(tabItem($r('sys.symbol.memory_module'), 'CPU'))

      TabContent() {
        HistoryPage()
      }
      .tabBar(tabItem($r('sys.symbol.clock'), tr('History', '历史')))

      TabContent() {
        ChartsPage()
      }
      .tabBar(tabItem($r('sys.symbol.histogram'), tr('Charts', '图表')))
    }
    .vertical(appState.sideTabs)
    .width('100%')
    .height('100%')
    .backgroundColor($r('sys.color.ohos_id_color_background'))
    .onSizeChange((oldValue: SizeOptions, newValue: SizeOptions) => {
      if (typeof newValue.width === 'number') {
        appState.windowWidth = newValue.width;
      }
    })
  }
}
