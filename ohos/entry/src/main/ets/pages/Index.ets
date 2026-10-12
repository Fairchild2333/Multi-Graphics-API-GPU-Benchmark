// The app shell: GPU, CPU, History and Charts, the same four pages as the
// other front ends. The tabs stand at the bottom of a narrow window and move
// to the side from 600 vp (an opened foldable, a tablet, a 2-in-1, a TV).
//
// This shell runs on every system. On HarmonyOS 6.1 and later EntryAbility
// loads IndexHds.ets instead.

import { tr } from '../common/I18n';
import { appState, runController } from '../common/RunController';
import { GpuFullscreen } from '../components/GpuFullscreen';
import { startShellTimer, tabImageItem, tabItem } from '../common/Shell';
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

  /** Back stops a running GPU test instead of leaving the app. */
  onBackPress(): boolean {
    if (appState.gpuFullscreen) {
      runController.stopGpu();
      return true;
    }
    return false;
  }

  build() {
    Stack() {
      Tabs({ barPosition: appState.sideTabs ? BarPosition.Start : BarPosition.End, controller: this.tabs }) {
        TabContent() {
          GpuPage()
        }
        .tabBar(tabImageItem($r('app.media.ic_cube'), 'GPU'))

        TabContent() {
          CpuPage()
        }
        // The system symbols have no processor; the nearest one is a memory module.
        .tabBar(tabImageItem($r('app.media.ic_chip'), 'CPU'))

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

      // A running GPU test covers the pages with the picture alone.
      if (appState.gpuFullscreen) {
        GpuFullscreen()
      }
    }
    .width('100%')
    .height('100%')
    .onSizeChange((oldValue: SizeOptions, newValue: SizeOptions) => {
      if (typeof newValue.width === 'number' && typeof newValue.height === 'number') {
        appState.windowWidth = newValue.width;
        appState.windowHeight = newValue.height;
      }
    })
  }
}
