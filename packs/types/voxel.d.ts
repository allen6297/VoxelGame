/**
 * TERRALITE — Pack Scripting API
 *
 * GENERATED FILE — do not edit by hand.
 * Source:      tools/codegen/schema/{biome,block,item,recipe,tag}.js
 * Regenerate:  node tools/codegen/generate.js   (or: cmake --build . --target generate_bindings)
 */


/** RGB colour in linear space, each channel in [0, 1]. */
type RGB = [number, number, number]

/** Creates a nominal string type for editor-only safety. */
type Brand<T, Name extends string> = T & { readonly __brand: Name }

/** Namespaced identifier: "pack:name". @example "base:grass" */
type NamespacedId = Brand<string, 'NamespacedId'>
type BlockId = Brand<NamespacedId, 'BlockId'>
type ItemId = Brand<NamespacedId, 'ItemId'>
type BiomeId = Brand<NamespacedId, 'BiomeId'>
type TagId = Brand<NamespacedId, 'TagId'>
type RecipeId = Brand<NamespacedId, 'RecipeId'>
type TexturePath = Brand<string, 'TexturePath'>
type ModelPath = Brand<string, 'ModelPath'>
type BlockMaterial = 'terrain' | 'rock' | 'liquid' | 'plant'
type BlockRenderType = 'cube' | 'model'
type RecipeType = 'crafting' | 'smelting'

/** A min/max range for a climate axis, normalised to [0, 1]. */
interface ClimateRange { min: number; max: number }

interface BlockTextures {
  albedo?:    TexturePath
  normal?:    TexturePath
  roughness?: TexturePath
  emissive?:  TexturePath
}

interface BlockDrop {
  /** Namespaced item ID. */
  item: ItemId
  count: number
}

interface BlockStatePropInt    { type: 'int';    min: number; max: number; default?: number }
interface BlockStatePropBool   { type: 'bool';   default?: boolean }
interface BlockStatePropString { type: 'string'; values: string[];  default?: string }
type BlockStateProp = BlockStatePropInt | BlockStatePropBool | BlockStatePropString

interface BlockStateVariant { model?: ModelPath }

/** Climate axis ranges this biome occupies (each normalised to [0,1]). */

interface BiomeClimate {
  /** Temperature range. */
  temperature?: ClimateRange
  /** Humidity range. */
  humidity?: ClimateRange
  /** Rainfall range. */
  rainfall?: ClimateRange
  /** Elevation range. */
  elevation?: ClimateRange
  /** Drainage range. */
  drainage?: ClimateRange
  /** Water table range. */
  waterTable?: ClimateRange
}

/** Terrain height parameters. */

interface BiomeTerrain {
  /** Average surface height in blocks. */
  baseHeight?: number
  /** Amplitude of height noise. */
  heightVariation?: number
}

/** Surface block layers. */

interface BiomeSurface {
  /** Block on the very top of the terrain column. */
  top?: BlockId
  /** Block filling the middle layer. */
  middle?: BlockId
  /** Block below the middle layer. */
  base?: BlockId
  /** Depth of the middle layer in blocks. */
  middleDepth?: number
}

/** Sky, fog, and water colours. */

interface BiomeAtmosphere {
  /** Sky gradient colour (linear RGB). */
  skyColor?: RGB
  /** Distance fog colour (linear RGB). */
  fogColor?: RGB
  /** Water tint colour (linear RGB). */
  waterColor?: RGB
}

/** Soil nutrient values for agriculture. */

interface BiomeFertility {
  nitrogen?: number
  phosphorus?: number
  potassium?: number
  magnesium?: number
  calcium?: number
  sulfur?: number
}

/** Physics and voxel-space properties. */

interface BlockVoxel {
  /** Whether this block has a solid collision box. */
  solid?: boolean
  /** Whether light passes through this block. */
  translucent?: boolean
  /** Physics/sound material group. @example "terrain" "rock" "liquid" "plant" */
  material?: BlockMaterial
}

