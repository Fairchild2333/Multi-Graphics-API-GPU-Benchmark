// ContentView.swift — Main navigation shell with sidebar.

import SwiftUI

enum SidebarItem: String, CaseIterable, Identifiable {
    case run      = "Run"
    case history  = "History"
    case charts   = "Charts"
    case settings = "Settings"
    case about    = "About"

    var id: String { rawValue }

    var icon: String {
        switch self {
        case .run:      return "play.fill"
        case .history:  return "clock.fill"
        case .charts:   return "chart.bar.fill"
        case .settings: return "gearshape.fill"
        case .about:    return "info.circle.fill"
        }
    }

    var localizedName: String {
        switch self {
        case .run:      return Localization.tr("Run", "运行")
        case .history:  return Localization.tr("History", "历史")
        case .charts:   return Localization.tr("Charts", "图表")
        case .settings: return Localization.tr("Settings", "设置")
        case .about:    return Localization.tr("About", "关于")
        }
    }
}

struct ContentView: View {
    @EnvironmentObject var engine: BenchEngine
    @State private var selection: SidebarItem = .run
    @Namespace private var animation

    var body: some View {
        NavigationSplitView {
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
            .navigationTitle("GPU Benchmark")
        } detail: {
            Group {
                switch selection {
                case .run:      RunView()
                case .history:  HistoryView()
                case .charts:   ChartsView()
                case .settings: SettingsView()
                case .about:    AboutView()
                }
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity)
        }
        .onAppear {
            if !engine.workingDirectory.isEmpty {
                engine.refreshGpus()
            }
        }
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
                    if #available(macOS 26.0, *) {
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
