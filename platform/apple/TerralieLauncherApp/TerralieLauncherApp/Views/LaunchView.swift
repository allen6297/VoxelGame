import SwiftUI
import TerraliteLauncherSwift

struct LaunchView: View {
    @ObservedObject var model: LauncherViewModel

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 18) {
                HeaderView(title: "Play Terralite", subtitle: model.selectedVersion?.name ?? "No version selected")

                Grid(alignment: .leading, horizontalSpacing: 16, verticalSpacing: 12) {
                    GridRow {
                        Text("Account")
                            .foregroundStyle(.secondary)
                        Picker("Account", selection: Binding(
                            get: { model.selectedAccountId },
                            set: { model.selectAccount($0) }
                        )) {
                            ForEach(model.accounts) { account in
                                Text(account.displayName).tag(account.id)
                            }
                        }
                        .labelsHidden()
                    }

                    GridRow {
                        Text("Version")
                            .foregroundStyle(.secondary)
                        Picker("Version", selection: Binding(
                            get: { model.selectedVersionId },
                            set: { model.selectVersion($0) }
                        )) {
                            ForEach(model.versions) { version in
                                Text(version.name).tag(version.id)
                            }
                        }
                        .labelsHidden()
                    }

                    GridRow {
                        Text("Mode")
                            .foregroundStyle(.secondary)
                        Picker("Mode", selection: $model.launchOptions.playMode) {
                            ForEach(PlayMode.allCases) { mode in
                                Text(mode.rawValue).tag(mode)
                            }
                        }
                        .pickerStyle(.segmented)
                    }

                    GridRow {
                        Text("Host")
                            .foregroundStyle(.secondary)
                        TextField("127.0.0.1", text: $model.launchOptions.hostName)
                    }

                    GridRow {
                        Text("Port")
                            .foregroundStyle(.secondary)
                        TextField("27015", text: $model.launchOptions.port)
                    }
                }
                .textFieldStyle(.roundedBorder)
                .frame(maxWidth: 620, alignment: .leading)

                HStack {
                    Button {
                        model.launchGame()
                    } label: {
                        Label("Launch Game", systemImage: "play.fill")
                    }
                    .buttonStyle(.borderedProminent)

                    Button {
                        model.launchServer()
                    } label: {
                        Label("Start Server", systemImage: "server.rack")
                    }
                }

                VersionSummaryView(
                    version: model.selectedVersion,
                    status: model.selectedVersion.map { model.versionStatus($0) }
                )
                StatusView(message: model.statusMessage)
            }
            .padding(24)
            .frame(maxWidth: .infinity, alignment: .leading)
        }
    }
}

struct VersionSummaryView: View {
    var version: GameVersion?
    var status: VersionStatus?

    var body: some View {
        if let version {
            VStack(alignment: .leading, spacing: 8) {
                Text("Selected Version")
                    .font(.headline)
                LabeledContent("Status", value: status?.rawValue ?? "")
                LabeledContent("Game", value: version.gameExecutable)
                LabeledContent("Server", value: version.serverExecutable)
                LabeledContent("Working Directory", value: version.workingDirectory)
            }
            .textSelection(.enabled)
            .padding()
            .background(.regularMaterial)
            .clipShape(RoundedRectangle(cornerRadius: 8))
        }
    }
}

#Preview("LaunchView") {
    LaunchView(model: .preview)
        .frame(width: 760, height: 560)
}

#Preview("VersionSummaryView") {
    VersionSummaryView(
        version: GameVersion(
            id: "local-dev",
            name: "Local development build",
            channel: "dev",
            source: "local-dev",
            gameExecutable: "/tmp/terralite-launcher-preview/bin/Terralite",
            serverExecutable: "/tmp/terralite-launcher-preview/bin/TerraliteServer",
            workingDirectory: "/tmp/terralite-launcher-preview/source"
        ),
        status: .localDev
    )
    .padding()
    .frame(width: 680)
}