/** Visual and rendering configuration. */

interface BlockRender {
  /** Base tint colour (linear RGB). Applied when tintKey is false. */
  color?: RGB
  /** Alpha opacity. 1 = fully opaque, 0 = invisible. */
  opacity?: number
  /** When true the engine uses the biome tint colour instead of color. */
  tintKey?: boolean
  /** Render geometry type. "cube" = full block, "model" = custom model. */
  type?: BlockRenderType
  /** Path to the block model JSON, relative to the pack assets folder. */
  model?: ModelPath
  /** Texture paths. Pass a string to set the albedo only, or an object for named maps. */
  texture?: TexturePath | BlockTextures
}

interface BiomeDef {
  /** Unique namespaced identifier. */
  id: BiomeId
  /** Human-readable display name. */
  name: string
  /** Tie-breaker when two biomes score equally. */
  priority?: number
  /** Score multiplier — values below 1 make the biome rarer. */
  rarity?: number
  /** Named tint colours applied to blocks that opt in via tintKey. @example { grass: [0.3, 0.76, 0.22] } */
  colors?: Record<string, RGB>
  climate?: BiomeClimate
  terrain?: BiomeTerrain
  surface?: BiomeSurface
  atmosphere?: BiomeAtmosphere
  fertility?: BiomeFertility
}

interface BlockDef {
  states?:   Record<string, BlockStateProp>
  variants?: Record<string, BlockStateVariant>
  /** Unique namespaced identifier. @example "base:grass" */
  id: BlockId
  /** Human-readable display name. */
  name: string
  /** Ordering hint for runtime ID assignment. Lower values are assigned earlier. */
  runtimeOrder?: number
  /** Items dropped when this block is broken. */
  drops?: BlockDrop[]
  /** Arbitrary key–value properties accessible from gameplay code. */
  properties?: Record<string, boolean | number | string>
  voxel?: BlockVoxel
  render?: BlockRender
}

interface ItemDef {
  /** Unique namespaced identifier. @example "base:wheat_seeds" */
  id: ItemId
  /** Human-readable display name. */
  name: string
  /** Maximum number of this item per inventory slot. */
  stackSize?: number
  /** Icon texture path relative to the pack assets folder. */
  icon?: TexturePath
  /** If set, using this item places the named block in the world. */
  placeableBlock?: BlockId
}

interface RecipeDef {
  /** Namespaced recipe id */
  id: RecipeId
  /** Recipe type (crafting, smelting, etc.) */
  type: RecipeType
  /** Output item id */
  output: ItemId
  /** Output count */
  count?: number
  /** List of ingredient item ids */
  ingredients?: ItemId[]
}

interface TagDef {
  /** Unique namespaced identifier. */
  id: TagId
  /** Optional human-readable note. */
  description?: string
  /** Namespaced ids contained in this tag. */
  members?: NamespacedId[]
}


// ── Utils ─────────────────────────────────────────────────────────────────────

declare const Utils: {
  clamp(value: number, min: number, max: number): number
  lerp(a: number, b: number, t: number): number
  remap(value: number, inMin: number, inMax: number, outMin: number, outMax: number): number
  hexToRgb(hex: string): RGB
  hslToRgb(h: number, s: number, l: number): RGB
  lerpRgb(a: RGB, b: RGB, t: number): RGB
  scaleRgb(rgb: RGB, factor: number): RGB
  parseId(id: NamespacedId): { namespace: string; path: string }
  makeId(namespace: string, path: string): NamespacedId
  isValidId(id: string): boolean
  climate(min: number, max: number): ClimateRange
  makeClimate(overrides?: Partial<BiomeClimate>): BiomeClimate
  drop(itemId: ItemId, count?: number): BlockDrop
}

declare const Logger: {
  info(...args: unknown[]): void
  warn(...args: unknown[]): void
  error(...args: unknown[]): void
  debug(...args: unknown[]): void
}

