## Notes
Feel free to divide and separate screens into separate files, as needed.
I'm not a fan of having everything in one file.
I'm also not a fan of having everything in one class.
feel free to add directories to organize code.
I'm not a fan of having everything in one header.

---

## Connected Textures

### Goal

The goal is to add connected textures that feel similar to the Fusion connected textures mod:

- cleaner large surfaces
- reduced visible tiling
- better seams between neighboring blocks
- more intentional material presentation

The target should be Fusion-like visual results, not a rushed attempt to copy every feature immediately.

---

## Why it is worth doing

Connected textures would be a strong next-step visual feature for the game because:

- terrain and building surfaces will look less grid-obvious
- material identity becomes stronger
- glass, bricks, paths, and decorative blocks gain much more value
- biome surfaces can feel richer without adding many new block types

This is especially useful once the terrain system starts producing larger readable landforms and more distinct surface materials.

---

## Design direction

The system should be:

- data-driven where possible
- neighbor-aware during meshing
- limited in first scope
- extensible toward richer connection logic later

Do not build the full most-general system in version one.

Aim for a staged "Fusion-inspired" approach.

---

## Recommended stages

### Stage 1: simple same-block connectivity

Blocks connect only to the same block id or exact material variant.

Use this first for:

- glass
- stone bricks
- path blocks
- selected decorative tiles

Behavior:

- check neighboring blocks during face generation
- choose a texture variant based on connection mask
- support edges, corners, and center tiles as needed

Benefits:

- high visual payoff
- lower implementation risk
- easier atlas authoring

Limits:

- no cross-family connections
- no overlays
- no advanced transitions

### Stage 2: connection groups

Blocks can connect by group rather than only exact block id.

Examples:

- clean stone + mossy stone share a connection family
- multiple glass colors connect cleanly
- decorative tile variants merge into a common surface

This requires:

- per-block connection group metadata
- rules for whether two blocks are visually compatible
- maybe using tags(a new data type)

Benefits:

- much more flexibility
- supports richer building materials

### Stage 3: overlay and transition support

Add support for transition overlays and seam-fixing logic.

Examples:

- grass edge overlays against dirt or path
- moss spread overlays on stone
- biome-specific material transitions
- cracked or aged decorative variants

This is closer to the feel of advanced connected texture mods.

Benefits:

- much more expressive materials
- natural-looking transitions

Risks:

- much more complex asset rules
- more difficult authoring
- more edge cases in meshing

---

## Good first use cases

The first connected-texture-enabled blocks should be ones with the biggest visual return and the smallest rule complexity.

Recommended initial set:

- glass
- stone bricks
- path / road blocks
- maybe one decorative terrain surface

Avoid starting with:

- every terrain block
- highly mixed biome transitions
- large families of partially compatible materials

That will create too much rendering and content complexity too early.

---

## Rendering model

Connected textures should be decided during face generation or meshing.

Core concept:

- when generating a visible face
- inspect relevant neighboring blocks
- compute a connection mask
- select the texture variant for that face from the block's connected texture definition

This means connected textures are primarily a meshing/material selection problem, not a gameplay-system problem.

---

## Data model ideas

Each block that supports connected textures should declare that explicitly.

data/blocks
- `connectedTexture.enabled` `bool`

models/blocks
- `connectedTexture.mode`
- `connectedTexture.connectionGroup`
- `connectedTexture.faceSet`
- `connectedTexture.connectsTo`

Example design concepts:

- `connectedTexture.enabled`
- `connectedTexture.mode`
- `connectedTexture.connectionGroup`
- `connectedTexture.faceSet`
- `connectedTexture.connectsTo`

Possible modes:

- `same_block`
- `same_group`
- `overlay`
- `glass_like`

This should stay data-driven enough that adding support to a new block does not require renderer special cases every time.

---

## Texture atlas implications

Connected textures increase atlas and asset complexity quickly.

You will need:

- a predictable tile layout
- a naming convention for variants
- clear authoring rules for edges, corners, center, and isolated states

Important constraint:

- do not let the first version explode into dozens of variants per block unless the visual gain is clearly worth it

A small curated set of patterns is much easier to ship and maintain.

---

## Neighbor logic

The exact mask system can vary, but the idea is consistent:

- check adjacent blocks relevant to a face
- determine whether each neighbor should connect
- build a bitmask or pattern id
- map that mask to a texture tile

Questions to decide later:

- should diagonals matter in v1?
- should the same mask logic apply to all six faces?
- should side faces and top faces use different rules?

My recommendation:

- start with cardinals first
- add diagonal refinement only if needed for visible quality

---

## Special cases

Some block categories deserve their own handling rules.

### Glass

- should connect seamlessly
- may hide internal borders
- may eventually support tinted variants connecting by group

### Decorative bricks / tiles

- should form larger coherent surfaces
- corners and edges matter a lot visually

### Paths

- should connect into roads and trails
- may need top-face rules different from side-face rules

### Terrain

- should be delayed until the system is stable
- terrain often needs transitions, not just same-material connectivity

---

## Risks

The main risks are:

- making the texture atlas too large or inconsistent
- hardcoding too many special cases into the mesher
- coupling rendering rules too tightly to individual block ids
- designing a system too general before real use cases prove the need

The safest strategy is to ship a narrow version first and expand only after it works well.

---

## Integration with future tooling

This system would benefit from the planned data editor.

Useful editor support later:

- connection group assignment
- preview of mask variants
- face-by-face connectivity preview
- validation for missing texture variants

That would make connected-texture authoring much easier once more blocks use the system.

---

## Recommended implementation path

### Milestone 1

- support connected textures for a few same-block materials
- neighbor mask selection during meshing
- basic atlas layout for connected variants

Enables:

- glass and brick style connectivity

### Milestone 2

- connection groups
- per-face rules
- better authoring conventions

Enables:

- compatible material families

### Milestone 3

- overlays and transition layers
- terrain/material seam cleanup
- more advanced block families

Enables:

- closer-to-Fusion visual quality

---

## Design guardrails

- target Fusion-like results, not Fusion-level scope on day one
- start with a few high-value blocks
- keep connection logic data-driven
- avoid renderer special cases per block whenever possible
- let real content needs drive expansion

---

## Open questions

- Should v1 support only exact same-block connectivity?
- Which blocks benefit most visually right now?
- Do top, side, and bottom faces need different connection rules?
- Do diagonals matter for the first version?
- Should glass hide internal faces completely in addition to texture connection?
