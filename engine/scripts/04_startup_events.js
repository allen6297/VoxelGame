/**
 * engine/scripts/04_startup_events.js
 *
 * StartupEvents — the sole public API for registering game content from scripts.
 * The other registration path is JSON files under each pack's blocks/ and items/
 * directories (loaded automatically by C++ before scripts run).
 *
 * ── Block / Item registration (fluent builder) ────────────────────────────────
 *
 *   StartupEvents.registry('block', event => {
 *     event.create('my_mod:copper_block')
 *       .displayName('Copper Block')
 *       .hardness(3.0)
 *       .color(0.72, 0.45, 0.20)
 *       .texture('textures/blocks/copper.ppm')
 *   })
 *
 *   StartupEvents.registry('item', event => {
 *     event.create('my_mod:raw_copper')
 *       .displayName('Raw Copper')
 *       .stackSize(64)
 *       .icon('textures/items/raw_copper.ppm')
 *   })
 *
 * ── Biome registration (object-based; schema is too rich for a fluent chain) ──
 *
 *   StartupEvents.registry('biome', event => {
 *     event.register({
 *       id: 'my_mod:coral_reef',
 *       name: 'Coral Reef',
 *       climate: { temperature: { min: 0.7, max: 1.0 }, … },
 *       …
 *     })
 *   })
 *
 * ── Tag registration ──────────────────────────────────────────────────────────
 *
 *   StartupEvents.registry('tag', event => {
 *     event.register('my_mod:metal')                          // id only
 *     event.register({ id: 'my_mod:ore', description: '…' }) // with description
 *   })
 *
 * ── Recipe helpers ───────────────────────────────────────────────────────────
 *
 *   Recipes.crafting('my_mod:plate', 'my_mod:plate', ['my_mod:ingot'])
 *   Recipes.smelting('my_mod:glass', 'my_mod:glass', 'my_mod:sand')
 *
 * All callbacks fire immediately (synchronously) when .registry() is called.
 * Builders / objects accumulate in a local list and are flushed to the C++
 * engine primitives (__registerBlock etc.) once the callback returns.
 *
 * This file is executed before any pack script.
 */