declare const console: {
  log(...args: unknown[]): void
  info(...args: unknown[]): void
  warn(...args: unknown[]): void
  error(...args: unknown[]): void
  debug(...args: unknown[]): void
}

declare const Platform: {
  isClient(): boolean
  isServer(): boolean
  isDevelopment(): boolean
  getGameVersion(): string
  isPackLoaded(id: string): boolean
}

declare const Resources: {
  exists(path: string): boolean
  readText(path: string): string | null
  readJson(path: string): unknown | null
  list(path: string): string[]
}

declare const Tags: {
  add(tag: TagId, member: NamespacedId): void
  remove(tag: TagId, member: NamespacedId): void
  has(tag: TagId, member: NamespacedId): boolean
}

declare const Recipes: {
  register(def: RecipeDef): void
  crafting(id: RecipeId, output: ItemId, ingredients: ItemId[] | ItemId, count?: number): void
  shapeless(id: RecipeId, output: ItemId, ingredients: ItemId[] | ItemId, count?: number): void
  smelting(id: RecipeId, output: ItemId, ingredient: ItemId | ItemId[], count?: number): void
}

declare const Localization: {
  add(locale: string, entries: Record<string, string>): void
  get(locale: string, key: string): string | null
}

declare const Data: {
  getBlock(id: BlockId): BlockDef | null
  getItem(id: ItemId): ItemDef | null
  getBiome(id: BiomeId): BiomeDef | null
  getTag(id: TagId): TagDef | null
  getRecipe(id: RecipeId): RecipeDef | null
  getLocalization(locale: string, key: string): string | null
}

declare const Timers: {
  setTimeout(callback: () => void, delayMs?: number): number
  setInterval(callback: () => void, intervalMs: number): number
  clear(id: number): void
}

declare const Commands: {
  register(name: string, handler: (ctx: { playerId: number; raw: string; name: string; args: string[] }) => string | string[] | void): void
  list(): string[]
}

declare const Models: {
  exists(path: string): boolean
  readText(path: string): string | null
  readJson(path: string): unknown | null
  list(path: string): string[]
}

// ── Globals ───────────────────────────────────────────────────────────────────

interface BlockBuilder {
  displayName(name: string): BlockBuilder
  hardness(value: number): BlockBuilder
  opacity(value: number): BlockBuilder
  color(r: number, g: number, b: number): BlockBuilder
  texture(pathOrObj: TexturePath | BlockTextures): BlockBuilder
  model(path: ModelPath): BlockBuilder
  renderType(type: BlockRenderType): BlockBuilder
  solid(value: boolean): BlockBuilder
  translucent(value: boolean): BlockBuilder
  tintKey(value: boolean): BlockBuilder
  material(value: BlockMaterial): BlockBuilder
  drops(entries: BlockDrop | BlockDrop[]): BlockBuilder
  states(states: Record<string, BlockStateProp>): BlockBuilder
  variants(variants: Record<string, BlockStateVariant>): BlockBuilder
  property(key: string, value: boolean | number | string): BlockBuilder
}

interface ItemBuilder {
  displayName(name: string): ItemBuilder
  stackSize(value: number): ItemBuilder
  icon(path: TexturePath): ItemBuilder
  placesBlock(blockId: BlockId): ItemBuilder
}

declare const StartupEvents: {
  registry(type: 'block', fn: (event: { create(id: BlockId): BlockBuilder }) => void): void
  registry(type: 'item', fn: (event: { create(id: ItemId): ItemBuilder }) => void): void
  registry(type: 'biome', fn: (event: { register(def: BiomeDef): void }) => void): void
  registry(type: 'tag', fn: (event: { register(idOrDef: TagId | TagDef): void }) => void): void
}

declare const Registry: {
  getBlock(id: BlockId): BlockDef | null
  getItem(id: ItemId):   ItemDef  | null
  getBiome(id: BiomeId): BiomeDef | null
  getTag(id: TagId):     TagDef   | null
}
