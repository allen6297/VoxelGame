// Schema for TagDefinition / TagDef
module.exports = {
  cppStruct:    'TagDefinition',
  dtsInterface: 'TagDef',
  dtsGroups:    {},

  fields: [
    {
      cpp: 'id', jsPath: 'id', type: 'string', required: true,
      validator: 'tag_id', dtsType: 'TagId',
      doc: 'Unique namespaced identifier.',
    },
    {
      cpp: 'description', jsPath: 'description', type: 'string', default: '',
      doc: 'Optional human-readable note.',
    },
    {
      cpp: 'members', jsPath: 'members', type: 'array',
      elementType: 'string', dtsType: 'NamespacedId[]',
      doc: 'Namespaced ids contained in this tag.',
    },
  ],
}
