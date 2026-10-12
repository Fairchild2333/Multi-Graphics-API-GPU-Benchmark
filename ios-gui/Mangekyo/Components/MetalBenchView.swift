// MetalBenchView.swift — UIView with CAMetalLayer for embedded MetalBackend.

import SwiftUI
import UIKit
import QuartzCore

final class MetalHostView: UIView {
    override class var layerClass: AnyClass { CAMetalLayer.self }

    var metalLayer: CAMetalLayer { layer as! CAMetalLayer }

    override init(frame: CGRect) {
        super.init(frame: frame)
        isOpaque = true
        backgroundColor = .black
        metalLayer.pixelFormat = .bgra8Unorm
        metalLayer.framebufferOnly = true
        // The engine renders 1280 x 720 whatever the size of this view, and the
        // system scales the picture. The view must never change that size: the
        // score is computed from it.
        metalLayer.drawableSize = CGSize(width: 1280, height: 720)
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
}

struct MetalBenchView: UIViewRepresentable {
    /// Called once for every view that is created, after the view exists and
    /// outside any view update, so the receiver may start a test in the layer.
    var onLayerReady: (CAMetalLayer) -> Void

    func makeUIView(context: Context) -> MetalHostView {
        let view = MetalHostView()
        DispatchQueue.main.async {
            onLayerReady(view.metalLayer)
        }
        return view
    }

    func updateUIView(_ uiView: MetalHostView, context: Context) {}
}
