// MetalBenchView.swift — UIView with CAMetalLayer for embedded MetalBackend.

import SwiftUI
import UIKit
import QuartzCore
import OpenGLES

final class GlesHostView: UIView {
    override class var layerClass: AnyClass { CAEAGLLayer.self }
    var eaglLayer: CAEAGLLayer { layer as! CAEAGLLayer }
    override init(frame: CGRect) {
        super.init(frame: frame)
        isOpaque = true
        contentScaleFactor = 1
        eaglLayer.contentsScale = 1
        eaglLayer.drawableProperties = [kEAGLDrawablePropertyRetainedBacking: false,
                                       kEAGLDrawablePropertyColorFormat: kEAGLColorFormatRGBA8]
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }
}

final class MetalHostView: UIView {
    let glesView = GlesHostView(frame: CGRect(x: 0, y: 0, width: 1280, height: 720))
    override func layoutSubviews() {
        super.layoutSubviews()
        glesView.center = CGPoint(x: bounds.midX, y: bounds.midY)
        glesView.transform = CGAffineTransform(scaleX: bounds.width / 1280, y: bounds.height / 720)
    }
    override class var layerClass: AnyClass { CAMetalLayer.self }

    var metalLayer: CAMetalLayer { layer as! CAMetalLayer }

    override init(frame: CGRect) {
        super.init(frame: frame)
        addSubview(glesView)
        glesView.isHidden = true
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
    var showGles = false
    var onLayerReady: (CAMetalLayer, CAEAGLLayer) -> Void

    func makeUIView(context: Context) -> MetalHostView {
        let view = MetalHostView()
        view.glesView.isHidden = !showGles
        DispatchQueue.main.async {
            view.layoutIfNeeded()
            onLayerReady(view.metalLayer, view.glesView.eaglLayer)
        }
        return view
    }

    func updateUIView(_ uiView: MetalHostView, context: Context) {
        uiView.glesView.isHidden = !showGles
    }
}
