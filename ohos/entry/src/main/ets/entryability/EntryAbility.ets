import { AbilityConstant, bundleManager, Configuration, ConfigurationConstant, UIAbility, Want } from '@kit.AbilityKit';
import { window } from '@kit.ArkUI';
import { resetLanguage } from '../common/I18n';
import { appState, logLine, runController } from '../common/RunController';

export default class EntryAbility extends UIAbility {
  onCreate(want: Want, launchParam: AbilityConstant.LaunchParam): void {
    appState.dark = this.context.config.colorMode === ConfigurationConstant.ColorMode.COLOR_MODE_DARK;
    try {
      const info: bundleManager.BundleInfo =
        bundleManager.getBundleInfoForSelfSync(bundleManager.BundleFlag.GET_BUNDLE_INFO_WITH_APPLICATION);
      appState.debuggable = info.appInfo.debug;
    } catch (e) {
      appState.debuggable = false;
    }
    runController.prepare(this.context);
    this.handleWant(want);
  }

  onNewWant(want: Want, launchParam: AbilityConstant.LaunchParam): void {
    this.handleWant(want);
  }

  onConfigurationUpdate(newConfig: Configuration): void {
    appState.dark = newConfig.colorMode === ConfigurationConstant.ColorMode.COLOR_MODE_DARK;
    resetLanguage();
  }

  onWindowStageCreate(windowStage: window.WindowStage): void {
    windowStage.loadContent('pages/Index', (err) => {
      if (err.code) {
        logLine('loadContent failed: ' + JSON.stringify(err));
      }
    });
  }

  /**
   * A test named on the command line, so a device can be tested without touching it:
   * hdc shell aa start -a EntryAbility -b com.mangekyo.benchmark --ps run cpu_membw
   * An optional `--pi seconds N` sets the seconds per CPU compute test or the length of a GPU run.
   */
  private handleWant(want: Want): void {
    const parameters: Record<string, Object> | undefined = want.parameters;
    if (parameters === undefined) {
      return;
    }
    const test: Object | undefined = parameters['run'];
    if (typeof test !== 'string') {
      return;
    }
    const seconds: Object | undefined = parameters['seconds'];
    runController.request(test, typeof seconds === 'number' ? seconds : 0);
  }
}
