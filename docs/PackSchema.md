<!-- GENERATED FILE - do not edit by hand. -->
# Pack Schema Reference

Source: `tools/codegen/schema/{biome,block,item,recipe,tag}.js`

## BiomeDef

| Field | Type | Required | Default | Validator | Description |
| --- | --- | --- | --- | --- | --- |
| `id` | `BiomeId` | yes |  | biome_id | Unique namespaced identifier. |
| `name` | `string` | yes |  |  | Human-readable display name. |
| `priority` | `float` | no | 0 | non_negative_float | Tie-breaker when two biomes score equally. |
| `rarity` | `float` | no | 1 | non_negative_float | Score multiplier — values below 1 make the biome rarer. |
| `climate.temperature` | `ClimateRange` | no |  |  | Temperature range. |
| `climate.humidity` | `ClimateRange` | no |  |  | Humidity range. |
| `climate.rainfall` | `ClimateRange` | no |  |  | Rainfall range. |
| `climate.elevation` | `ClimateRange` | no |  |  | Elevation range. |
| `climate.drainage` | `ClimateRange` | no |  |  | Drainage range. |
| `climate.waterTable` | `ClimateRange` | no |  |  | Water table range. |
| `terrain.baseHeight` | `float` | no | 48 |  | Average surface height in blocks. |
| `terrain.heightVariation` | `float` | no | 12 |  | Amplitude of height noise. |
| `surface.top` | `BlockId` | no | "base:grass" | block_id | Block on the very top of the terrain column. |
| `surface.middle` | `BlockId` | no | "base:dirt" | block_id | Block filling the middle layer. |
| `surface.base` | `BlockId` | no | "base:stone" | block_id | Block below the middle layer. |
| `surface.middleDepth` | `int` | no | 3 | positive_int | Depth of the middle layer in blocks. |
| `atmosphere.skyColor` | `rgb` | no | [0.58,0.78,0.98] | rgb_0_1 | Sky gradient colour (linear RGB). |
| `atmosphere.fogColor` | `rgb` | no | [0.75,0.85,0.95] | rgb_0_1 | Distance fog colour (linear RGB). |
| `atmosphere.waterColor` | `rgb` | no | [0.2,0.45,0.8] | rgb_0_1 | Water tint colour (linear RGB). |
| `fertility.nitrogen` | `float` | no | 0.5 | non_negative_float |  |
| `fertility.phosphorus` | `float` | no | 0.5 | non_negative_float |  |
| `fertility.potassium` | `float` | no | 0.5 | non_negative_float |  |
| `fertility.magnesium` | `float` | no | 0.5 | non_negative_float |  |
| `fertility.calcium` | `float` | no | 0.5 | non_negative_float |  |
| `fertility.sulfur` | `float` | no | 0.2 | non_negative_float |  |
| `colors` | `Record<string, RGB>` | no |  |  | Named tint colours applied to blocks that opt in via tintKey. @example { grass: [0.3, 0.76, 0.22] } |

## BlockDef

| Field | Type | Required | Default | Validator | Description |
| --- | --- | --- | --- | --- | --- |
| `id` | `BlockId` | yes |  | block_id | Unique namespaced identifier. @example "base:grass" |
| `name` | `string` | yes |  |  | Human-readable display name. |
| `runtimeOrder` | `int` | no | 1000 |  | Ordering hint for runtime ID assignment. Lower values are assigned earlier. |
| `voxel.solid` | `bool` | no | false |  | Whether this block has a solid collision box. |
| `voxel.translucent` | `bool` | no | false |  | Whether light passes through this block. |
| `voxel.material` | `BlockMaterial` | no | "terrain" |  | Physics/sound material group. @example "terrain" "rock" "liquid" "plant" |
| `render.color` | `rgb` | no | [1,1,1] | rgb_0_1 | Base tint colour (linear RGB). Applied when tintKey is false. |
| `render.opacity` | `float` | no | 1 | opacity_0_1 | Alpha opacity. 1 = fully opaque, 0 = invisible. |
| `render.tintKey` | `bool` | no | false |  | When true the engine uses the biome tint colour instead of color. |
| `render.type` | `BlockRenderType` | no | "cube" |  | Render geometry type. "cube" = full block, "model" = custom model. |
| `render.model` | `ModelPath` | no | "" | model_path | Path to the block model JSON, relative to the pack assets folder. |
| `render.texture` | `TexturePath | BlockTextures` | no |  |  | Texture paths. Pass a string to set the albedo only, or an object for named maps. |
| `drops` | `BlockDrop[]` | no |  |  | Items dropped when this block is broken. |
| `properties` | `Record<string, boolean | number | string>` | no |  |  | Arbitrary key–value properties accessible from gameplay code. |

## ItemDef

| Field | Type | Required | Default | Validator | Description |
| --- | --- | --- | --- | --- | --- |
| `id` | `ItemId` | yes |  | item_id | Unique namespaced identifier. @example "base:wheat_seeds" |
| `name` | `string` | yes |  |  | Human-readable display name. |
| `stackSize` | `int` | no | 1 | positive_int | Maximum number of this item per inventory slot. |
| `icon` | `TexturePath` | no | "" | texture_path | Icon texture path relative to the pack assets folder. |
| `placeableBlock` | `BlockId` | no |  | block_id | If set, using this item places the named block in the world. |

## RecipeDef

| Field | Type | Required | Default | Validator | Description |
| --- | --- | --- | --- | --- | --- |
| `id` | `RecipeId` | yes |  | recipe_id | Namespaced recipe id |
| `type` | `RecipeType` | yes |  |  | Recipe type (crafting, smelting, etc.) |
| `output` | `ItemId` | yes |  | item_id | Output item id |
| `count` | `int` | no | 1 | positive_int | Output count |
| `ingredients` | `ItemId[]` | no |  | item_id | List of ingredient item ids |

## TagDef

| Field | Type | Required | Default | Validator | Description |
| --- | --- | --- | --- | --- | --- |
| `id` | `TagId` | yes |  | tag_id | Unique namespaced identifier. |
| `description` | `string` | no | "" |  | Optional human-readable note. |
| `members` | `NamespacedId[]` | no |  |  | Namespaced ids contained in this tag. |
