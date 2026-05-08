#!/usr/bin/env node
/**
 * tools/codegen/generate.js
 *
 * Reads schema files and emits:
 *   include/common/pack/generated/ParseBindings.hpp  — parse function declarations
 *   src/common/pack/generated/ParseBindings.cpp      — parse function implementations
 *   packs/types/voxel.d.ts                    — TypeScript type declarations
 *
 * Usage:
 *   node tools/codegen/generate.js
 */

'use strict'

const fs = require('fs')
const path = require('path')

const ROOT = path.join(__dirname, '../..')
const SCHEMA_DIR = path.join(__dirname, 'schema')
const SCHEMAS = fs
    .readdirSync(SCHEMA_DIR)
    .filter(f => f.endsWith('.js'))
    .sort()
    .map(f => require(path.join(SCHEMA_DIR, f)))
const SCHEMA_SOURCE_LABEL = `tools/codegen/schema/{${fs
    .readdirSync(SCHEMA_DIR)
    .filter(f => f.endsWith('.js'))
    .map(f => path.basename(f, '.js'))
    .sort()
    .join(',')}}.js`

// ── Type mappings ─────────────────────────────────────────────────────────────

const CPP_TYPES = {
    string: 'std::string',
    enum: 'std::string',
    bool: 'bool',
    int: 'int',
    float: 'float',
    rgb: 'std::array<float, 3>',
    'string?': 'std::optional<std::string>',
}

const DTS_TYPES = {
    string: 'string',
    array: 'unknown[]',
    object: 'Record<string, unknown>',
    bool: 'boolean',
    int: 'number',
    float: 'number',
    rgb: 'RGB',
    'string?': 'string | undefined',
}

// ── Helpers ───────────────────────────────────────────────────────────────────

function fmtFloat(n) {
    return Number.isInteger(n) ? `${n}.0f` : `${n}f`
}

function fmtDefault(type, def) {
    if (def === undefined) return null
    switch (type) {
        case 'string':
        case 'enum':
            return `"${def}"`
        case 'bool':
            return def ? 'true' : 'false'
        case 'int':
            return String(def)
        case 'float':
            return fmtFloat(def)
        case 'rgb':
            return `{${def.map(fmtFloat).join(', ')}}`
        case 'string?':
            return 'std::nullopt'
        default:
            return null
    }
}

// Build the C++ read expression for a simple (non-custom) field.
function dtsEnumType(values) {
    return values.map(v => `'${v}'`).join(' | ')
}

function cppEnumValues(values) {
    return `{${values.map(v => `"${v}"`).join(', ')}}`
}

function cppRequiredCheck(obj, jsKey) {
    return `voxel::js::jsRequire(ctx, ${obj}, "${jsKey}")`
}

function cppValidatorCheck(field, jsPath) {
    return `voxel::js::jsValidate(ctx, "${jsPath}", out.${field.cpp}, "${field.validator}")`
}

function cppReadExpr(field, obj, jsKey) {
    const type = field.type
    const def = field.default
    const d = fmtDefault(type, def)
    const dflt = d !== null ? `, ${d}` : ''
    switch (type) {
        case 'string':
            return `voxel::js::jsStr  (ctx, ${obj}, "${jsKey}"${dflt})`
        case 'enum':
            return `voxel::js::jsEnum (ctx, ${obj}, "${jsKey}", ${cppEnumValues(field.values)}, ${d ?? '""'})`
        case 'array':
            if (field.elementType === 'string') return `voxel::js::jsStringArray(ctx, ${obj}, "${jsKey}")`
            throw new Error(`Unsupported array elementType for ${field.jsPath}: ${field.elementType}`)
        case 'object':
            return `${field.parser}(ctx, ${obj})`
        case 'bool':
            return `voxel::js::jsBool (ctx, ${obj}, "${jsKey}"${dflt})`
        case 'int':
            return `voxel::js::jsInt  (ctx, ${obj}, "${jsKey}"${dflt})`
        case 'float':
            return `voxel::js::jsFloat(ctx, ${obj}, "${jsKey}"${dflt})`
        case 'rgb':
            return `voxel::js::jsColor(ctx, ${obj}, "${jsKey}"${dflt})`
        case 'string?':
            return `voxel::js::jsOptStr(ctx, ${obj}, "${jsKey}")`
    }
}

// Group fields by their top-level JS sub-object (first segment of jsPath).
// Returns { topLevel: [...], groups: { prefix: [...fields with jsKey set] } }
function groupFields(fields) {
    const topLevel = []
    const groups = {}

    for (const f of fields) {
        const parts = f.jsPath.split('.')
        if (parts.length === 1) {
            topLevel.push(f)
        } else {
            const prefix = parts[0]
            const jsKey = parts.slice(1).join('.')
            if (!groups[prefix]) groups[prefix] = []
            groups[prefix].push({...f, jsKey})
        }
    }

    return {topLevel, groups}
}

