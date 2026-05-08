#include "pack/ScriptManager.hpp"
#include "pack/PackManager.hpp"
#include "pack/Pack.hpp"
#include "pack/JsHelpers.hpp"
#include "pack/generated/ParseBindings.hpp"
#include "data/GameData.hpp"
#include "data/BiomeDefinition.hpp"
#include "data/JsonValue.hpp"
#include "world/WorldSimulation.hpp"
#include "Player.hpp"
#include <quickjs.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace {
static std::string readFile(const std::filesystem::path& path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("ScriptManager: cannot open " + path.string());
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::string jsValueToString(JSContext* ctx, JSValueConst value) {
    const char* text = JS_ToCString(ctx, value);
    std::string result = text ? text : "";
    JS_FreeCString(ctx, text);
    return result;
}

std::string getJsStringProperty(JSContext* ctx, JSValueConst value, const char* key) {
    JSValue prop = JS_GetPropertyStr(ctx, value, key);
    if (JS_IsUndefined(prop) || JS_IsNull(prop)) {
        JS_FreeValue(ctx, prop);
        return {};
    }
    std::string result = jsValueToString(ctx, prop);
    JS_FreeValue(ctx, prop);
    return result;
}

std::optional<std::pair<int, int>> extractLocation(const std::string& stack, const std::string& needle) {
    const std::size_t pos = stack.find(needle);
    if (pos == std::string::npos) {
        return std::nullopt;
    }

    std::size_t cursor = pos + needle.size();
    if (cursor >= stack.size() || stack[cursor] != ':') {
        return std::nullopt;
    }
    ++cursor;

    const std::size_t lineStart = cursor;
    while (cursor < stack.size() && std::isdigit(static_cast<unsigned char>(stack[cursor]))) {
        ++cursor;
    }
    if (cursor == lineStart || cursor >= stack.size() || stack[cursor] != ':') {
        return std::nullopt;
    }
    const int line = std::stoi(stack.substr(lineStart, cursor - lineStart));
    ++cursor;

    const std::size_t columnStart = cursor;
    while (cursor < stack.size() && std::isdigit(static_cast<unsigned char>(stack[cursor]))) {
        ++cursor;
    }
    if (cursor == columnStart) {
        return std::make_pair(line, 0);
    }

    return std::make_pair(line, std::stoi(stack.substr(columnStart, cursor - columnStart)));
}

std::string sourceSnippet(const std::string& source, const int lineNumber, const int columnNumber) {
    if (lineNumber <= 0 || source.empty()) {
        return {};
    }

    int currentLine = 1;
    std::size_t lineStart = 0;
    for (std::size_t i = 0; i <= source.size(); ++i) {
        if (i == source.size() || source[i] == '\n') {
            if (currentLine == lineNumber) {
                const std::size_t lineEnd = i;
                std::string snippet = source.substr(lineStart, lineEnd - lineStart);
                if (columnNumber > 0) {
                    snippet += "\n";
                    snippet.append(static_cast<std::size_t>(columnNumber - 1), ' ');
                    snippet += "^";
                }
                return snippet;
            }
            ++currentLine;
            lineStart = i + 1;
        }
    }

    return {};
}

const char* phaseName(const voxel::ScriptPhase phase) {
    return phase == voxel::ScriptPhase::Startup ? "startup" : "runtime";
}
}  // namespace

