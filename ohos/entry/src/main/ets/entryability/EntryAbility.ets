import { AbilityConstant, bundleManager, Configuration, ConfigurationConstant, UIAbility, Want } from '@kit.AbilityKit';
import { window } from '@kit.ArkUI';
import { deviceInfo } from '@kit.BasicServicesKit';
import { resetLanguage } from '../common/I18n';
import { appState, logLine, runController } from '../common/RunController';

export default class EntryAbility extends UIAbility {
  onCreate(want: Want, launchParam: AbilityConstant.LaunchParam): void {
    // Follow the system's light or dark mode. An app that neither says so nor
    // has a dark resource directory keeps its system colours light in dark
    // mode; only the floating tab bar's material went dark (Mate 60 Pro,
    // 2026-10-08).
    try {
      this.context.getApplicationContext().setColorMode(ConfigurationConstant.ColorMode.COLOR_MODE_NOT_SET);
    } catch (e) {
      logLine('setColorMode failed: ' + JSON.stringify(e));
    }
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
    try {
      runController.mainWindow = windowStage.getMainWindowSync();
    } catch (e) {
      logLine('main window not available: ' + JSON.stringify(e));
    }
    // HarmonyOS 6.1 (API 23) brought the floating tab bar on the system
    // material; systems that have it get the shell built with it. The SDK
    // marks these interfaces as unavailable to TVs and cars.
    const kitDevices: string[] = ['phone', 'tablet', '2in1'];
    const newShell: boolean = deviceInfo.sdkApiVersion >= 23 && kitDevices.includes(deviceInfo.deviceType) &&
      canIUse('SystemCapability.UIDesign.HDSComponent.Core');
    logLine(`shell=${newShell ? 'floating' : 'plain'} api=${deviceInfo.sdkApiVersion} device=${deviceInfo.deviceType}` +
      ` hds=${canIUse('SystemCapability.UIDesign.HDSComponent.Core')}`);
    windowStage.loadContent(newShell ? 'pages/IndexHds' : 'pages/Index', (err) => {
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
