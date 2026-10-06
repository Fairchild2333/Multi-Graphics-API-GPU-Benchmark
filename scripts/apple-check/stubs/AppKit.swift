// Stand-in for AppKit: only what BenchEngine.swift uses.
@_exported import Foundation

public class NSWorkspace {
    public static let shared = NSWorkspace()
    @discardableResult public func open(_ url: URL) -> Bool { true }
}
