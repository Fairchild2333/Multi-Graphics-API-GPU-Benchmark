// What the two shells of the app share. Index.ets is the shell for every
// system; IndexHds.ets replaces it on HarmonyOS 6.1 and later, where the tab
// bar floats over the page on the system material.

import { SymbolGlyphModifier } from '@kit.ArkUI';
import { appState, runController } from './RunController';

// The colours of a tab entry are set here. Left to the system, a TV draws the
// name of the selected entry in a colour that does not show on the page (seen
// on the HarmonyOS 6.1 TV emulator, 2026-10-07).
const TAB_LABEL: LabelStyle = {
  selectedColor: $r('sys.color.font_emphasize'),
  unselectedColor: $r('sys.color.font_secondary')
};

/** An entry of the tab bar: a system symbol above (or beside) its name. */
export function tabItem(symbol: Resource, label: string): BottomTabBarStyle {
  // TabBarSymbol is a type only: the system has no such constructor, and
  // `new TabBarSymbol()` ends the app at launch with a ReferenceError.
  const icon: TabBarSymbol = {
    normal: new SymbolGlyphModifier(symbol).fontColor([$r('sys.color.icon_secondary')]),
    selected: new SymbolGlyphModifier(symbol).fontColor([$r('sys.color.icon_emphasize')])
  };
  return new BottomTabBarStyle(icon, label).labelStyle(TAB_LABEL);
}

/**
 * An entry of the tab bar with a picture of the app instead of a system
 * symbol. The GPU page is a cube on every platform, and the system symbols of
 * HarmonyOS have no cube.
 */
export function tabImageItem(image: Resource, label: string): BottomTabBarStyle {
  const colours: TabBarIconStyle = {
    selectedColor: $r('sys.color.icon_emphasize'),
    unselectedColor: $r('sys.color.icon_secondary')
  };
  return new BottomTabBarStyle(image, label).iconStyle(colours).labelStyle(TAB_LABEL);
}

/**
 * Collects engine output and finished runs a few times a second, and shows the
 * tab a command-line request asks for. Returns the timer to clear again.
 */
export function startShellTimer(tabs: TabsController): number {
  return setInterval(() => {
    runController.tick();
    if (appState.requestedTab >= 0) {
      tabs.changeIndex(appState.requestedTab);
      appState.requestedTab = -1;
    }
  }, 200);
}