// ── C++ generation ────────────────────────────────────────────────────────────

function genParseBody(schema) {
    const {topLevel, groups} = groupFields(schema.fields)
    const lines = []
    const indent = '    '

    const writeLine = (l = '') => lines.push(l ? indent + l : '')

    writeLine(`${schema.cppStruct} out{};`)
    writeLine()

    // Top-level fields
    for (const f of topLevel) {
        if (f.required) {
            writeLine(cppRequiredCheck('obj', f.jsPath) + ';')
        }
        if (f.type === 'custom') {
            writeLine(`out.${f.cpp} = ${f.parser}(ctx, obj);`)
        } else {
            writeLine(`out.${f.cpp} = ${cppReadExpr(f, 'obj', f.jsPath)};`)
        }
        if (f.validator) {
            writeLine(cppValidatorCheck(f, f.jsPath) + ';')
        }
    }

    // Sub-object groups
    for (const [prefix, fields] of Object.entries(groups)) {
        writeLine()
        writeLine(`// ${prefix}`)
        writeLine('{')
        writeLine(`    JSValue sub = JS_GetPropertyStr(ctx, obj, "${prefix}");`)
        for (const f of fields) {
            if (f.required) {
                writeLine('    ' + cppRequiredCheck('sub', f.jsKey) + ';')
            }
            if (f.type === 'custom') {
                const keyArg = f.parserPassKey ? `, "${f.jsKey}"` : ''
                writeLine(`    out.${f.cpp} = ${f.parser}(ctx, sub${keyArg});`)
            } else {
                writeLine(`    out.${f.cpp} = ${cppReadExpr(f, 'sub', f.jsKey)};`)
            }
            if (f.validator) {
                writeLine('    ' + cppValidatorCheck(f, f.jsPath) + ';')
            }
        }
        writeLine('    JS_FreeValue(ctx, sub);')
        writeLine('}')
    }

    writeLine()
    writeLine('return out;')

    return lines.join('\n')
}

function genCppSource(schemas) {
    const banner = [
        '// GENERATED FILE — do not edit by hand.',
        `// Source:      ${SCHEMA_SOURCE_LABEL}`,
        '// Regenerate:  node tools/codegen/generate.js',
        '',
    ].join('\n')

    const includes = [
        '#include "pack/generated/ParseBindings.hpp"',
        '#include "pack/JsHelpers.hpp"',
        '#include "data/GameData.hpp"',
        '#include "data/BiomeDefinition.hpp"',
        '#include <quickjs.h>',
        '',
    ].join('\n')

    // Forward-declare every custom parser used across all schemas.
    const customParsers = new Set()
    for (const s of schemas)
        for (const f of s.fields)
            if (f.type === 'custom') customParsers.add(f.parser)

    // Build extern declarations for each unique custom parser signature.
    // parserPassKey fields get an extra `const char*` argument.
    const parserSigs = new Map()
    for (const s of schemas) {
        for (const f of s.fields) {
            if (f.type !== 'custom' && f.type !== 'object') continue
            if (parserSigs.has(f.parser)) continue
            const extraArg = f.parserPassKey ? ', const char*' : ''
            parserSigs.set(f.parser, `extern ${f.cppType} ${f.parser}(JSContext*, JSValueConst${extraArg});`)
        }
    }

    const fwdDecls = [
        'namespace voxel {',
        '// Custom parsers — defined in ScriptBindingsCustom.cpp',
        ...[...parserSigs.values()],
        '',
        'namespace generated {',
        '',
    ].join('\n')

    const bodies = schemas.map(s => [
        `${s.cppStruct} parse${s.cppStruct}(JSContext* ctx, JSValueConst obj) {`,
        genParseBody(s),
        '}',
        '',
    ].join('\n')).join('\n')

    const footer = '} // namespace generated\n} // namespace voxel\n'

    return [banner, includes, fwdDecls, bodies, footer].join('\n')
}

function genCppHeader(schemas) {
    const banner = [
        '// GENERATED FILE — do not edit by hand.',
        '// Regenerate:  node tools/codegen/generate.js',
        '',
    ].join('\n')

    const guard = [
        '#pragma once',
        '#include "data/GameData.hpp"',
        '#include "data/BiomeDefinition.hpp"',
        '#include <quickjs.h>',
        '',
        'namespace voxel::generated {',
        '',
    ].join('\n')

    const decls = schemas.map(s =>
        `${s.cppStruct} parse${s.cppStruct}(JSContext* ctx, JSValueConst obj);`
    ).join('\n')

    const footer = '\n} // namespace voxel::generated\n'

    return [banner, guard, decls, footer].join('\n')
}

