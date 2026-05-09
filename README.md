# Terralite

Terralite is a voxel survival game prototype with a C++ engine, scriptable content packs, RmlUI-based HUD, and ENet-powered client/server networking.

## Build

```sh
cmake -S . -B build
cmake --build build --target TerraliteLauncher Terralite TerraliteServer TerraliteDataTests
ctest --test-dir build --output-on-failure
```

The project uses CMake FetchContent for third-party dependencies. A clean out-of-tree build is supported and should not depend on the build directory being inside the repository.

## Run

Launcher:

```sh
./build/TerraliteLauncher
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

## Project Layout

- `src/common` and `include/common`: shared game data, scripting, networking, ECS, world simulation, and persistence.
- `src/client` and `include/client`: rendering, UI, asset-pack loading, and client game runtime.
- `src/server` and `include/server`: headless server bootstrap and server runtime.
- `src/app`: executable entrypoints and app-layer wiring for client/server.
- `src/launcher` and `include/launcher`: platform-neutral launcher core for accounts, versions, config, and process launch.
- `engine/scripts`: built-in JavaScript APIs exposed to packs.
- `packs/base`: base game content, scripts, models, textures, and UI assets.
- `packs/types`: generated schema, TypeScript declarations, and editor snippets for pack authors.
- `tools/codegen`: schema-driven generation for C++ script bindings and pack-authoring metadata.

## Near-Term Roadmap

1. Launcher polish: add install/update manifests, authenticated account providers, and clearer version health/status checks.
2. Multiplayer smoke test: run a dedicated server and client, then verify chunk sync, chat, block edits, inventory updates, crafting requests, and world time sync.
3. Pack-authoring polish: keep `packs/types` generated artifacts current, expand example pack coverage, and document the stable script APIs.
4. Test hardening: add targeted tests for path-independent pack discovery, script command registration, network packet round trips, and server bootstrap.
5. Runtime cleanup: continue moving legacy path-string asset access behind `AssetPackManager` so client systems read consistently from loaded packs.