namespace voxel {

// ── Constructor / Destructor ─────────────────────────────────────────────────

ScriptManager::ScriptManager() {
    runtime_ = JS_NewRuntime();
    if (!runtime_) throw std::runtime_error("Failed to create QuickJS runtime");

    context_ = JS_NewContext(runtime_);
    if (!context_) {
        JS_FreeRuntime(runtime_);
        runtime_ = nullptr;
        throw std::runtime_error("Failed to create QuickJS context");
    }

    JS_SetContextOpaque(context_, this);
    setupGlobals();
}

ScriptManager::~ScriptManager() {
    if (context_) JS_FreeContext(context_);
    if (runtime_) JS_FreeRuntime(runtime_);
}

// ── loadGameData ─────────────────────────────────────────────────────────────

GameData ScriptManager::loadGameData(const PackManager& packManager,
                                     const std::filesystem::path& engineScriptsPath) {
    engineScriptsPath_ = engineScriptsPath;
    currentPhase_ = ScriptPhase::Startup;

    // Execute engine scripts first (defines StartupEvents, Registry, etc.) in sorted order.
    if (std::filesystem::is_directory(engineScriptsPath)) {
        std::vector<std::filesystem::path> engineFiles;
        for (const auto& entry : std::filesystem::directory_iterator(engineScriptsPath)) {
            if (entry.path().extension() == ".js")
                engineFiles.push_back(entry.path());
        }
        std::sort(engineFiles.begin(), engineFiles.end());
        for (const auto& path : engineFiles) {
            if (path.filename() == "05_runtime.js") {
                continue;
            }
            executeScript(readFile(path), "<engine>/" + path.filename().string(), ScriptPhase::Startup);
        }
    } else {
        std::cerr << "[ScriptManager] Warning: engine scripts path not found: "
                  << engineScriptsPath << '\n';
    }

    const auto& packs = packManager.packs();

    // Process in reverse priority order (base first, highest-priority pack last)
    // so higher-priority packs can override registrations made by lower ones.
    for (auto it = packs.rbegin(); it != packs.rend(); ++it) {
        const Pack& pack = *it;

        // ── 1. Load JSON block/item/recipe definitions ───────────────────────
        // JSON definitions are loaded before pack scripts so that startup.js
        // can call Registry.modifyBlock on any JSON-defined block.
        loadPackJsonBlocks(pack);
        loadPackJsonItems(pack);
        loadPackJsonRecipes(pack);

        // ── 2. Run startup scripts ────────────────────────────────────────────
        if (pack.manifest().scripts.startup) {
            if (pack.hasFile("scripts/startup/main.js")) {
                auto src = pack.readFile("scripts/startup/main.js");
                if (src) {
                    executeScript(*src, pack.id() + ":scripts/startup/main.js", ScriptPhase::Startup);
                }
            } else {
                // Execute every .js file in scripts/startup/ in alphabetical order.
                auto files = pack.listFiles("scripts/startup");
                std::sort(files.begin(), files.end());
                for (const auto& f : files) {
                    if (f.size() > 3 && f.compare(f.size() - 3, 3, ".js") == 0) {
                        auto src = pack.readFile(f);
                        if (src) {
                            executeScript(*src, pack.id() + ":" + f, ScriptPhase::Startup);
                        }
                    }
                }
            }
        }
    }

    GameData data;
    auto insertUnique = [](auto& map, auto&& value, const char* kind) {
        const std::string id = value.id;
        auto [it, inserted] = map.emplace(id, std::move(value));
        if (!inserted) {
            throw std::runtime_error(std::string("[Pack Validation Error]\nDuplicate ") + kind + " id: " + id);
        }
    };

    for (auto& b  : pendingBlocks_)      insertUnique(data.blocks,       std::move(b),  "block");
    for (auto& i  : pendingItems_)       insertUnique(data.items,        std::move(i),  "item");
    for (auto& bi : pendingBiomes_)      insertUnique(data.biomes,       std::move(bi), "biome");
    for (auto& t  : pendingTags_)        data.tags.insert_or_assign(t.id, std::move(t));
    for (auto& bs : pendingBlockStates_) insertUnique(data.blockStates,   std::move(bs), "block state");
    for (auto& r  : pendingRecipes_)     insertUnique(data.recipes,      std::move(r),  "recipe");

    validateGameData(data);

    finalizeGameData(data);
    return data;
}


// ── setupGlobals ─────────────────────────────────────────────────────────────

void ScriptManager::setupGlobals() {
    JSContext* ctx = context_;
    JSValue global = JS_GetGlobalObject(ctx);

    // Expose thin C primitives as __* globals — not part of the public pack API.
    auto setFn = [&](const char* name, JSCFunction* fn) {
        JS_SetPropertyStr(ctx, global, name, JS_NewCFunction(ctx, fn, name, 1));
    };
    setFn("__registerBlock", jsRegisterBlock);
    setFn("__registerItem",  jsRegisterItem);
    setFn("__registerBiome", jsRegisterBiome);
    setFn("__registerTag",   jsRegisterTag);
    setFn("__getBlock",      jsGetBlock);
    setFn("__getItem",       jsGetItem);
    setFn("__getBiome",      jsGetBiome);
    setFn("__getTag",        jsGetTag);
    setFn("__modifyBlock",   jsModifyBlock);

    // Runtime World
    setFn("__world_getBlock", jsWorldGetBlock);
    setFn("__world_setBlock", jsWorldSetBlock);

    // Runtime Player
    setFn("__player_getPosition",  jsPlayerGetPosition);
    setFn("__player_setPosition",  jsPlayerSetPosition);
    setFn("__player_getInventory", jsPlayerGetInventory);

    JS_FreeValue(ctx, global);
}

// ── executeScript ─────────────────────────────────────────────────────────────

void ScriptManager::executeScript(const std::string& source, const std::string& filename, const ScriptPhase phase) {
    currentPhase_ = phase;
    JSValue result = JS_Eval(context_, source.c_str(), source.size(),
                             filename.c_str(), JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(result)) {
        checkException(context_, -1, filename, phase, source);
    }
    JS_FreeValue(context_, result);
}

// ── checkException ────────────────────────────────────────────────────────────

bool ScriptManager::checkException(JSContext* ctx, int result, const std::string& context, const ScriptPhase phase, const std::string& source) {
    if (result >= 0) return false;

    JSValue exc = JS_GetException(ctx);
    const std::string message = getJsStringProperty(ctx, exc, "message");
    const std::string stack = getJsStringProperty(ctx, exc, "stack");
    const std::string errorName = getJsStringProperty(ctx, exc, "name");

    std::optional<std::pair<int, int>> location;
    if (!context.empty() && !stack.empty()) {
        location = extractLocation(stack, context);
    }
    if (!location && !stack.empty()) {
        location = extractLocation(stack, "<eval>");
    }

    std::cerr << "[ScriptManager] " << phaseName(phase) << " script error in " << context << '\n';
    if (!errorName.empty() || !message.empty()) {
        std::cerr << "  " << (errorName.empty() ? "Error" : errorName) << ": "
                  << (message.empty() ? "(unknown error)" : message) << '\n';
    }
    if (location.has_value()) {
        std::cerr << "  Location: line " << location->first;
        if (location->second > 0) {
            std::cerr << ", column " << location->second;
        }
        std::cerr << '\n';
    }
    if (!stack.empty()) {
        std::cerr << "  Stack:\n" << stack << '\n';
    }
    if (!source.empty() && location.has_value()) {
        const std::string snippet = sourceSnippet(source, location->first, location->second);
        if (!snippet.empty()) {
            std::cerr << "  Source:\n" << snippet << '\n';
        }
    }
    JS_FreeValue(ctx, exc);
    return true;
}

const char* ScriptManager::scriptPhaseName(const ScriptPhase phase) {
    return phaseName(phase);
}

bool ScriptManager::requirePhase(JSContext* ctx, const ScriptPhase expected, const char* apiName) const {
    if (currentPhase_ == expected) {
        return true;
    }

    JS_ThrowTypeError(ctx, "%s is only available during %s scripts", apiName, scriptPhaseName(expected));
    return false;
}

// ── parseBlockState ───────────────────────────────────────────────────────────

BlockStateDefinition ScriptManager::parseBlockState(JSValueConst obj, const std::string& blockId) {
    using namespace voxel::js;
    JSContext* ctx = context_;
    BlockStateDefinition bsd;
    bsd.id = blockId;

    JSValue states = JS_GetPropertyStr(ctx, obj, "states");
    if (!JS_IsUndefined(states) && !JS_IsNull(states)) {
        jsForEachProp(ctx, states, [&](const char* propName, JSValue propDef) {
            BlockStatePropDef pd;
            std::string type = jsStr(ctx, propDef, "type", "int");

            if (type == "bool") {
                pd.values = {false, true};
                pd.defaultValue = jsBool(ctx, propDef, "default", false);
            } else if (type == "string") {
                JSValue vals = JS_GetPropertyStr(ctx, propDef, "values");
                if (!JS_IsUndefined(vals) && !JS_IsNull(vals)) {
                    uint32_t len = jsArrayLen(ctx, vals);
                    for (uint32_t vi = 0; vi < len; ++vi) {
                        JSValue e = JS_GetPropertyUint32(ctx, vals, vi);
                        const char* s = JS_ToCString(ctx, e);
                        if (s) pd.values.push_back(std::string(s));
                        JS_FreeCString(ctx, s);
                        JS_FreeValue(ctx, e);
                    }
                }
                JS_FreeValue(ctx, vals);
                pd.defaultValue = jsStr(ctx, propDef, "default");
            } else if (type == "int") {
                // int range: min..max
                int minV = jsInt(ctx, propDef, "min", 0);
                int maxV = jsInt(ctx, propDef, "max", 0);
                if (maxV < minV) {
                    throw std::runtime_error(std::string("Block state property '") + propName +
                                             "' has max below min");
                }
                int defV = jsInt(ctx, propDef, "default", minV);
                for (int v = minV; v <= maxV; ++v) pd.values.push_back(v);
                pd.defaultValue = defV;
            } else {
                throw std::runtime_error(std::string("Block state property '") + propName +
                                         "' has invalid type '" + type + "'");
            }

            bsd.props.emplace_back(propName, std::move(pd));
        });
    }
    JS_FreeValue(ctx, states);

    // Sort props by name for deterministic state ID assignment across loads
    std::sort(bsd.props.begin(), bsd.props.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    // variants: { "age=0": { model: "..." }, ... }
    JSValue variants = JS_GetPropertyStr(ctx, obj, "variants");
    if (!JS_IsUndefined(variants) && !JS_IsNull(variants)) {
        jsForEachProp(ctx, variants, [&](const char* varKey, JSValue varObj) {
            BlockStateVariant variant;
            std::string model = jsStr(ctx, varObj, "model");
            if (!model.empty()) variant.modelPath = model;
            bsd.variants[varKey] = std::move(variant);
        });
    }
    JS_FreeValue(ctx, variants);

    return bsd;
}

// ── *ToJs helpers ─────────────────────────────────────────────────────────────

JSValue ScriptManager::blockToJs(const BlockDefinition& block) const {
    JSContext* ctx = context_;
    JSValue obj = JS_NewObject(ctx);

    JS_SetPropertyStr(ctx, obj, "id",          JS_NewString(ctx, block.id.c_str()));
    JS_SetPropertyStr(ctx, obj, "name",        JS_NewString(ctx, block.name.c_str()));
    JS_SetPropertyStr(ctx, obj, "solid",       JS_NewBool(ctx, static_cast<int>(block.solid)));
    JS_SetPropertyStr(ctx, obj, "translucent", JS_NewBool(ctx, static_cast<int>(block.translucent)));
    JS_SetPropertyStr(ctx, obj, "material",    JS_NewString(ctx, block.material.c_str()));
    JS_SetPropertyStr(ctx, obj, "opacity",     JS_NewFloat64(ctx, block.opacity));
    JS_SetPropertyStr(ctx, obj, "renderType",  JS_NewString(ctx, block.renderType.c_str()));
    JS_SetPropertyStr(ctx, obj, "modelPath",   JS_NewString(ctx, block.modelPath.c_str()));

    JSValue colorArr = JS_NewArray(ctx);
    for (uint32_t i = 0; i < 3; ++i)
        JS_SetPropertyUint32(ctx, colorArr, i, JS_NewFloat64(ctx, block.color[i]));
    JS_SetPropertyStr(ctx, obj, "color", colorArr);

    return obj;
}

JSValue ScriptManager::itemToJs(const ItemDefinition& item) const {
    JSContext* ctx = context_;
    JSValue obj = JS_NewObject(ctx);

    JS_SetPropertyStr(ctx, obj, "id",        JS_NewString(ctx, item.id.c_str()));
    JS_SetPropertyStr(ctx, obj, "name",      JS_NewString(ctx, item.name.c_str()));
    JS_SetPropertyStr(ctx, obj, "stackSize", JS_NewInt32(ctx, item.stackSize));
    JS_SetPropertyStr(ctx, obj, "icon",      JS_NewString(ctx, item.icon.c_str()));

    if (item.placeableBlock)
        JS_SetPropertyStr(ctx, obj, "placeableBlock",
                          JS_NewString(ctx, item.placeableBlock->c_str()));
    else
        JS_SetPropertyStr(ctx, obj, "placeableBlock", JS_NULL);

    return obj;
}

JSValue ScriptManager::biomeToJs(const BiomeDefinition& biome) const {
    JSContext* ctx = context_;
    JSValue obj = JS_NewObject(ctx);

    JS_SetPropertyStr(ctx, obj, "id",       JS_NewString(ctx, biome.id.c_str()));
    JS_SetPropertyStr(ctx, obj, "name",     JS_NewString(ctx, biome.name.c_str()));
    JS_SetPropertyStr(ctx, obj, "priority", JS_NewFloat64(ctx, biome.priority));
    JS_SetPropertyStr(ctx, obj, "rarity",   JS_NewFloat64(ctx, biome.rarity));

    // surface
    JSValue surface = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, surface, "top",         JS_NewString(ctx, biome.surface.top.c_str()));
    JS_SetPropertyStr(ctx, surface, "middle",      JS_NewString(ctx, biome.surface.middle.c_str()));
    JS_SetPropertyStr(ctx, surface, "base",        JS_NewString(ctx, biome.surface.base.c_str()));
    JS_SetPropertyStr(ctx, surface, "middleDepth", JS_NewInt32(ctx, biome.surface.middleDepth));
    JS_SetPropertyStr(ctx, obj, "surface", surface);

    return obj;
}

// ── Static JS callbacks ───────────────────────────────────────────────────────

JSValue ScriptManager::jsRegisterBlock(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* self = static_cast<ScriptManager*>(JS_GetContextOpaque(ctx));
    if (!self || !self->requirePhase(ctx, ScriptPhase::Startup, "StartupEvents.registry(\"block\")")) {
        return JS_EXCEPTION;
    }
    if (argc < 1 || !JS_IsObject(argv[0]))
        return JS_ThrowTypeError(ctx, "Registry.registerBlock: expected an object argument");

    BlockDefinition block;
    try {
        block = voxel::generated::parseBlockDefinition(ctx, argv[0]);
    } catch (const std::exception& e) {
        return JS_ThrowTypeError(ctx, "Registry.registerBlock: %s", e.what());
    }
    if (block.id.empty())
        return JS_ThrowTypeError(ctx, "Registry.registerBlock: block must have an 'id' field");

    // states + variants → BlockStateDefinition (if present)
    JSValue states = JS_GetPropertyStr(ctx, argv[0], "states");
    if (!JS_IsUndefined(states) && !JS_IsNull(states)) {
        try {
            self->pendingBlockStates_.push_back(self->parseBlockState(argv[0], block.id));
        } catch (const std::exception& e) {
            JS_FreeValue(ctx, states);
            return JS_ThrowTypeError(ctx, "Registry.registerBlock: %s", e.what());
        }
    }
    JS_FreeValue(ctx, states);

    self->pendingBlocks_.push_back(std::move(block));
    return JS_UNDEFINED;
}

JSValue ScriptManager::jsRegisterItem(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* self = static_cast<ScriptManager*>(JS_GetContextOpaque(ctx));
    if (!self || !self->requirePhase(ctx, ScriptPhase::Startup, "StartupEvents.registry(\"item\")")) {
        return JS_EXCEPTION;
    }
    if (argc < 1 || !JS_IsObject(argv[0]))
        return JS_ThrowTypeError(ctx, "Registry.registerItem: expected an object argument");

    ItemDefinition item;
    try {
        item = voxel::generated::parseItemDefinition(ctx, argv[0]);
    } catch (const std::exception& e) {
        return JS_ThrowTypeError(ctx, "Registry.registerItem: %s", e.what());
    }
    if (item.id.empty())
        return JS_ThrowTypeError(ctx, "Registry.registerItem: item must have an 'id' field");

    self->pendingItems_.push_back(std::move(item));
    return JS_UNDEFINED;
}

JSValue ScriptManager::jsRegisterBiome(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* self = static_cast<ScriptManager*>(JS_GetContextOpaque(ctx));
    if (!self || !self->requirePhase(ctx, ScriptPhase::Startup, "StartupEvents.registry(\"biome\")")) {
        return JS_EXCEPTION;
    }
    if (argc < 1 || !JS_IsObject(argv[0]))
        return JS_ThrowTypeError(ctx, "Registry.registerBiome: expected an object argument");

    BiomeDefinition biome;
    try {
        biome = voxel::generated::parseBiomeDefinition(ctx, argv[0]);
    } catch (const std::exception& e) {
        return JS_ThrowTypeError(ctx, "Registry.registerBiome: %s", e.what());
    }
    if (biome.id.empty())
        return JS_ThrowTypeError(ctx, "Registry.registerBiome: biome must have an 'id' field");

    self->pendingBiomes_.push_back(std::move(biome));
    return JS_UNDEFINED;
}

JSValue ScriptManager::jsRegisterTag(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* self = static_cast<ScriptManager*>(JS_GetContextOpaque(ctx));
    if (!self || !self->requirePhase(ctx, ScriptPhase::Startup, "StartupEvents.registry(\"tag\")")) {
        return JS_EXCEPTION;
    }
    if (argc < 1)
        return JS_ThrowTypeError(ctx, "__registerTag: expected an id string or object");

    if (JS_IsString(argv[0])) {
        const char* id = JS_ToCString(ctx, argv[0]);
        const std::string tagId = id ? id : "";
        JS_FreeCString(ctx, id);

        JSValue obj = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, obj, "id", JS_NewString(ctx, tagId.c_str()));
        TagDefinition tag;
        try {
            tag = voxel::generated::parseTagDefinition(ctx, obj);
        } catch (const std::exception& e) {
            JS_FreeValue(ctx, obj);
            return JS_ThrowTypeError(ctx, "__registerTag: %s", e.what());
        }
        JS_FreeValue(ctx, obj);
        if (tag.id.empty() || tag.id.find(':') == std::string::npos)
            return JS_ThrowTypeError(ctx, "__registerTag: id must be namespaced (e.g. \"base:flammable\")");
        self->pendingTags_.push_back(std::move(tag));
        return JS_UNDEFINED;
    } else if (JS_IsObject(argv[0])) {
        TagDefinition tag;
        try {
            tag = voxel::generated::parseTagDefinition(ctx, argv[0]);
        } catch (const std::exception& e) {
            return JS_ThrowTypeError(ctx, "__registerTag: %s", e.what());
        }
        if (tag.id.empty() || tag.id.find(':') == std::string::npos)
            return JS_ThrowTypeError(ctx, "__registerTag: id must be namespaced (e.g. \"base:flammable\")");
        self->pendingTags_.push_back(std::move(tag));
        return JS_UNDEFINED;
    } else {
        return JS_ThrowTypeError(ctx, "__registerTag: expected a string id or {id, description} object");
    }
}