// ── TypeScript generation ─────────────────────────────────────────────────────

function dtsDoc(doc, indent = '') {
    if (!doc) return ''
    return `${indent}/** ${doc} */\n`
}

function dtsOptional(f) {
    return f.required ? '' : '?'
}

function dtsFieldType(f) {
    if (f.dtsType) return f.dtsType
    if (f.type === 'enum') return dtsEnumType(f.values)
    if (f.type === 'array') {
        const elementType = f.elementDtsType ?? DTS_TYPES[f.elementType] ?? 'unknown'
        return `${elementType}[]`
    }
    return DTS_TYPES[f.type] ?? 'unknown'
}

function genDtsGroupInterface(groupKey, groupDef, fields) {
    const members = fields.map(f => {
        const key = f.dtsKey ?? f.jsPath.split('.').pop()
        const opt = dtsOptional(f)
        const type = dtsFieldType(f)
        const docLine = f.doc ? `  /** ${f.doc} */\n` : ''
        return `${docLine}  ${key}${opt}: ${type}`
    }).join('\n')

    return [
        dtsDoc(groupDef.doc),
        `interface ${groupDef.name} {`,
        members,
        '}',
        '',
    ].join('\n')
}

function genDtsMainInterface(schema) {
    const groupFields = {}
    const rootFields = []

    for (const f of schema.fields) {
        if (f.dtsGroup) {
            if (!groupFields[f.dtsGroup]) groupFields[f.dtsGroup] = []
            groupFields[f.dtsGroup].push(f)
        } else {
            rootFields.push(f)
        }
    }

    const members = []

    // Root-level fields
    for (const f of rootFields) {
        if (f.doc) members.push(`  /** ${f.doc} */`)
        const opt = dtsOptional(f)
        const type = dtsFieldType(f)
        members.push(`  ${f.cpp}${opt}: ${type}`)
    }

    // Sub-interface fields
    for (const [groupKey, groupDef] of Object.entries(schema.dtsGroups ?? {})) {
        const groupName = groupDef.name
        members.push(`  ${groupKey}?: ${groupName}`)
    }

    return [
        `interface ${schema.dtsInterface} {`,
        members.join('\n'),
        '}',
        '',
    ].join('\n')
}

