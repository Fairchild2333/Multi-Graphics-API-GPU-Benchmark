// ContentView.swift — iOS navigation shell.
// iOS 16+: NavigationStack + TabView (iPhone) or NavigationSplitView (iPad).
// iOS 26+: Liquid Glass selection highlights (via shared GlassCard/SidebarButton).

import SwiftUI

enum SidebarItem: String, CaseIterable, Identifiable, Hashable {
    case run      = "Run"
    case cpu      = "CPU"
    case history  = "History"
    case charts   = "Charts"
    case settings = "Settings"
    case about    = "About"

    var id: String { rawValue }

    var icon: String {
        switch self {
        case .run:      return "play.fill"
        case .cpu:      return "cpu"
        case .history:  return "clock.fill"
        case .charts:   return "chart.bar.fill"
        case .settings: return "gearshape.fill"
        case .about:    return "info.circle.fill"
        }
    }

    var localizedName: String {
        switch self {
        case .run:      return Localization.tr("GPU", "GPU", "GPU")
        case .cpu:      return Localization.tr("CPU", "CPU", "CPU")
        case .history:  return Localization.tr("History", "历史", "履歴")
        case .charts:   return Localization.tr("Charts", "图表", "チャート")
        case .settings: return Localization.tr("Settings", "设置", "設定")
        case .about:    return Localization.tr("About", "关于", "情報")
        }
    }
}

struct ContentView: View {
    @EnvironmentObject var engine: BenchEngine
    @State private var selection: SidebarItem = .run
    @Environment(\.horizontalSizeClass) private var sizeClass

    var body: some View {
        if sizeClass == .regular {
            // iPad: sidebar + detail (split view)
            NavigationSplitView {
                sidebar
                    .navigationTitle("Mangekyo")
            } detail: {
                detail
            }
        } else {
            // iPhone: bottom tab bar
            TabView(selection: $selection) {
                ForEach(SidebarItem.allCases) { item in
                    NavigationStack {
                        detailView(for: item)
                            .navigationTitle(item.localizedName)
                    }
                    .tabItem {
                        Label(item.localizedName, systemImage: item.icon)
                    }
                    .tag(item)
                }
            }
        }
    }

    private var sidebar: some View {
        List(SidebarItem.allCases, selection: $selection) { item in
            Label(item.localizedName, systemImage: item.icon)
                .tag(item)
        }
    }

    @ViewBuilder
    private var detail: some View {
        detailView(for: selection)
    }

    @ViewBuilder
    private func detailView(for item: SidebarItem) -> some View {
        switch item {
        case .run:      RunView()
        case .cpu:      CpuView()
        case .history:  HistoryView()
        case .charts:   ChartsView()
        case .settings: SettingsView()
        case .about:    AboutView()
        }
    }
}
