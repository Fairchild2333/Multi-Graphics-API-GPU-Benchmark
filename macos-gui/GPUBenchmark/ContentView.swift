// ContentView.swift — Main navigation shell with sidebar (WinUI-aligned pages).
// Deployment floor: macOS 12. NavigationSplitView needs 13+; Monterey uses NavigationView.

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
    @Namespace private var animation

    var body: some View {
        Group {
            if #available(macOS 13.0, *) {
                NavigationSplitView {
                    sidebar
                        .navigationTitle("Mangekyo")
                } detail: {
                    detail
                }
            } else {
                // macOS 12 Monterey: double-column NavigationView.
                NavigationView {
                    sidebar
                        .navigationTitle("Mangekyo")
                        .frame(minWidth: 180, idealWidth: 200, maxWidth: 260)
                    detail
                }
                .navigationViewStyle(.columns)
            }
        }
        .onAppear {
            if !engine.workingDirectory.isEmpty {
                engine.refreshGpus()
            }
        }
    }

    private var sidebar: some View {
        VStack(alignment: .leading, spacing: 6) {
            ForEach(SidebarItem.allCases) { item in
                SidebarButton(item: item, isSelected: selection == item, namespace: animation) {
                    withAnimation(.spring(response: 0.38, dampingFraction: 0.76)) {
                        selection = item
                    }
                }
            }
            Spacer()
        }
        .padding(.top, 16)
        .padding(.horizontal, 12)
        .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
    }

    @ViewBuilder
    private var detail: some View {
        Group {
            switch selection {
            case .run:      RunView()
            case .cpu:      CpuView()
            case .history:  HistoryView()
            case .charts:   ChartsView()
            case .settings: SettingsView()
            case .about:    AboutView()
            }
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
    }
}

struct SidebarButton: View {
    let item: SidebarItem
    let isSelected: Bool
    let namespace: Namespace.ID
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            HStack(spacing: 12) {
                Image(systemName: item.icon)
                    .font(.title3)
                    .foregroundStyle(isSelected ? Color.accentColor : .primary)
                    .frame(width: 24, height: 24)
                Text(item.localizedName)
                    .font(.body)
                    .foregroundStyle(isSelected ? Color.accentColor : .primary)
                    .fontWeight(isSelected ? .semibold : .regular)
                Spacer()
            }
            .padding(.vertical, 8)
            .padding(.horizontal, 12)
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .background {
            ZStack {
                if isSelected {
                    if #available(macOS 26.0, iOS 26.0, *) {
                        RoundedRectangle(cornerRadius: 10)
                            .fill(Color.accentColor.opacity(0.12))
                            .glassEffect(.regular.tint(Color.accentColor.opacity(0.2)).interactive(), in: .rect(cornerRadius: 10))
                            .matchedGeometryEffect(id: "activeBackground", in: namespace)
                    } else {
                        RoundedRectangle(cornerRadius: 10)
                            .fill(Color.accentColor.opacity(0.12))
                            .overlay(
                                RoundedRectangle(cornerRadius: 10)
                                    .stroke(Color.accentColor.opacity(0.25), lineWidth: 1)
                            )
                            .matchedGeometryEffect(id: "activeBackground", in: namespace)
                    }
                }
            }
        }
    }
}
