# Engine Roadmap

This document collects high-leverage directions for growing the Terralite engine.
The near-term priority should be changes that make the engine easier to observe,
extend, and debug before adding large gameplay systems.

## Core Engine

- Add a job system for chunk generation, mesh building, asset loading, and network serialization.
- Move world and chunk state behind cleaner ECS-style systems so gameplay logic does not leak into rendering or client code.
- Add deterministic ticks and replay/log capture for debugging multiplayer and simulation bugs.
- Build an engine diagnostics overlay with frame time, chunk counts, draw calls, memory, network stats, and loaded packs.

## Rendering

- Finish the backend abstraction so OpenGL and Diligent are cleanly swappable.
- Add frustum culling, chunk mesh batching, and transparent pass ordering.
- Add basic lighting, starting with ambient occlusion, then sunlight and block light propagation.
- Add material definitions in packs so blocks can declare transparency, emissive behavior, normal maps, tinting, and related properties.
- Add shader hot reload for faster visual iteration.

## World And Gameplay

- Expand terrain generation with biome layers, ore distribution, structures, caves, and vegetation.
- Add block entities for chests, crafting tables, furnaces, signs, and similar interactive blocks.
- Add item stack metadata and durability.
- Refine collision and physics for stairs, slabs, fluids, ladders, and swimming.
- Add save migration and versioning for world data.

## Packs And Scripting

- Add script lifecycle events for world load, player join, tick, block break/place, and item use.
- Add pack dependency resolution and version constraints.
- Add script permissions and capabilities so packs cannot perform unsafe actions by default.
- Add hot reload for packs and assets.
- Add a validation CLI, such as `terralite validate-pack packs/base`.

## Networking

- Add snapshot interpolation and client prediction for player movement.
- Add chunk streaming priorities based on player movement and view direction.
- Add compression for chunk packets.
- Add auth and session handling, even if offline mode comes first.
- Add a headless server admin console with commands.

## Tooling

- Make the Swift launcher install and manage local builds and packs cleanly.
- Add a content browser or debug editor for blocks, items, recipes, and textures.
- Add golden tests for pack parsing and model baking.
- Add CI builds for macOS, Linux, and Windows.

## Recommended Next Step

Finish the render backend split and add the diagnostics overlay. This gives immediate
visibility into performance and makes future engine work less tangled.