const StartupEvents = (() => {

  // ── Default-filling helpers (previously in 03_startup.js) ─────────────────

  function _blockDefaults(def) {
    const d = Object.assign({}, def)
    d.voxel = Object.assign({ solid: false, translucent: false, material: 'terrain' }, d.voxel)
    d.render = Object.assign({ color: [1, 1, 1], opacity: 1.0, type: 'cube' }, d.render)
    d.drops      = d.drops      ?? []
    d.properties = d.properties ?? {}
    return d
  }

  function _itemDefaults(def) {
    const d = Object.assign({}, def)
    d.stackSize = d.stackSize ?? 1
    d.icon      = d.icon      ?? ''
    return d
  }

  function _biomeDefaults(def) {
    const d = Object.assign({}, def)
    d.priority = d.priority ?? 0
    d.rarity   = d.rarity   ?? 1
    d.terrain  = Object.assign({ baseHeight: 48, heightVariation: 12 }, d.terrain)
    d.surface  = Object.assign(
      { top: 'base:grass', middle: 'base:dirt', base: 'base:stone', middleDepth: 3 },
      d.surface
    )
    d.atmosphere = Object.assign(
      { skyColor: [0.58, 0.78, 0.98], fogColor: [0.75, 0.85, 0.95], waterColor: [0.20, 0.45, 0.80] },
      d.atmosphere
    )
    d.fertility = Object.assign(
      { nitrogen: 0.5, phosphorus: 0.5, potassium: 0.5, magnesium: 0.5, calcium: 0.5, sulfur: 0.2 },
      d.fertility
    )
    d.colors = d.colors ?? {}
    return d
  }

  // ── Validation helpers ─────────────────────────────────────────────────────

  function _requireNamespacedId(type, id) {
    if (typeof id !== 'string' || !id.includes(':'))
      throw new Error(
        `StartupEvents.registry("${type}"): id must be a namespaced string (e.g. "my_mod:name"), got: ${JSON.stringify(id)}`
      )
  }

  // ── Block builder ──────────────────────────────────────────────────────────

  function BlockBuilder(id) {
    this._def = {
      id,
      name: id.split(':')[1]
               .replace(/_/g, ' ')
               .replace(/\b\w/g, ch => ch.toUpperCase()),
      voxel:      { solid: true, translucent: false, material: 'terrain' },
      render:     { color: [1, 1, 1], opacity: 1.0, type: 'cube' },
      drops:      [],
      properties: {},
    }
  }

  /** Override the human-readable display name. */
  BlockBuilder.prototype.displayName = function (name) {
    this._def.name = String(name); return this
  }

  /** Hardness — stored in properties.hardness (no first-class C++ field). */
  BlockBuilder.prototype.hardness = function (h) {
    this._def.properties.hardness = +h; return this
  }

  /** Opacity 0–1. */
  BlockBuilder.prototype.opacity = function (o) {
    this._def.render.opacity = +o; return this
  }

  /** Average/tint color as normalised [0–1] components. */
  BlockBuilder.prototype.color = function (r, g, b) {
    this._def.render.color = [+r, +g, +b]; return this
  }

  /**
   * Texture path(s).
   *   .texture('textures/blocks/stone.ppm')           → albedo only
   *   .texture({ albedo: '…', normal: '…', … })        → multiple maps
   */
  BlockBuilder.prototype.texture = function (pathOrObj) {
    this._def.render.texture = (typeof pathOrObj === 'string')
      ? { albedo: pathOrObj } : pathOrObj
    return this
  }

  /**
   * Set the model path used to derive face textures / geometry.
   * Does NOT change the render type — use .renderType('model') explicitly when
   * you want full model geometry (e.g. wheat, doors).  Cube-shaped blocks that
   * only use the model to derive per-face UV assignments should omit renderType.
   */
  BlockBuilder.prototype.model = function (path) {

    this._def.render.model = path
    return this
  }

  /** Explicitly set render type: 'cube' (default) or 'model'. */
  BlockBuilder.prototype.renderType = function (t) {
    this._def.render.type = t
    return this
  }

  BlockBuilder.prototype.solid       = function (s) { this._def.voxel.solid       = Boolean(s); return this }
  BlockBuilder.prototype.translucent = function (t) { this._def.voxel.translucent = Boolean(t); return this }
  BlockBuilder.prototype.tintKey     = function (t) { this._def.render.tintKey    = Boolean(t); return this }
  BlockBuilder.prototype.material    = function (m) { this._def.voxel.material    = String(m);  return this }
  BlockBuilder.prototype.runtimeOrder = function (n) { this._def.runtimeOrder      = +n;         return this }

  /**
   * Drop entries.
   *   .drops({ item: 'base:stone', count: 1 })
   *   .drops([{ item: 'base:stone', count: 1 }, …])
   */
  BlockBuilder.prototype.drops = function (entries) {
    this._def.drops = Array.isArray(entries) ? entries : [entries]; return this
  }

  /** Block-state properties and per-variant model overrides. */
  BlockBuilder.prototype.states   = function (s) { this._def.states   = s; return this }
  BlockBuilder.prototype.variants = function (v) { this._def.variants = v; return this }

  /**
   * Set a single arbitrary property (stored in block.properties map).
   *   .property('crop', true)
   *   .property('maxAge', 3)
   */
  BlockBuilder.prototype.property = function (key, value) {
    this._def.properties[key] = value; return this
  }

  // ── Item builder ───────────────────────────────────────────────────────────

  function ItemBuilder(id) {
    this._def = {
      id,
      name: id.split(':')[1]
               .replace(/_/g, ' ')
               .replace(/\b\w/g, ch => ch.toUpperCase()),
      stackSize: 64,
      icon:      '',
    }
  }

  ItemBuilder.prototype.displayName  = function (n) { this._def.name          = String(n); return this }
  ItemBuilder.prototype.stackSize    = function (n) { this._def.stackSize     = +n;         return this }
  ItemBuilder.prototype.icon         = function (p) { this._def.icon          = p;          return this }
  ItemBuilder.prototype.placesBlock  = function (b) { this._def.placeableBlock = b;          return this }

  // ── StartupEvents ──────────────────────────────────────────────────────────

  return {
    /**
     * Open a content-registry event for the given type and call fn with an
     * event object.  All content created/registered inside fn is flushed to
     * the engine once fn returns.
     *
     * @param {'block'|'item'|'biome'|'tag'} type
     * @param {function} fn
     */
    registry(type, fn) {

      // ── block ──────────────────────────────────────────────────────────────
      if (type === 'block') {
        const pending = []
        fn({
          create(id) {
            _requireNamespacedId('block', id)
            const b = new BlockBuilder(id)
            pending.push(b)
            return b
          }
        })
        for (const b of pending) __registerBlock(_blockDefaults(b._def))

      // ── item ───────────────────────────────────────────────────────────────
      } else if (type === 'item') {
        const pending = []
        fn({
          create(id) {
            _requireNamespacedId('item', id)
            const it = new ItemBuilder(id)
            pending.push(it)
            return it
          }
        })
        for (const it of pending) __registerItem(_itemDefaults(it._def))

      // ── biome ──────────────────────────────────────────────────────────────
      // Biomes have a rich nested schema; an object literal is clearer than a
      // fluent chain, so we expose event.register(def) instead of event.create(id).
      } else if (type === 'biome') {
        const pending = []
        fn({
          register(def) {
            if (!def || typeof def !== 'object')
              throw new Error('StartupEvents.registry("biome"): expected an object')
            _requireNamespacedId('biome', def.id)
            pending.push(def)
          }
        })
        for (const def of pending) __registerBiome(_biomeDefaults(def))

      // ── tag ────────────────────────────────────────────────────────────────
      } else if (type === 'tag') {
        const pending = []
        fn({
          register(idOrDef) {
            if (typeof idOrDef === 'string') {
              _requireNamespacedId('tag', idOrDef)
            } else {
              if (!idOrDef || typeof idOrDef !== 'object')
                throw new Error('StartupEvents.registry("tag"): expected a string id or { id, description } object')
              _requireNamespacedId('tag', idOrDef.id)
            }
            pending.push(idOrDef)
          }
        })
        for (const t of pending) __registerTag(t)

      } else {
        throw new Error(
          `StartupEvents.registry: unknown type "${type}". Supported: "block", "item", "biome", "tag"`
        )
      }
    }
  }

})()

const Recipes = (() => {
  function register(def) {
    __registerRecipe(def)
  }

  function normalizeIngredients(ingredients) {
    return Array.isArray(ingredients) ? ingredients : [ingredients]
  }

  return {
    register,
    crafting(id, output, ingredients, count = 1) {
      register({ id, type: 'crafting', output, count, ingredients: normalizeIngredients(ingredients) })
    },
    shapeless(id, output, ingredients, count = 1) {
      register({ id, type: 'crafting', output, count, ingredients: normalizeIngredients(ingredients) })
    },
    smelting(id, output, ingredient, count = 1) {
      register({ id, type: 'smelting', output, count, ingredients: normalizeIngredients(ingredient) })
    },
  }
})()
