// A running GPU test: the picture alone over the whole window, in landscape
// with the system bars hidden (RunController does that to the window). The
// engine always renders 1280 x 720 into this surface and the system scales it,
// so the picture is 16:9; a window of another shape gets black bars instead of
// a stretched image.

import { gpuTestName } from '../common/GpuOptions';
import { tr } from '../common/I18n';
import { appState, runController } from '../common/RunController';

@ComponentV2
export struct GpuFullscreen {
  /** True when the window is wider than the picture, so the height sets its size. */
  private widerThanPicture(): boolean {
    return appState.windowHeight > 0 && appState.windowWidth / appState.windowHeight > 16 / 9;
  }

  build() {
    Stack({ alignContent: Alignment.Center }) {
      XComponent({ id: 'mangekyo_surface', type: XComponentType.SURFACE, libraryname: 'mangekyo' })
        .width(this.widerThanPicture() ? undefined : '100%')
        .height(this.widerThanPicture() ? '100%' : undefined)
        .aspectRatio(16 / 9)

      Column() {
        Column({ space: 2 }) {
          Text(gpuTestName(appState.gpuWorkload))
            .fontSize(16)
            .fontWeight(FontWeight.Medium)
            .fontColor(Color.White)
          Text(appState.gpuStatus)
            .fontSize(12)
            .fontColor('#CCFFFFFF')
        }
        .alignItems(HorizontalAlign.Start)
        .width('100%')

        Row() {
          Button(tr('Stop', '停止'))
            .onClick(() => {
              runController.stopGpu();
            })
        }
        .justifyContent(FlexAlign.End)
        .width('100%')
      }
      .justifyContent(FlexAlign.SpaceBetween)
      .padding(16)
      .width('100%')
      .height('100%')
    }
    .width('100%')
    .height('100%')
    .backgroundColor(Color.Black)
  }
}
