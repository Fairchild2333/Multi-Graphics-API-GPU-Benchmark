// The content of a page. A narrow window gets one scrolling column, `primary`
// above `secondary`; from 840 vp the two stand beside each other and scroll on
// their own.
//
// `primary` is built at the same place in both layouts. What it holds, the
// preview surface of a running test in particular, therefore survives a fold,
// a rotation or a resize of the window.

import { appState } from '../common/RunController';

/** One column is not stretched beyond this; a line of text or a chart wider than it reads badly. */
const SINGLE_PANE_MAX_WIDTH: number = 720;

@ComponentV2
export struct AdaptivePanes {
  @Builder
  emptyPane() {
  }

  @BuilderParam primary: () => void = this.emptyPane;
  @BuilderParam secondary: () => void = this.emptyPane;

  build() {
    Row({ space: appState.twoPane ? 24 : 0 }) {
      Scroll() {
        Column({ space: 12 }) {
          this.primary()
          if (!appState.twoPane) {
            this.secondary()
          }
        }
        .alignItems(HorizontalAlign.Start)
        .padding({ top: 16, bottom: 16 + appState.bottomInset })
        .width('100%')
        .constraintSize({ maxWidth: appState.twoPane ? '100%' : SINGLE_PANE_MAX_WIDTH })
      }
      .layoutWeight(1)
      .height('100%')
      .align(Alignment.Top)
      .scrollBar(BarState.Off)

      if (appState.twoPane) {
        Scroll() {
          Column({ space: 12 }) {
            this.secondary()
          }
          .alignItems(HorizontalAlign.Start)
          .padding({ top: 16, bottom: 16 + appState.bottomInset })
          .width('100%')
        }
        .layoutWeight(1)
        .height('100%')
        .align(Alignment.Top)
        .scrollBar(BarState.Off)
      }
    }
    .alignItems(VerticalAlign.Top)
    .padding({ left: appState.margin, right: appState.margin })
    .width('100%')
    .height('100%')
  }
}
