// One setting of a test: a row of presets with "Custom…" at its end, and a
// number field while that is selected. The same choices as in the desktop app.

import { LengthMetrics } from '@kit.ArkUI';
import { CUSTOM, Preset } from '../common/GpuOptions';
import { tr } from '../common/I18n';

@ComponentV2
export struct PresetRow {
  @Param title: string = '';
  @Param presets: Preset[] = [];
  /** The value of the selected preset, or CUSTOM. */
  @Param choice: number = 0;
  /** What is typed for "Custom…". */
  @Param custom: string = '';
  @Param hint: string = '';
  /** False while the typed number is outside its range. */
  @Param valid: boolean = true;
  @Param active: boolean = true;
  @Event onChoice: (value: number) => void = (value: number) => {
  };
  @Event onCustom: (text: string) => void = (text: string) => {
  };

  @Builder
  chip(label: string, value: number) {
    Button(label)
      .fontSize(13)
      .height(32)
      .fontColor(this.choice === value ? $r('sys.color.font_on_primary')
        : $r('sys.color.ohos_id_color_text_primary'))
      .backgroundColor(this.choice === value ? $r('sys.color.comp_background_emphasize')
        : $r('sys.color.ohos_id_color_button_normal'))
      .enabled(this.active)
      .onClick(() => {
        this.onChoice(value);
      })
  }

  build() {
    Column({ space: 6 }) {
      Text(this.title)
        .fontSize(14)
        .fontWeight(FontWeight.Medium)
        .fontColor($r('sys.color.ohos_id_color_text_primary'))
      Flex({ wrap: FlexWrap.Wrap, space: { main: LengthMetrics.vp(8), cross: LengthMetrics.vp(8) } }) {
        ForEach(this.presets, (preset: Preset) => {
          this.chip(tr(preset.nameEn, preset.nameZh), preset.value)
        }, (preset: Preset) => preset.value.toString())
        this.chip(tr('Custom…', '自定义…'), CUSTOM)
      }
      if (this.choice === CUSTOM) {
        TextInput({ text: this.custom, placeholder: this.hint })
          .type(InputType.Number)
          .maxLength(9)
          .height(40)
          .enabled(this.active)
          .fontColor(this.valid ? $r('sys.color.ohos_id_color_text_primary') : $r('sys.color.ohos_id_color_warning'))
          .onChange((value: string) => {
            this.onCustom(value);
          })
      }
    }
    .alignItems(HorizontalAlign.Start)
    .width('100%')
  }
}
