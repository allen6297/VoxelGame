import Foundation

public struct AccountProfile: Codable, Identifiable, Equatable {
    public var id: String
    public var displayName: String
    public var type: String

    public init(id: String, displayName: String, type: String = "offline") {
        self.id = id
        self.displayName = displayName
        self.type = type
    }
}
