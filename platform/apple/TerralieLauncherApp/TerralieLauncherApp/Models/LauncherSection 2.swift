import Foundation

enum LauncherSection: String, CaseIterable, Identifiable {
    case play = "Play"
    case accounts = "Accounts"
    case versions = "Versions"

    var id: String { rawValue }

    var systemImage: String {
        switch self {
        case .play: return "play.fill"
        case .accounts: return "person.fill"
        case .versions: return "square.stack.fill"
        }
    }
}