JSValue ScriptManager::jsGetTag(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if (argc < 1)
        return JS_ThrowTypeError(ctx, "Registry.getTag: expected a tag ID string");

    const char* id = JS_ToCString(ctx, argv[0]);
    if (!id) return JS_NULL;
    std::string tagId(id);
    JS_FreeCString(ctx, id);

    auto* self = static_cast<ScriptManager*>(JS_GetContextOpaque(ctx));
    for (auto it = self->pendingTags_.rbegin(); it != self->pendingTags_.rend(); ++it) {
        const auto& t = *it;
        if (t.id == tagId) {
            JSValue obj = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, obj, "id",          JS_NewString(ctx, t.id.c_str()));
            JS_SetPropertyStr(ctx, obj, "description", JS_NewString(ctx, t.description.c_str()));
            return obj;
        }
    }
    return JS_NULL;
}

JSValue ScriptManager::jsGetBlock(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if (argc < 1)
        return JS_ThrowTypeError(ctx, "Registry.getBlock: expected a block ID string");

    const char* id = JS_ToCString(ctx, argv[0]);
    if (!id) return JS_NULL;
    std::string blockId(id);
    JS_FreeCString(ctx, id);

    auto* self = static_cast<ScriptManager*>(JS_GetContextOpaque(ctx));
    for (const auto& b : self->pendingBlocks_) {
        if (b.id == blockId) return self->blockToJs(b);
    }
    return JS_NULL;
}

