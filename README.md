# Terralite

Terralite is a voxel survival game prototype with a C++ engine, scriptable content packs, RmlUI-based HUD, and ENet-powered client/server networking.

## Build

```sh
cmake -B build -DTERRALITE_ENABLE_DILIGENT=ON
cmake --build build

The project uses CMake FetchContent for third-party dependencies. A clean out-of-tree build is supported and should not depend on the build directory being inside the repository.

## Run

Launcher:

```sh
./build/TerraliteLauncher
```

Native macOS Swift launcher:

```sh
./scripts/build_and_run.sh
```

Offline client:

```sh
./build/Terralite
```

Client hosting a local multiplayer session:

```sh
./build/Terralite --host 27015 --name Host
```

Client connecting to a server:

```sh
./build/Terralite --connect 127.0.0.1 27015 --name Player
```

Dedicated server:

```sh
./build/TerraliteServer --port 27015
```

Server bootstrap smoke test:

```sh
./build/TerraliteServer --bootstrap-only
```

## Launcher Versions

The launcher always creates a `local-dev` version for the current build. It also discovers installed versions from the launcher data directory:

```text
~/Library/Application Support/TERRALITE/Launcher/versions/<version-id>/manifest.json
```

Each manifest can describe a release installed in that folder:

```json
{
  "id": "0.1.0",
  "name": "Terralite 0.1.0",
  "channel": "stable",
  "source": "manifest",
  "gameExecutable": "Terralite",
  "serverExecutable": "TerraliteServer",
  "workingDirectory": ".",
  "extraArguments": "",
  "installed": true
}
```

Relative executable and working-directory paths are resolved from the manifest folder.

The ImGui launcher can install the current local build into this layout with `Install current local build`. That copies the built `Terralite` and `TerraliteServer` executables plus the repo `packs/` directory into a version folder, then writes `manifest.json` for that installed copy.

The Swift launcher lives at `platform/apple/TerraliteLauncherSwift` and uses the same `launcher.json` and version manifest layout as the ImGui launcher. The repo `Run` action is wired to `scripts/build_and_run.sh`, which builds the SwiftPM app, stages `dist/TerraliteLauncherSwiftStandalone.app`, and opens it as a normal macOS app bundle.

## Project Layout

- `src/common` and `include/common`: shared game data, scripting, networking, ECS, world simulation, and persistence.
- `src/client` and `include/client`: rendering, UI, asset-pack loading, and client game runtime.
- `src/server` and `include/server`: headless server bootstrap and server runtime.
- `src/app`: executable entrypoints and app-layer wiring for client/server.
- `src/launcher` and `include/launcher`: platform-neutral launcher core for accounts, versions, config, and process launch.
- `platform/apple/TerraliteLauncherSwift`: native SwiftUI macOS launcher using the same launcher data files.
- `cmake`: split build configuration for dependencies, targets/tests/codegen, and packaging.
- `docs`: project and pack-authoring documentation.
- `engine/scripts`: built-in JavaScript APIs exposed to packs.
- `packs/base`: base game content, scripts, models, textures, and UI assets.
- `packs/types`: generated schema, TypeScript declarations, and editor snippets for pack authors.
- `third_party`: vendored single-header libraries used by the C++ targets.
- `tools/codegen`: schema-driven generation for C++ script bindings and pack-authoring metadata.

## Near-Term Roadmap

1. Launcher polish: add install/update manifests, authenticated account providers, and clearer version health/status checks.
2. Multiplayer smoke test: run a dedicated server and client, then verify chunk sync, chat, block edits, inventory updates, crafting requests, and world time sync.
3. Pack-authoring polish: keep `packs/types` generated artifacts current, expand example pack coverage, and document the stable script APIs.
4. Test hardening: add targeted tests for path-independent pack discovery, script command registration, network packet round trips, and server bootstrap.
5. Runtime cleanup: continue moving legacy path-string asset access behind `AssetPackManager` so client systems read consistently from loaded packs.
