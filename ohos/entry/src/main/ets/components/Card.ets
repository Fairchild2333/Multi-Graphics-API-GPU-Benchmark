// A titled panel; the pages are stacks of these.

@ComponentV2
export struct Card {
  @Param title: string = '';

  @Builder
  emptyContent() {
  }

  @BuilderParam content: () => void = this.emptyContent;

  build() {
    Column({ space: 8 }) {
      if (this.title.length > 0) {
        Text(this.title)
          .fontSize(16)
          .fontWeight(FontWeight.Medium)
          .fontColor($r('sys.color.ohos_id_color_text_primary'))
      }
      this.content()
    }
    .alignItems(HorizontalAlign.Start)
    .padding(14)
    .borderRadius(16)
    .backgroundColor($r('sys.color.ohos_id_color_sub_background'))
    .width('100%')
  }
}
