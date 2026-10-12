// The app shell for HarmonyOS 6.1 (API 23) and later: the same four pages as
// Index.ets, in the tabs of the UI Design Kit. On a narrow window the tab bar
// floats over the page on the system material; from 600 vp the tabs stand at
// the side, as in Index.ets.
//
// EntryAbility loads this page only where the kit and the floating style
// exist. Nothing else imports it, so an older system never loads the kit.

import { window } from '@kit.ArkUI';
import { hdsMaterial, HdsTabs, HdsTabsController } from '@kit.UIDesignKit';
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
struct IndexHds {
  private tabs: HdsTabsController = new HdsTabsController();
  private timer: number = -1;

  aboutToAppear(): void {
    appState.floatingTabs = true;
    appState.floatingBarGap = this.systemKeepsBottomArea() ? 0 : 16;
    this.timer = startShellTimer(this.tabs);
  }

  /**
   * True where the system keeps an area at the bottom of the window for
   * itself, as a phone does for its navigation bar. The floating bar then
   * stands clear of the edge already; in a free window of a two-in-one it
   * would sit right on it.
   */
  private systemKeepsBottomArea(): boolean {
    try {
      const area: window.AvoidArea | undefined =
        runController.mainWindow?.getWindowAvoidArea(window.AvoidAreaType.TYPE_NAVIGATION_INDICATOR);
      return area !== undefined && area.bottomRect.height > 0;
    } catch (e) {
      return false;
    }
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
      HdsTabs({ barPosition: appState.sideTabs ? BarPosition.Start : BarPosition.End, controller: this.tabs }) {
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
      // The pages run on under the floating bar and leave room for it at their
      // end (AppState.bottomInset).
      .barOverlap(!appState.sideTabs)
      .barFloatingStyle(appState.sideTabs ? undefined : {
        systemMaterialEffect: { materialType: hdsMaterial.MaterialType.ADAPTIVE },
        barBottomMargin: appState.floatingBarGap
      })
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