JSValue ScriptManager::jsGetItem(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if (argc < 1)
        return JS_ThrowTypeError(ctx, "Registry.getItem: expected an item ID string");

    const char* id = JS_ToCString(ctx, argv[0]);
    if (!id) return JS_NULL;
    std::string itemId(id);
    JS_FreeCString(ctx, id);

    auto* self = static_cast<ScriptManager*>(JS_GetContextOpaque(ctx));
    for (const auto& i : self->pendingItems_) {
        if (i.id == itemId) return self->itemToJs(i);
    }
    return JS_NULL;
}

JSValue ScriptManager::jsGetBiome(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if (argc < 1)
        return JS_ThrowTypeError(ctx, "Registry.getBiome: expected a biome ID string");

    const char* id = JS_ToCString(ctx, argv[0]);
    if (!id) return JS_NULL;
    std::string biomeId(id);
    JS_FreeCString(ctx, id);

    auto* self = static_cast<ScriptManager*>(JS_GetContextOpaque(ctx));
    for (const auto& b : self->pendingBiomes_) {
        if (b.id == biomeId) return self->biomeToJs(b);
    }
    return JS_NULL;
}

// ── jsModifyBlock ─────────────────────────────────────────────────────────────
//
// C++ primitive exposed as __modifyBlock(id, patchObj).
// The JS Registry.modifyBlock wrapper calls this after collecting user changes.
// Only fields explicitly present in patchObj are applied; absent fields are left
// as-is so a partial patch doesn't accidentally clear unrelated data.

