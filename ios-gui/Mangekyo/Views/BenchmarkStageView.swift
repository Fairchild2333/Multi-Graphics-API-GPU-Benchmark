// BenchmarkStageView.swift — Full-screen run surface.
//
// Nothing is drawn over the workload. A GPU benchmark is judged on what it
// renders, and any overlay both obscures the image and costs frames on the very
// GPU being measured. The screen is therefore the Metal layer and nothing else.
//
// Exiting without controls:
//   * the run ends  -> dismiss automatically
//   * tap anywhere  -> cancel and dismiss
// Both are invisible; neither draws anything.

import SwiftUI

struct BenchmarkStageView: View {
    @EnvironmentObject var engine: BenchEngine
    @Environment(\.dismiss) private var dismiss

    /// Starts the run. Called only after the Metal layer is attached and laid
    /// out, so the engine sizes its render target to the full screen.
    let start: () -> Void

    @State private var didStart = false
    @State private var sawRunning = false

    var body: some View {
        ZStack {
            Color.black.ignoresSafeArea()

            MetalBenchView { layer in
                engine.attachMetalLayer(layer)
            }
            .ignoresSafeArea()
        }
        .statusBarHidden(true)
        .persistentSystemOverlays(.hidden)
        // Invisible: no control is drawn, the whole surface just responds.
        .contentShape(Rectangle())
        .onTapGesture {
            engine.cancel(channel: .gpu)
            dismiss()
        }
        .onAppear(perform: startWhenLaidOut)
        .onChange(of: engine.gpuState.isRunning) { running in
            // Leave as soon as the workload finishes. `sawRunning` guards the
            // initial false before the run has started.
            if running {
                sawRunning = true
            } else if sawRunning {
                dismiss()
            }
        }
        .onDisappear {
            engine.cancel(channel: .gpu)
        }
    }

    /// The engine samples drawableSize once during init. Starting on the same
    /// runloop turn as `onAppear` would read the layer before UIKit has laid it
    /// out, and the run would be sized to the fallback resolution instead of
    /// the screen.
    private func startWhenLaidOut() {
        guard !didStart else { return }
        didStart = true
        DispatchQueue.main.async {
            DispatchQueue.main.asyncAfter(deadline: .now() + 0.05, execute: start)
        }
    }
}
