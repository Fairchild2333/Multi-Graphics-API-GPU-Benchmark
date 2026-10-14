// Stand-in for UIKit: the checked files only import it.
@_exported import Foundation

@MainActor public final class UIApplication {
    public static let shared = UIApplication()
    public var isIdleTimerDisabled = false
}