JSValue ScriptManager::jsModifyBlock(JSContext* ctx, JSValueConst,
                                     int argc, JSValueConst* argv) {
    auto* self = static_cast<ScriptManager*>(JS_GetContextOpaque(ctx));
    if (!self || !self->requirePhase(ctx, ScriptPhase::Startup, "Registry.modifyBlock")) {
        return JS_EXCEPTION;
    }
    if (argc < 2)
        return JS_ThrowTypeError(ctx, "__modifyBlock: expected (id, patchObj)");

    const char* id = JS_ToCString(ctx, argv[0]);
    if (!id)
        return JS_ThrowTypeError(ctx, "__modifyBlock: invalid id argument");
    std::string blockId(id);
    JS_FreeCString(ctx, id);

    // Find the pending block registration by id.
    for (auto& b : self->pendingBlocks_) {
        if (b.id != blockId) continue;

        JSValueConst patch = argv[1];

        // Helper: read an optional string field from the patch object.
        auto applyStr = [&](const char* jsKey, std::string& target) {
            JSValue v = JS_GetPropertyStr(ctx, patch, jsKey);
            if (!JS_IsUndefined(v) && !JS_IsNull(v)) {
                const char* s = JS_ToCString(ctx, v);
                if (s) target = s;
                JS_FreeCString(ctx, s);
            }
            JS_FreeValue(ctx, v);
        };

        // Helper: read an optional float field.
        auto applyFloat = [&](const char* jsKey, float& target) {
            JSValue v = JS_GetPropertyStr(ctx, patch, jsKey);
            if (JS_IsNumber(v)) {
                double d; JS_ToFloat64(ctx, &d, v);
                target = static_cast<float>(d);
            }
            JS_FreeValue(ctx, v);
        };

        // Helper: read an optional bool field.
        auto applyBool = [&](const char* jsKey, bool& target) {
            JSValue v = JS_GetPropertyStr(ctx, patch, jsKey);
            if (JS_IsBool(v)) target = JS_ToBool(ctx, v);
            JS_FreeValue(ctx, v);
        };

        // Apply supported fields.
        applyStr("name",       b.name);
        applyStr("material",   b.material);
        applyStr("renderType", b.renderType);
        applyStr("modelPath",  b.modelPath);
        applyFloat("opacity",  b.opacity);
        applyBool("solid",       b.solid);
        applyBool("translucent", b.translucent);
        applyBool("tintKey",     b.tintKey);

        // color: [r, g, b]
        {
            JSValue colorVal = JS_GetPropertyStr(ctx, patch, "color");
            if (JS_IsArray(colorVal)) {
                for (uint32_t i = 0; i < 3; ++i) {
                    JSValue elem = JS_GetPropertyUint32(ctx, colorVal, i);
                    if (JS_IsNumber(elem)) {
                        double d; JS_ToFloat64(ctx, &d, elem);
                        b.color[i] = static_cast<float>(d);
                    }
                    JS_FreeValue(ctx, elem);
                }
            }
            JS_FreeValue(ctx, colorVal);
        }

        // hardness → stored in properties map so scripts can read it back.
        {
            JSValue hv = JS_GetPropertyStr(ctx, patch, "hardness");
            if (JS_IsNumber(hv)) {
                double d; JS_ToFloat64(ctx, &d, hv);
                b.properties["hardness"] = static_cast<float>(d);
            }
            JS_FreeValue(ctx, hv);
        }

        return JS_UNDEFINED;
    }

    return JS_ThrowReferenceError(ctx,
        "__modifyBlock: block \"%s\" has not been registered", blockId.c_str());
}

