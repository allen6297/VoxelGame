import SwiftUI
import TerraliteLauncherSwift

struct AccountsView: View {
    @ObservedObject var model: LauncherViewModel
    @State private var displayName = ""

    var body: some View {
        HSplitView {
            List(selection: Binding(
                get: { model.selectedAccountId },
                set: { model.selectAccount($0) }
            )) {
                ForEach(model.accounts) { account in
                    Label(account.displayName, systemImage: "person.crop.circle")
                        .tag(account.id)
                }
            }
            .frame(minWidth: 220)

            VStack(alignment: .leading, spacing: 16) {
                HeaderView(title: "Accounts", subtitle: "Offline profiles for local play and servers")

                TextField("Display name", text: $displayName)
                    .textFieldStyle(.roundedBorder)
                    .frame(maxWidth: 360)

                HStack {
                    Button {
                        model.renameSelectedAccount(displayName)
                    } label: {
                        Label("Save", systemImage: "checkmark")
                    }
                    .disabled(displayName.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty)

                    Button {
                        model.addAccount()
                        displayName = model.selectedAccount?.displayName ?? ""
                    } label: {
                        Label("Add", systemImage: "plus")
                    }

                    Button(role: .destructive) {
                        model.removeSelectedAccount()
                        displayName = model.selectedAccount?.displayName ?? ""
                    } label: {
                        Label("Remove", systemImage: "trash")
                    }
                    .disabled(model.accounts.count <= 1)
                }

                StatusView(message: model.statusMessage)
                Spacer()
            }
            .padding(24)
            .frame(maxWidth: .infinity, alignment: .leading)
        }
        .onAppear {
            displayName = model.selectedAccount?.displayName ?? ""
        }
        .onChange(of: model.selectedAccountId) {
            displayName = model.selectedAccount?.displayName ?? ""
        }
    }
}

#Preview("AccountsView") {
    AccountsView(model: .preview)
        .frame(width: 760, height: 500)
}
