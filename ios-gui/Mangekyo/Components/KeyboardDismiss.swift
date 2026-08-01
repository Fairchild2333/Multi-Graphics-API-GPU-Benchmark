// KeyboardDismiss.swift — "Done" affordance for numeric keyboards.
//
// `.numberPad` and `.decimalPad` have no return key, so a field using them
// cannot be dismissed by the keyboard alone. Every numeric field in the app
// gets an explicit Done button in the keyboard accessory bar.

import SwiftUI
import UIKit

extension View {
    /// Adds a Done button above the keyboard for fields that have no return key.
    func numericKeyboardDoneButton() -> some View {
        toolbar {
            ToolbarItemGroup(placement: .keyboard) {
                Spacer()
                Button(Localization.tr("Done", "完成", "完了")) {
                    UIApplication.shared.sendAction(
                        #selector(UIResponder.resignFirstResponder),
                        to: nil, from: nil, for: nil)
                }
                .fontWeight(.semibold)
            }
        }
    }
}