// ── loadPackJsonBlocks ────────────────────────────────────────────────────────
//
// Reads every *.json file under the pack's blocks/ directory and pushes the
// result into pendingBlocks_.  The JSON format mirrors StartupEvents block registration
// object shape so pack authors can choose either JSON or JS registration.
//
// Block id rules:
//   • If the JSON has an explicit "id" field, that is used as-is.
//   • Otherwise the id is inferred as  packId + ":" + stem(filename).

void ScriptManager::loadPackJsonBlocks(const Pack& pack) {
    auto files = pack.listFiles("data/blocks");
    std::sort(files.begin(), files.end());

    for (const auto& relPath : files) {
        // Only process .json files.
        if (relPath.size() < 5 ||
            relPath.compare(relPath.size() - 5, 5, ".json") != 0) continue;

        auto src = pack.readFile(relPath);
        if (!src) continue;

        JsonValue json;
        try { json = parseJson(*src); }
        catch (const std::exception& e) {
            std::cerr << "[ScriptManager] JSON parse error in "
                      << pack.id() << ":" << relPath << ": " << e.what() << '\n';
            continue;
        }
        if (!json.isObject()) continue;
        const auto& obj = json.asObject();

        BlockDefinition def;

        // ── id ────────────────────────────────────────────────────────────────
        if (obj.count("id") && obj.at("id").isString()) {
            def.id = obj.at("id").asString();
        } else {
            // Infer from filename: "blocks/copper_ore.json" → "example_pack:copper_ore"
            std::string stem = relPath;
            const auto slash = stem.rfind('/');
            if (slash != std::string::npos) stem = stem.substr(slash + 1);
            const auto dot = stem.rfind('.');
            if (dot != std::string::npos) stem = stem.substr(0, dot);
            def.id = pack.id() + ':' + stem;
        }

        // ── name ──────────────────────────────────────────────────────────────
        if (obj.count("name") && obj.at("name").isString())
            def.name = obj.at("name").asString();
        else
            def.name = def.id.substr(def.id.find(':') + 1);

        // ── voxel ─────────────────────────────────────────────────────────────
        if (obj.count("voxel") && obj.at("voxel").isObject()) {
            const auto& v = obj.at("voxel").asObject();
            if (v.count("solid")       && v.at("solid").isBool())       def.solid       = v.at("solid").asBool();
            if (v.count("translucent") && v.at("translucent").isBool()) def.translucent = v.at("translucent").asBool();
            if (v.count("material")    && v.at("material").isString())  def.material    = v.at("material").asString();
        }

        // ── render ────────────────────────────────────────────────────────────
        if (obj.count("render") && obj.at("render").isObject()) {
            const auto& r = obj.at("render").asObject();
            if (r.count("opacity") && r.at("opacity").isNumber())
                def.opacity = static_cast<float>(r.at("opacity").asNumber());
            if (r.count("type") && r.at("type").isString())
                def.renderType = r.at("type").asString();
            if (r.count("model") && r.at("model").isString())
                def.modelPath = r.at("model").asString();
            if (r.count("tintKey") && r.at("tintKey").isBool())
                def.tintKey = r.at("tintKey").asBool();
            if (r.count("color") && r.at("color").isArray()) {
                const auto& arr = r.at("color").asArray();
                for (int i = 0; i < 3 && i < static_cast<int>(arr.size()); ++i)
                    if (arr[i].isNumber()) def.color[i] = static_cast<float>(arr[i].asNumber());
            }
            // texture: "path" OR { albedo, normal, roughness, emissive }
            if (r.count("texture")) {
                const auto& tex = r.at("texture");
                if (tex.isString()) {
                    def.textures.albedo = tex.asString();
                } else if (tex.isObject()) {
                    const auto& t = tex.asObject();
                    if (t.count("albedo")    && t.at("albedo").isString())    def.textures.albedo    = t.at("albedo").asString();
                    if (t.count("normal")    && t.at("normal").isString())    def.textures.normal    = t.at("normal").asString();
                    if (t.count("roughness") && t.at("roughness").isString()) def.textures.roughness = t.at("roughness").asString();
                    if (t.count("emissive")  && t.at("emissive").isString())  def.textures.emissive  = t.at("emissive").asString();
                }
            }
        }

        // ── drops ─────────────────────────────────────────────────────────────
        if (obj.count("drops") && obj.at("drops").isArray()) {
            for (const auto& entry : obj.at("drops").asArray()) {
                if (!entry.isObject()) continue;
                const auto& d = entry.asObject();
                BlockDrop drop;
                if (d.count("item")  && d.at("item").isString())  drop.item  = d.at("item").asString();
                if (d.count("count") && d.at("count").isNumber()) drop.count = static_cast<int>(d.at("count").asNumber());
                if (!drop.item.empty()) def.drops.push_back(std::move(drop));
            }
        }

        // ── properties (arbitrary key→value for things like "hardness") ───────
        if (obj.count("properties") && obj.at("properties").isObject()) {
            for (const auto& [key, val] : obj.at("properties").asObject()) {
                if      (val.isBool())   def.properties[key] = val.asBool();
                else if (val.isNumber()) def.properties[key] = static_cast<float>(val.asNumber());
                else if (val.isString()) def.properties[key] = val.asString();
            }
        }

        if (def.id.empty() || def.id.find(':') == std::string::npos) {
            std::cerr << "[ScriptManager] Skipping " << relPath
                      << ": could not determine a valid namespaced id\n";
            continue;
        }

        pendingBlocks_.push_back(std::move(def));
    }
}

