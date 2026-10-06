// What the two shells of the app share. Index.ets is the shell for every
// system; IndexHds.ets replaces it on HarmonyOS 6.1 and later, where the tab
// bar floats over the page on the system material.

import { SymbolGlyphModifier } from '@kit.ArkUI';
import { appState, runController } from './RunController';

/** An entry of the tab bar: a system symbol above (or beside) its name. */
export function tabItem(symbol: Resource, label: string): BottomTabBarStyle {
  const icon: TabBarSymbol = new TabBarSymbol();
  icon.normal = new SymbolGlyphModifier(symbol);
  return new BottomTabBarStyle(icon, label);
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
