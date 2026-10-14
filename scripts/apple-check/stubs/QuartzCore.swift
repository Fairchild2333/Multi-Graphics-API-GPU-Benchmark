// Stand-in for QuartzCore: only what BenchEngine.swift uses.
@_exported import Foundation

open class CALayer { public init() {} }
open class CAMetalLayer: CALayer {}

open class CAEAGLLayer: CALayer {}