// ── loadPackJsonItems ─────────────────────────────────────────────────────────
//
// Same approach as loadPackJsonBlocks but for data/items/*.json.

void ScriptManager::loadPackJsonItems(const Pack& pack) {
    auto files = pack.listFiles("data/items");
    std::sort(files.begin(), files.end());

    for (const auto& relPath : files) {
        if (relPath.size() < 5 ||
            relPath.compare(relPath.size() - 5, 5, ".json") != 0) continue;

        auto src = pack.readFile(relPath);
        if (!src) continue;

        JsonValue json;
        try { json = parseJson(*src); }
        catch (const std::exception& e) {
            std::cerr << "[ScriptManager] JSON parse error in "
                      << pack.id() << ":" << relPath << ": " << e.what() << '\n';
            continue;
        }
        if (!json.isObject()) continue;
        const auto& obj = json.asObject();

        ItemDefinition def;

        // ── id ────────────────────────────────────────────────────────────────
        if (obj.count("id") && obj.at("id").isString()) {
            def.id = obj.at("id").asString();
        } else {
            std::string stem = relPath;
            const auto slash = stem.rfind('/');
            if (slash != std::string::npos) stem = stem.substr(slash + 1);
            const auto dot = stem.rfind('.');
            if (dot != std::string::npos) stem = stem.substr(0, dot);
            def.id = pack.id() + ':' + stem;
        }

        if (obj.count("name")      && obj.at("name").isString())      def.name      = obj.at("name").asString();
        if (obj.count("stackSize") && obj.at("stackSize").isNumber()) def.stackSize = static_cast<int>(obj.at("stackSize").asNumber());
        if (obj.count("icon")      && obj.at("icon").isString())      def.icon      = obj.at("icon").asString();
        if (obj.count("placeableBlock") && obj.at("placeableBlock").isString())
            def.placeableBlock = obj.at("placeableBlock").asString();

        if (def.id.empty() || def.id.find(':') == std::string::npos) continue;

        pendingItems_.push_back(std::move(def));
    }
}

void ScriptManager::loadPackJsonRecipes(const Pack& pack) {
    auto files = pack.listFiles("data/recipes");
    std::sort(files.begin(), files.end());

    for (const auto& relPath : files) {
        if (relPath.size() < 5 ||
            relPath.compare(relPath.size() - 5, 5, ".json") != 0) continue;

        auto src = pack.readFile(relPath);
        if (!src) continue;

        JsonValue json;
        try { json = parseJson(*src); }
        catch (const std::exception& e) {
            std::cerr << "[ScriptManager] JSON parse error in "
                      << pack.id() << ":" << relPath << ": " << e.what() << '\n';
            continue;
        }
        if (!json.isObject()) continue;
        const auto& obj = json.asObject();

        RecipeDefinition def;

        // ── id ────────────────────────────────────────────────────────────────
        if (obj.count("id") && obj.at("id").isString()) {
            def.id = obj.at("id").asString();
        } else {
            std::string stem = relPath;
            const auto slash = stem.rfind('/');
            if (slash != std::string::npos) stem = stem.substr(slash + 1);
            const auto dot = stem.rfind('.');
            if (dot != std::string::npos) stem = stem.substr(0, dot);
            def.id = pack.id() + ':' + stem;
        }

        if (obj.count("type")   && obj.at("type").isString())   def.type   = obj.at("type").asString();
        if (obj.count("output") && obj.at("output").isString()) def.output = obj.at("output").asString();
        if (obj.count("count")  && obj.at("count").isNumber())  def.count  = static_cast<int>(obj.at("count").asNumber());

        if (obj.count("ingredients") && obj.at("ingredients").isArray()) {
            for (const auto& entry : obj.at("ingredients").asArray()) {
                if (entry.isString()) def.ingredients.push_back(entry.asString());
            }
        }

        if (def.id.empty() || def.id.find(':') == std::string::npos) continue;

        pendingRecipes_.push_back(std::move(def));
    }
}

// ── Runtime Bindings Implementation ──────────────────────────────────────────

JSValue ScriptManager::jsWorldGetBlock(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* self = static_cast<ScriptManager*>(JS_GetContextOpaque(ctx));
    if (!self || !self->requirePhase(ctx, ScriptPhase::Runtime, "World.getBlock")) {
        return JS_EXCEPTION;
    }
    if (!self->worldSimulation_) return JS_UNDEFINED;

    int32_t x, y, z;
    if (JS_ToInt32(ctx, &x, argv[0]) < 0) return JS_EXCEPTION;
    if (JS_ToInt32(ctx, &y, argv[1]) < 0) return JS_EXCEPTION;
    if (JS_ToInt32(ctx, &z, argv[2]) < 0) return JS_EXCEPTION;

    uint16_t stateId = getBlock(self->worldSimulation_->world(), x, y, z);
    if (self->gameData_) {
        const auto it = self->gameData_->blockIdByStateId.find(stateId);
        if (it != self->gameData_->blockIdByStateId.end()) {
            return JS_NewString(ctx, it->second.c_str());
        }
    }
    return JS_NewInt32(ctx, stateId);
}

