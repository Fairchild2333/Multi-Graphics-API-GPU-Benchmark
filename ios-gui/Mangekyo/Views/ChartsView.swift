// ChartsView.swift — Comparison charts drawn from the saved results.
// One chart per test: the best score of every device under the newest version.

import SwiftUI

struct ChartsView: View {
    @EnvironmentObject var engine: BenchEngine

    var body: some View {
        ScrollView {
            SavedResultCharts(results: engine.results)
                .padding(16)
        }
        .onAppear {
            if !engine.workingDirectory.isEmpty {
                engine.refreshResults()
            }
        }
    }
}