function genDts(schemas) {
    const banner = [
        '/**',
        ' * Voxel Game — Pack Scripting API',
        ' *',
        ' * GENERATED FILE — do not edit by hand.',
        ` * Source:      ${SCHEMA_SOURCE_LABEL}`,
        ' * Regenerate:  node tools/codegen/generate.js   (or: cmake --build . --target generate_bindings)',
        ' */',
        '',
    ].join('\n')

    const primitives = `
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
`

    // Sub-interfaces
    const subInterfaces = []
    for (const schema of schemas) {
        for (const [groupKey, groupDef] of Object.entries(schema.dtsGroups ?? {})) {
            const fields = schema.fields.filter(f => f.dtsGroup === groupKey)
            subInterfaces.push(genDtsGroupInterface(groupKey, groupDef, fields))
        }
    }

    // Main definition interfaces
    const mainInterfaces = schemas.map(s => {
        // Append states/variants to BlockDef manually (too complex to schema-ise)
        let iface = genDtsMainInterface(s)
        if (s.cppStruct === 'BlockDefinition') {
            iface = iface.replace(
                /^(interface BlockDef \{)/,
                '$1\n  states?:   Record<string, BlockStateProp>\n  variants?: Record<string, BlockStateVariant>'
            )
        }
        return iface
    })

    const utils = `
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
`

    return [banner, primitives, ...subInterfaces, ...mainInterfaces, utils].join('\n')
}

// ── Write output files ────────────────────────────────────────────────────────

function write(filePath, content) {
    fs.mkdirSync(path.dirname(filePath), {recursive: true})
    fs.writeFileSync(filePath, content, 'utf8')
    console.log(`  wrote ${path.relative(ROOT, filePath)}`)
}

function schemaFieldRows(schema) {
    return schema.fields.map(f => {
        const type = f.dtsType ?? (f.type === 'enum' ? dtsEnumType(f.values) : f.type)
        const required = f.required ? 'yes' : 'no'
        const def = f.default === undefined ? '' : JSON.stringify(f.default)
        return `| \`${f.jsPath}\` | \`${type}\` | ${required} | ${def} | ${f.validator ?? ''} | ${f.doc ?? ''} |`
    }).join('\n')
}

function genMarkdownDocs(schemas) {
    const sections = schemas.map(s => [
        `## ${s.dtsInterface}`,
        '',
        '| Field | Type | Required | Default | Validator | Description |',
        '| --- | --- | --- | --- | --- | --- |',
        schemaFieldRows(s),
        '',
    ].join('\n'))

    return [
        '<!-- GENERATED FILE - do not edit by hand. -->',
        '# Pack Schema Reference',
        '',
        `Source: \`${SCHEMA_SOURCE_LABEL}\``,
        '',
        ...sections,
    ].join('\n')
}

function jsonSchemaType(f) {
    if (f.type === 'enum') return {type: 'string', enum: f.values}
    if (f.type === 'array') return {type: 'array', items: {type: f.elementType ?? 'string'}}
    if (f.type === 'object' || f.type === 'custom') return {type: ['object', 'array', 'string']}
    if (f.type === 'bool') return {type: 'boolean'}
    if (f.type === 'int') return {type: 'integer'}
    if (f.type === 'float') return {type: 'number'}
    if (f.type === 'rgb') return {type: 'array', items: {type: 'number'}, minItems: 3, maxItems: 3}
    return {type: 'string'}
}

function assignNestedProperty(root, pathText, schema) {
    const parts = pathText.split('.')
    let cursor = root
    for (let i = 0; i < parts.length - 1; ++i) {
        const part = parts[i]
        cursor.properties ??= {}
        cursor.properties[part] ??= {type: 'object', properties: {}}
        cursor = cursor.properties[part]
    }
    cursor.properties ??= {}
    cursor.properties[parts[parts.length - 1]] = schema
}

function genJsonSchema(schemas) {
    const definitions = {}
    for (const schema of schemas) {
        const def = {type: 'object', properties: {}, required: []}
        for (const field of schema.fields) {
            assignNestedProperty(def, field.jsPath, {
                ...jsonSchemaType(field),
                description: field.doc,
                default: field.default,
            })
            if (field.required) def.required.push(field.jsPath)
        }
        if (def.required.length === 0) delete def.required
        definitions[schema.dtsInterface] = def
    }
    return JSON.stringify({
        $schema: 'https://json-schema.org/draft/2020-12/schema',
        $id: 'https://terralite.local/pack.schema.json',
        title: 'TERRALITE Pack Schema',
        definitions,
    }, null, 2) + '\n'
}

function genSchemaDump(schemas) {
    return JSON.stringify(schemas, null, 2) + '\n'
}

function genSnippets() {
    return JSON.stringify({
        'TERRALITE block registration': {
            prefix: 'tl-block',
            body: [
                "StartupEvents.registry('block', event => {",
                "  event.create('${1:pack:block_id}')",
                "    .displayName('${2:Block Name}')",
                "    .solid(true)",
                "    .model('${3:models/blocks/block.json}')",
                "    .texture('${4:textures/blocks/block.ppm}')",
                "})",
            ],
            description: 'Register a TERRALITE block.',
        },
        'TERRALITE item registration': {
            prefix: 'tl-item',
            body: [
                "StartupEvents.registry('item', event => {",
                "  event.create('${1:pack:item_id}')",
                "    .displayName('${2:Item Name}')",
                "    .stackSize(${3:64})",
                "    .icon('${4:textures/items/item.ppm}')",
                "})",
            ],
            description: 'Register a TERRALITE item.',
        },
        'TERRALITE startup log': {
            prefix: 'tl-log',
            body: [
                "Logger.info('Loading ${1:pack_name}...')",
                "if (Platform.isDevelopment()) {",
                "  Logger.debug('Development build detected')",
                "}",
            ],
            description: 'Log a simple startup message.',
        },
    }, null, 2) + '\n'
}

console.log('codegen: generating bindings...')
write(path.join(ROOT, 'include/common/pack/generated/ParseBindings.hpp'), genCppHeader(SCHEMAS))
write(path.join(ROOT, 'src/common/pack/generated/ParseBindings.cpp'), genCppSource(SCHEMAS))
write(path.join(ROOT, 'packs/types/voxel.d.ts'), genDts(SCHEMAS))
write(path.join(ROOT, 'documentation/PackSchema.md'), genMarkdownDocs(SCHEMAS))
write(path.join(ROOT, 'packs/types/pack.schema.json'), genJsonSchema(SCHEMAS))
write(path.join(ROOT, 'packs/types/schema-dump.json'), genSchemaDump(SCHEMAS))
write(path.join(ROOT, 'packs/types/voxel.code-snippets'), genSnippets())
console.log('codegen: done.')