JSValue ScriptManager::jsWorldSetBlock(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* self = static_cast<ScriptManager*>(JS_GetContextOpaque(ctx));
    if (!self || !self->requirePhase(ctx, ScriptPhase::Runtime, "World.setBlock")) {
        return JS_EXCEPTION;
    }
    if (!self->worldSimulation_) return JS_UNDEFINED;

    int32_t x, y, z;
    if (JS_ToInt32(ctx, &x, argv[0]) < 0) return JS_EXCEPTION;
    if (JS_ToInt32(ctx, &y, argv[1]) < 0) return JS_EXCEPTION;
    if (JS_ToInt32(ctx, &z, argv[2]) < 0) return JS_EXCEPTION;

    uint16_t stateId = 0;
    if (JS_IsNumber(argv[3])) {
        uint32_t val;
        JS_ToUint32(ctx, &val, argv[3]);
        stateId = static_cast<uint16_t>(val);
    } else if (JS_IsString(argv[3])) {
        const char* s = JS_ToCString(ctx, argv[3]);
        if (self->gameData_) {
            stateId = runtimeIdForBlock(*self->gameData_, s);
        }
        JS_FreeCString(ctx, s);
    }

    self->worldSimulation_->applyBlockChange({x, y, z}, stateId);
    return JS_UNDEFINED;
}

JSValue ScriptManager::jsPlayerGetPosition(JSContext* ctx, JSValueConst, int, JSValueConst*) {
    auto* self = static_cast<ScriptManager*>(JS_GetContextOpaque(ctx));
    if (!self || !self->requirePhase(ctx, ScriptPhase::Runtime, "Player.getPosition")) {
        return JS_EXCEPTION;
    }
    if (!self->currentPlayer_) return JS_UNDEFINED;

    JSValue obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "x", JS_NewFloat64(ctx, self->currentPlayer_->position.x));
    JS_SetPropertyStr(ctx, obj, "y", JS_NewFloat64(ctx, self->currentPlayer_->position.y));
    JS_SetPropertyStr(ctx, obj, "z", JS_NewFloat64(ctx, self->currentPlayer_->position.z));
    return obj;
}

JSValue ScriptManager::jsPlayerSetPosition(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* self = static_cast<ScriptManager*>(JS_GetContextOpaque(ctx));
    if (!self || !self->requirePhase(ctx, ScriptPhase::Runtime, "Player.setPosition")) {
        return JS_EXCEPTION;
    }
    if (!self->currentPlayer_) return JS_UNDEFINED;

    double x, y, z;
    if (JS_ToFloat64(ctx, &x, argv[0]) < 0) return JS_EXCEPTION;
    if (JS_ToFloat64(ctx, &y, argv[1]) < 0) return JS_EXCEPTION;
    if (JS_ToFloat64(ctx, &z, argv[2]) < 0) return JS_EXCEPTION;

    self->currentPlayer_->position = {static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)};
    return JS_UNDEFINED;
}

JSValue ScriptManager::jsPlayerGetInventory(JSContext* ctx, JSValueConst, int, JSValueConst*) {
    auto* self = static_cast<ScriptManager*>(JS_GetContextOpaque(ctx));
    if (!self || !self->requirePhase(ctx, ScriptPhase::Runtime, "Player.getInventory")) {
        return JS_EXCEPTION;
    }
    if (!self->currentPlayer_) return JS_UNDEFINED;

    JSValue arr = JS_NewArray(ctx);
    for (int i = 0; i < kInventorySlots; ++i) {
        const auto& slot = self->currentPlayer_->inventory.slots[i];
        JSValue s = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, s, "itemId", JS_NewString(ctx, slot.itemId.c_str()));
        JS_SetPropertyStr(ctx, s, "count", JS_NewInt32(ctx, slot.count));
        JS_SetPropertyUint32(ctx, arr, i, s);
    }
    return arr;
}

void ScriptManager::loadRuntimeScripts(const PackManager& packManager) {
    currentPhase_ = ScriptPhase::Runtime;

    if (std::filesystem::is_directory(engineScriptsPath_)) {
        // Runtime wrappers are intentionally loaded in the runtime phase so they
        // can exist without exposing startup-only APIs in that same pass.
        // The file defines the public World/Player helper objects.
        const std::filesystem::path runtimeWrapper = engineScriptsPath_ / "05_runtime.js";
        if (std::filesystem::exists(runtimeWrapper)) {
            executeScript(readFile(runtimeWrapper), "<engine>/05_runtime.js", ScriptPhase::Runtime);
        }
    }

    for (auto it = packManager.packs().rbegin(); it != packManager.packs().rend(); ++it) {
        const Pack& pack = *it;
        if (!pack.manifest().scripts.server) continue;

        if (pack.hasFile("scripts/main.js")) {
            auto src = pack.readFile("scripts/main.js");
            if (src) {
                executeScript(*src, pack.id() + ":scripts/main.js", ScriptPhase::Runtime);
            }
        }
    }
}

void ScriptManager::tick(float deltaTime) {
    currentPhase_ = ScriptPhase::Runtime;
    JSValue global = JS_GetGlobalObject(context_);
    JSValue tickFn = JS_GetPropertyStr(context_, global, "tick");
    if (JS_IsFunction(context_, tickFn)) {
        JSValue arg = JS_NewFloat64(context_, deltaTime);
        JSValue result = JS_Call(context_, tickFn, global, 1, &arg);
        if (JS_IsException(result)) {
            checkException(context_, -1, "tick", ScriptPhase::Runtime);
        }
        JS_FreeValue(context_, result);
    }
    JS_FreeValue(context_, tickFn);
    JS_FreeValue(context_, global);
}

}  // namespace voxel
