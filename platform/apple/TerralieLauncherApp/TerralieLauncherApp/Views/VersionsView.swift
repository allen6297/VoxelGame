import SwiftUI
import TerraliteLauncherSwift

struct VersionsView: View {
    @ObservedObject var model: LauncherViewModel
    @State private var draft = GameVersion()

    var body: some View {
        HSplitView {
            List(selection: Binding(
                get: { model.selectedVersionId },
                set: { model.selectVersion($0) }
            )) {
                ForEach(model.versions) { version in
                    VStack(alignment: .leading, spacing: 2) {
                        Text(version.name)
                            .lineLimit(1)
                        Text(model.versionStatus(version).rawValue)
                            .font(.caption)
                            .foregroundStyle(.secondary)
                            .lineLimit(1)
                    }
                    .tag(version.id)
                }
            }
            .frame(minWidth: 260)

            ScrollView {
                VStack(alignment: .leading, spacing: 20) {
                    HeaderView(title: "Game Versions", subtitle: model.versionRootURL.path)

                    VersionEditorView(version: $draft)

                    HStack {
                        Button {
                            model.updateSelectedVersion(draft)
                        } label: {
                            Label("Save Version", systemImage: "checkmark")
                        }
                        Button {
                            model.addVersion()
                            draft = model.selectedVersion ?? GameVersion()
                        } label: {
                            Label("Add", systemImage: "plus")
                        }
                        Button(role: .destructive) {
                            model.removeSelectedVersion()
                            draft = model.selectedVersion ?? GameVersion()
                        } label: {
                            Label("Remove", systemImage: "trash")
                        }
                        .disabled(model.versions.count <= 1)
                        Button {
                            model.refreshVersions()
                            draft = model.selectedVersion ?? GameVersion()
                        } label: {
                            Label("Refresh", systemImage: "arrow.clockwise")
                        }
                    }

                    Divider()
                    InstallLocalBuildView(model: model)
                    StatusView(message: model.statusMessage)
                }
                .padding(24)
                .frame(maxWidth: .infinity, alignment: .leading)
            }
        }
        .onAppear {
            draft = model.selectedVersion ?? GameVersion()
        }
        .onChange(of: model.selectedVersionId) {
            draft = model.selectedVersion ?? GameVersion()
        }
    }
}

struct VersionEditorView: View {
    @Binding var version: GameVersion

    var body: some View {
        Grid(alignment: .leading, horizontalSpacing: 16, verticalSpacing: 12) {
            editableRow("Version id", text: $version.id)
            editableRow("Name", text: $version.name)
            editableRow("Channel", text: $version.channel)
            editableRow("Source", text: $version.source)
            editableRow("Game executable", text: $version.gameExecutable)
            editableRow("Server executable", text: $version.serverExecutable)
            editableRow("Working directory", text: $version.workingDirectory)
            editableRow("Extra arguments", text: $version.extraArguments)
        }
        .textFieldStyle(.roundedBorder)
    }

    private func editableRow(_ title: String, text: Binding<String>) -> some View {
        GridRow {
            Text(title)
                .foregroundStyle(.secondary)
            TextField(title, text: text)
                .frame(minWidth: 420)
        }
    }
}

struct InstallLocalBuildView: View {
    @ObservedObject var model: LauncherViewModel

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            Text("Install Local Build")
                .font(.headline)

            Grid(alignment: .leading, horizontalSpacing: 16, verticalSpacing: 12) {
                GridRow {
                    Text("Install id")
                        .foregroundStyle(.secondary)
                    TextField("local-dev-install", text: $model.installId)
                }
                GridRow {
                    Text("Name")
                        .foregroundStyle(.secondary)
                    TextField("Local Dev Install", text: $model.installName)
                }
                GridRow {
                    Text("Channel")
                        .foregroundStyle(.secondary)
                    TextField("local", text: $model.installChannel)
                }
            }
            .textFieldStyle(.roundedBorder)
            .frame(maxWidth: 560)

            Toggle("Overwrite existing version", isOn: $model.overwriteInstall)

            Button {
                model.installLocalBuild()
            } label: {
                Label("Install Current Local Build", systemImage: "square.and.arrow.down")
            }
        }
    }
}

#Preview("VersionsView") {
    VersionsView(model: .preview)
        .frame(width: 920, height: 680)
}

#Preview("VersionEditorView") {
    @Previewable @State var version = GameVersion(
        id: "local-dev",
        name: "Local development build",
        channel: "dev",
        source: "local-dev",
        gameExecutable: "/tmp/terralite-launcher-preview/bin/Terralite",
        serverExecutable: "/tmp/terralite-launcher-preview/bin/TerraliteServer",
        workingDirectory: "/tmp/terralite-launcher-preview/source"
    )

    VersionEditorView(version: $version)
        .padding()
        .frame(width: 720)
}

#Preview("InstallLocalBuildView") {
    InstallLocalBuildView(model: .preview)
        .padding()
        .frame(width: 640)
}
