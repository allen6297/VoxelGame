/**
 * packs/example_pack/scripts/startup/startup.js
 *
 * Example startup script showing both APIs:
 *  1. StartupEvents.registry — builder-style block/item creation
 *  2. Registry.modifyBlock   — patch a block registered by JSON or another script
 */

// ── Create new blocks via the builder API ────────────────────────────────────

Logger.info('Example Pack startup loading on', Platform.isClient() ? 'client' : 'server')
Logger.info('Base pack loaded?', Platform.isPackLoaded('base'))
Logger.info('Startup scripts visible:', Resources.list('scripts/startup').length)

Localization.add('en_us', {
  'item.example_pack.tin_ingot': 'Tin Ingot',
  'recipe.example_pack.smelt_raw_tin': 'Smelt Raw Tin',
})
Logger.info('Tin ingot label:', Localization.get('en_us', 'item.example_pack.tin_ingot'))

StartupEvents.registry('tag', event => {
  event.register({ id: 'example_pack:demo_blocks', description: 'Blocks added by the example pack' })
})

Tags.add('example_pack:demo_blocks', 'example_pack:tin_ore')
Tags.add('example_pack:demo_blocks', 'example_pack:tin_block')
Tags.remove('example_pack:demo_blocks', 'example_pack:tin_block')
Logger.info('Tin ore tagged?', Tags.has('example_pack:demo_blocks', 'example_pack:tin_ore'))
Logger.info('Tin block tagged?', Tags.has('example_pack:demo_blocks', 'example_pack:tin_block'))

StartupEvents.registry('block', event => {
  // Fluent builder: each method returns `this` so calls can be chained.
  event.create('example_pack:tin_ore')
    .displayName('Tin Ore')
    .hardness(2.5)
    .color(0.8, 0.8, 0.75)
    .texture('textures/blocks/tin_ore.ppm')

  event.create('example_pack:tin_block')
    .displayName('Tin Block')
    .hardness(4.0)
    .color(0.75, 0.75, 0.7)
    .texture('textures/blocks/tin_block.ppm')
})

// ── Create new items ─────────────────────────────────────────────────────────

StartupEvents.registry('item', event => {
  event.create('example_pack:raw_copper')
    .displayName('Raw Copper')
    .stackSize(64)
    .icon('textures/blocks/stone.ppm')

  event.create('example_pack:raw_tin')
    .displayName('Raw Tin')
    .stackSize(64)
    .icon('textures/blocks/dirt.ppm')

  event.create('example_pack:tin_ingot')
    .displayName('Tin Ingot')
    .stackSize(64)
    .icon('textures/blocks/stone.ppm')
})

Recipes.smelting('example_pack:smelt_raw_tin', 'example_pack:tin_ingot', 'example_pack:raw_tin', 1)

// ── Modify copper_ore (loaded from blocks/copper_ore.json) ───────────────────
//
// Registry.modifyBlock runs after all JSON files and earlier scripts have been
// processed, so it can override anything set by JSON or another pack.

Registry.modifyBlock('example_pack:copper_ore', block => {
  block.displayName = 'Dense Copper Ore'   // "displayName" is an alias for name
  block.hardness    = 4.0                  // stored in block.properties.hardness
})
