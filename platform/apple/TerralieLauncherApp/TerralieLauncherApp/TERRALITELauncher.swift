//
//  TERRALITELauncher.swift
//  TerralieLauncherApp
//
//  Created by Kalob Allen on 5/9/26.
//

import SwiftUI

struct TERRALITELauncher: View {
    @ObservedObject var model: LauncherViewModel
    @State var selected: LauncherButtons = .play

    var body: some View {
        HStack(alignment: .top, spacing: 8) {
            sidebar
            detailContent
        }
        .padding(8)
        .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
    }

    private var sidebar: some View {
        VStack(alignment: .leading, spacing: 8) {
            ForEach(LauncherButtons.allCases) { section in
                LauncherSidebarButton(
                    section: section,
                    isSelected: selected == section
                ) {
                    selected = section
                }
            }
        }
        .padding(12)
        .frame(width: 190)
        .frame(maxHeight: .infinity, alignment: .top)
        .background(.regularMaterial)
        .clipShape(RoundedRectangle(cornerRadius: 8))
    }

    @ViewBuilder
    private var detailContent: some View {
        switch selected {
        case .play:
            LaunchView(model: model)
        case .accounts:
            AccountsView(model: model)
        case .versions:
            VersionsView(model: model)
        case .settings:
            SettingsView(store: model)
                .padding(12)
        case .help:
            LauncherHelpView()
        }
    }
}

private struct LauncherSidebarButton: View {
    let section: LauncherButtons
    let isSelected: Bool
    let action: () -> Void

    private var foregroundColor: Color {
        isSelected ? .white : .primary
    }

    private var fillColor: Color {
        isSelected ? .green : Color.primary.opacity(0.06)
    }

    private var borderColor: Color {
        isSelected ? Color.green.opacity(0.85) : Color.primary.opacity(0.08)
    }

    var body: some View {
        Button(action: action) {
            Label(section.rawValue, systemImage: section.systemImage)
                .font(.system(size: 14, weight: .semibold))
                .frame(maxWidth: .infinity, alignment: .leading)
                .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .foregroundStyle(foregroundColor)
        .padding(.horizontal, 8)
        .padding(.vertical, 8)
        .background {
            RoundedRectangle(cornerRadius: 8)
                .fill(fillColor)
        }
        .overlay {
            RoundedRectangle(cornerRadius: 8)
                .strokeBorder(borderColor)
        }
    }
}

private struct LauncherHelpView: View {
    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            Label("Help", systemImage: "questionmark.circle.fill")
                .font(.title2.weight(.semibold))

            Text("Use Play to launch a local build, Accounts to manage offline profiles, and Versions to review installed manifests.")
                .foregroundStyle(.secondary)
                .lineLimit(3)

            Spacer()
        }
        .padding(24)
        .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
        .background(Color.primary.opacity(0.035))
        .clipShape(RoundedRectangle(cornerRadius: 8))
    }
}

enum LauncherButtons: String, CaseIterable, Identifiable {
    case play = "Play"
    case accounts = "Accounts"
    case versions = "Versions"
    case settings = "Settings"
    case help = "Help"

    var id: String { rawValue }

    var systemImage: String {
        switch self {
        case .play: return "play.fill"
        case .accounts: return "person.fill"
        case .versions: return "square.stack.fill"
        case .settings: return "gearshape.fill"
        case .help: return "questionmark.circle.fill"
        }
    }

    var subtitle: String {
        switch self {
        case .play: return "Launch the local build and pick the profile or server mode you want."
        case .accounts: return "Manage local player profiles for testing and multiplayer sessions."
        case .versions: return "Review installed builds, manifests, and local development versions."
        case .settings: return "Tune launcher paths and app preferences."
        case .help: return "Find project notes, troubleshooting steps, and launcher support."
        }
    }
}

#Preview {
    TERRALITELauncher(model: LauncherViewModel(), selected: .accounts)
        .frame(width: 600, height: 400)
}
