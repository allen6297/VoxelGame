import Foundation

extension String {
    var terraliteSlug: String {
        var slug = ""
        for scalar in unicodeScalars {
            if CharacterSet.alphanumerics.contains(scalar) {
                slug.unicodeScalars.append(UnicodeScalar(String(scalar).lowercased())!)
            } else if !slug.isEmpty && slug.last != "-" {
                slug.append("-")
            }
        }
        while slug.last == "-" {
            slug.removeLast()
        }
        return slug.isEmpty ? "player" : slug
    }
}
