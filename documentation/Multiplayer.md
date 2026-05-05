# Multiplayer

VoxelGame uses ENet for reliable UDP networking.

## Targets

- `VoxelGame` is the graphical client. It can also run a headless server with `--server`.
- `VoxelServer` is the headless server entry point. It runs ENet without creating a window or OpenGL context.

## Running

Start a headless server through the main executable:

```powershell
.\cmake-build-debug\VoxelGame.exe --server --port 27015
```

`VoxelServer` is also available when Windows policy allows newly built executables:

```powershell
.\cmake-build-debug\VoxelServer.exe --port 27015
```

Start a client:

```powershell
.\cmake-build-debug\VoxelGame.exe --connect 127.0.0.1 27015
```

Disable V-Sync for higher frame rates:

```powershell
.\cmake-build-debug\VoxelGame.exe --limit-fps 0
```

For quick local testing, the graphical client can still host:

```powershell
.\cmake-build-debug\VoxelGame.exe --host 27015
```

## Current Sync

The current multiplayer layer syncs:

- player IDs assigned by the server
- player position, yaw, and pitch
- player inventory and selected slot
- crafting requests (validated by server)
- client chunk interest requests
- authoritative chunk snapshots (compressed with miniz and delta encoding)
- block place and break changes (validated against inventory)
- server-side block simulations (crops, liquids)
- entity synchronization (dropped items and pickups)
- spherical chunk loading and interest areas
- world persistence (chunks and player data saved to disk)
- player names (synchronized and rendered as billboards)
- **Spatial Hashing**: Optimized entity management and targeted synchronization (broadcast range: 64 units).
- **Scripting Integration**: Exposed world and player APIs to QuickJS (runtime scripts in packs/scripts/main.js).
- **Crafting UI**: Implemented an ImGui-based crafting menu (toggle with 'G') that validates ingredients and sends authoritative requests to the server.

Block edits use a request/accept flow:

1. A client sends a reliable block edit request.
2. The server accepts the request and broadcasts a reliable block change.
3. Clients apply only the accepted block change.

Crafting follows a similar authoritative flow:

1. A client sends a `CraftRequest` with a recipe ID.
2. The server validates the player's inventory against recipe ingredients.
3. If valid, the server removes ingredients, adds the output item, and sends reliable inventory updates to the client.

When a graphical client hosts with `--host`, its `Game` instance owns the loaded world and applies accepted requests before broadcasting them. The fully headless server now loads packs and `GameData`, owns a `WorldSimulation`, generates authoritative chunks around spawn and requested player areas, and applies block edit requests only after the target chunk, source player, reach distance, state ID, target block rules, crop soil rules, and placement collision are valid.

## Code Layout

Current layout:

```text
include/common/
include/client/
include/server/
src/common/
src/client/
src/server/
```

Individual modules:

- client-only: GLFW input, rendering, UI, textures, model loading, multithreaded greedy meshing, view frustum culling
- server-only: headless loop, authoritative world simulation, chunk unloading, validation
- shared: packet protocol, `WorldSimulation`, block IDs, terrain/chunk data, serialization

`WorldSimulation` is the shared boundary for authoritative world state. The graphical game now uses it for loaded world data, terrain generation access, and accepted block mutations.

## Next Step

1. **Scripting UI**: Implement UI components that can be driven by QuickJS scripts.
