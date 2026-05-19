// swift-tools-version: 5.9

import PackageDescription

let package = Package(
    name: "TerraliteLauncherSwift",
    platforms: [
        .macOS(.v13)
    ],
    products: [
        .library(name: "TerraliteLauncherSwift", targets: ["TerraliteLauncherSwift"])
    ],
    targets: [
        .target(
            name: "TerraliteLauncherSwift",
            path: "Sources/TerraliteLauncherSwift",
            exclude: [
                "App",
                "Views"
            ]
        ),
        .testTarget(
            name: "TerraliteLauncherSwiftTests",
            dependencies: ["TerraliteLauncherSwift"],
            path: "Tests/TerraliteLauncherSwiftTests"
        )
    ]
)
