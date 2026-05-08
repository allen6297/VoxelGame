// Schema for BlockDefinition / BlockDef
// Each field: { cpp, jsPath, type, default?, required?, validator?, cppType?, parser?, dtsType?, dtsGroup?, dtsKey?, doc? }
//
// type:     'string' | 'enum' | 'bool' | 'int' | 'float' | 'rgb' | 'string?' | 'custom'
// jsPath:   dot-separated path into the JS object  (e.g. 'render.color')
// cpp:      dot-separated path into the C++ struct (e.g. 'color')
// dtsGroup: which sub-interface this field lives in for the .d.ts output
// dtsKey:   property name inside that sub-interface (defaults to last segment of jsPath)

module.exports = {
  cppStruct:    'BlockDefinition',
  dtsInterface: 'BlockDef',

  // Sub-interfaces generated for the .d.ts
  dtsGroups: {
    voxel:  { name: 'BlockVoxel',  doc: 'Physics and voxel-space properties.' },
    render: { name: 'BlockRender', doc: 'Visual and rendering configuration.' },
  },

  fields: [
    // ── Top-level ────────────────────────────────────────────────────────────
    {
      cpp: 'id',   jsPath: 'id',   type: 'string', required: true,
      validator: 'block_id', dtsType: 'BlockId',
      doc: 'Unique namespaced identifier. @example "base:grass"',
    },
    {
      cpp: 'name', jsPath: 'name', type: 'string', required: true,
      doc: 'Human-readable display name.',
    },
    {
      cpp: 'runtimeOrder', jsPath: 'runtimeOrder', type: 'int', default: 1000,
      doc: 'Ordering hint for runtime ID assignment. Lower values are assigned earlier.',
    },

    // ── voxel group ──────────────────────────────────────────────────────────
    {
      cpp: 'solid',       jsPath: 'voxel.solid',       type: 'bool',   default: false,
      dtsGroup: 'voxel',  doc: 'Whether this block has a solid collision box.',
    },
    {
      cpp: 'translucent', jsPath: 'voxel.translucent',  type: 'bool',   default: false,
      dtsGroup: 'voxel',  doc: 'Whether light passes through this block.',
    },
    {
      cpp: 'material',    jsPath: 'voxel.material',     type: 'enum',
      values: ['terrain', 'rock', 'liquid', 'plant'], default: 'terrain',
      dtsGroup: 'voxel', dtsType: 'BlockMaterial',
      doc: 'Physics/sound material group. @example "terrain" "rock" "liquid" "plant"',
    },

    // ── render group ─────────────────────────────────────────────────────────
    {
      cpp: 'color',      jsPath: 'render.color',   type: 'rgb',    default: [1, 1, 1],
      dtsGroup: 'render', validator: 'rgb_0_1', doc: 'Base tint colour (linear RGB). Applied when tintKey is false.',
    },
    {
      cpp: 'opacity',    jsPath: 'render.opacity', type: 'float',  default: 1.0,
      dtsGroup: 'render', validator: 'opacity_0_1', doc: 'Alpha opacity. 1 = fully opaque, 0 = invisible.',
    },
    {
      cpp: 'tintKey',    jsPath: 'render.tintKey', type: 'bool',   default: false,
      dtsGroup: 'render', doc: 'When true the engine uses the biome tint colour instead of color.',
    },
    {
      cpp: 'renderType', jsPath: 'render.type',    type: 'enum', values: ['cube', 'model'], default: 'cube',
      dtsGroup: 'render', dtsKey: 'type', dtsType: 'BlockRenderType',
      doc: 'Render geometry type. "cube" = full block, "model" = custom model.',
    },
    {
      cpp: 'modelPath',  jsPath: 'render.model',   type: 'string', default: '',
      dtsGroup: 'render', dtsKey: 'model', dtsType: 'ModelPath', validator: 'model_path',
      doc: 'Path to the block model JSON, relative to the pack assets folder.',
    },

    // ── render group — custom fields ──────────────────────────────────────────
    {
      cpp: 'textures',   jsPath: 'render.texture', type: 'object',
      cppType: 'BlockTextures', cppDefault: '{}',
      parser: 'parseBlockTextures',
      dtsGroup: 'render', dtsKey: 'texture', dtsType: 'TexturePath | BlockTextures',
      doc: 'Texture paths. Pass a string to set the albedo only, or an object for named maps.',
    },

    // ── Top-level — custom fields ─────────────────────────────────────────────
    {
      cpp: 'drops', jsPath: 'drops', type: 'custom',
      cppType: 'std::vector<BlockDrop>', cppDefault: '{}',
      parser: 'parseBlockDrops',
      dtsType: 'BlockDrop[]',
      doc: 'Items dropped when this block is broken.',
    },
    {
      cpp: 'properties', jsPath: 'properties', type: 'custom',
      cppType: 'std::unordered_map<std::string, BlockProperty>', cppDefault: '{}',
      parser: 'parseBlockProperties',
      dtsType: 'Record<string, boolean | number | string>',
      doc: 'Arbitrary key–value properties accessible from gameplay code.',
    },
  ],
}
