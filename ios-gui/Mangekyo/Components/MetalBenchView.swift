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
        metalLayer.contentsScale = UIScreen.main.scale
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    override func layoutSubviews() {
        super.layoutSubviews()
        let scale = UIScreen.main.scale
        let w = max(bounds.width * scale, 1)
        let h = max(bounds.height * scale, 1)
        metalLayer.drawableSize = CGSize(width: w, height: h)
    }
}

struct MetalBenchView: UIViewRepresentable {
    var onLayerReady: (CAMetalLayer) -> Void

    func makeUIView(context: Context) -> MetalHostView {
        let view = MetalHostView()
        DispatchQueue.main.async {
            onLayerReady(view.metalLayer)
        }
        return view
    }

    func updateUIView(_ uiView: MetalHostView, context: Context) {
        onLayerReady(uiView.metalLayer)
    }
}
