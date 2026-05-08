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
#include <unordered_set>
#include <unordered_map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace {
constexpr const char* kGameVersion = "0.1.0";

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

const char* hostName(const voxel::ScriptHost host) {
    switch (host) {
        case voxel::ScriptHost::Client: return "client";
        case voxel::ScriptHost::Server: return "server";
        default: return "unknown";
    }
}

std::string joinLogArguments(JSContext* ctx, int argc, JSValueConst* argv) {
    std::ostringstream ss;
    for (int i = 0; i < argc; ++i) {
        if (i > 0) {
            ss << ' ';
        }
        const char* text = JS_ToCString(ctx, argv[i]);
        if (text) {
            ss << text;
            JS_FreeCString(ctx, text);
        } else {
            ss << "<unprintable>";
        }
    }
    return ss.str();
}

bool isNamespacedIdLike(const std::string& value) {
    const auto colon = value.find(':');
    return !value.empty() && colon != std::string::npos && colon > 0 && colon + 1 < value.size();
}

bool isLocaleCodeLike(const std::string& value) {
    if (value.empty()) {
        return false;
    }
    for (const char ch : value) {
        if (!(std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' || ch == '-')) {
            return false;
        }
    }
    return true;
}

std::string resolveTextureRef(
    const std::unordered_map<std::string, std::string>& raw,
    const std::string& ref
) {
    std::string current = ref;
    for (int i = 0; i < 16; ++i) {
        if (current.empty() || current[0] != '#') {
            return current;
        }
        const auto it = raw.find(current.substr(1));
        if (it == raw.end()) {
            return current;
        }
        current = it->second;
    }
    return current;
}

std::string assetPath(const std::string& relativePath) {
    if (relativePath.empty() || relativePath.starts_with("assets/")) {
        return relativePath;
    }
    return "assets/" + relativePath;
}

JSValue jsonValueToJs(JSContext* ctx, const voxel::JsonValue& value) {
    if (std::holds_alternative<std::nullptr_t>(value.value)) {
        return JS_NULL;
    }
    if (std::holds_alternative<bool>(value.value)) {
        return JS_NewBool(ctx, std::get<bool>(value.value));
    }
    if (std::holds_alternative<double>(value.value)) {
        return JS_NewFloat64(ctx, std::get<double>(value.value));
    }
    if (std::holds_alternative<std::string>(value.value)) {
        return JS_NewString(ctx, std::get<std::string>(value.value).c_str());
    }
    if (std::holds_alternative<voxel::JsonValue::Object>(value.value)) {
        JSValue obj = JS_NewObject(ctx);
        for (const auto& [key, child] : std::get<voxel::JsonValue::Object>(value.value)) {
            JS_SetPropertyStr(ctx, obj, key.c_str(), jsonValueToJs(ctx, child));
        }
        return obj;
    }
    if (std::holds_alternative<voxel::JsonValue::Array>(value.value)) {
        JSValue arr = JS_NewArray(ctx);
        const auto& array = std::get<voxel::JsonValue::Array>(value.value);
        for (uint32_t i = 0; i < array.size(); ++i) {
            JS_SetPropertyUint32(ctx, arr, i, jsonValueToJs(ctx, array[i]));
        }
        return arr;
    }
    return JS_NULL;
}

std::vector<std::string> splitCommandArgs(const std::string& input) {
    std::vector<std::string> parts;
    std::istringstream stream(input);
    std::string token;
    while (stream >> token) {
        parts.push_back(token);
    }
    return parts;
}

void collectCommandResult(JSContext* ctx, JSValueConst value, std::vector<std::string>& messages) {
    if (JS_IsUndefined(value) || JS_IsNull(value)) {
        return;
    }
    if (JS_IsString(value)) {
        const char* text = JS_ToCString(ctx, value);
        if (text) {
            messages.emplace_back(text);
            JS_FreeCString(ctx, text);
        }
        return;
    }
    if (JS_IsArray(value)) {
        JSValue lengthValue = JS_GetPropertyStr(ctx, value, "length");
        uint32_t len = 0;
        JS_ToUint32(ctx, &len, lengthValue);
        JS_FreeValue(ctx, lengthValue);
        for (uint32_t i = 0; i < len; ++i) {
            JSValue item = JS_GetPropertyUint32(ctx, value, i);
            collectCommandResult(ctx, item, messages);
            JS_FreeValue(ctx, item);
        }
    }
}

void validateModelAssetRecursive(
    const voxel::PackManager& packManager,
    const std::string& modelPath,
    std::vector<std::string>& errors,
    std::unordered_set<std::string>& seenModels,
    const std::string& context
) {
    if (!seenModels.insert(modelPath).second) {
        return;
    }

    const auto assetRelativePath = assetPath(modelPath);
    const auto modelText = packManager.readFile(assetRelativePath);
    if (!modelText.has_value()) {
        errors.push_back(context + " references missing model asset: " + modelPath);
        return;
    }

    voxel::JsonValue json;
    try {
        json = voxel::parseJson(*modelText);
    } catch (const std::exception& e) {
        errors.push_back(context + " model " + modelPath + " has invalid JSON: " + e.what());
        return;
    }

    if (!json.isObject()) {
        errors.push_back(context + " model " + modelPath + " must be a JSON object");
        return;
    }

    const auto& obj = json.asObject();
    std::unordered_map<std::string, std::string> rawTextures;

    if (const auto texIt = obj.find("textures"); texIt != obj.end() && texIt->second.isObject()) {
        for (const auto& [key, val] : texIt->second.asObject()) {
            if (val.isString()) {
                rawTextures[key] = val.asString();
            }
        }
    }

    if (const auto parentIt = obj.find("parent"); parentIt != obj.end() && parentIt->second.isString()) {
        const std::string parentPath = parentIt->second.asString();
        if (!parentPath.empty()) {
            validateModelAssetRecursive(
                packManager,
                parentPath,
                errors,
                seenModels,
                context + " parent of " + modelPath
            );
        }
    }

    const auto validateTexture = [&](const std::string& textureRef, const std::string& label) {
        const std::string resolved = resolveTextureRef(rawTextures, textureRef);
        if (resolved.empty()) {
            return;
        }
        if (resolved == "missing") {
            return;
        }
        if (resolved[0] == '#') {
            errors.push_back(context + " model " + modelPath + " has unresolved texture reference " + label + ": " + textureRef);
            return;
        }
        if (!packManager.readFile(assetPath(resolved)).has_value()) {
            errors.push_back(context + " model " + modelPath + " references missing texture " + label + ": " + resolved);
        }
    };

    for (const auto& [key, raw] : rawTextures) {
        validateTexture(raw, "texture " + key);
    }

    if (const auto elemIt = obj.find("elements"); elemIt != obj.end() && elemIt->second.isArray()) {
        for (const auto& elemVal : elemIt->second.asArray()) {
            if (!elemVal.isObject()) {
                continue;
            }
            const auto& elemObj = elemVal.asObject();
            const auto facesIt = elemObj.find("faces");
            if (facesIt == elemObj.end() || !facesIt->second.isObject()) {
                continue;
            }
            for (const auto& [faceName, faceVal] : facesIt->second.asObject()) {
                if (!faceVal.isObject()) {
                    continue;
                }
                const auto& faceObj = faceVal.asObject();
                const auto textureIt = faceObj.find("texture");
                if (textureIt != faceObj.end() && textureIt->second.isString()) {
                    validateTexture(textureIt->second.asString(), "face " + faceName);
                }
            }
        }
    }
}

void validateAssetReferences(const voxel::GameData& data, const voxel::PackManager& packManager, std::vector<std::string>& errors) {
    std::unordered_set<std::string> seenModels;

    const auto validatePath = [&](const std::string& context, const std::string& path) {
        if (!path.empty() && !packManager.readFile(assetPath(path)).has_value()) {
            errors.push_back(context + " references missing asset: " + path);
        }
    };

    for (const auto& [id, block] : data.blocks) {
        (void)id;
        validatePath("block " + block.id + " model", block.modelPath);
        if (block.modelPath.empty()) {
            continue;
        }
        validateModelAssetRecursive(packManager, block.modelPath, errors, seenModels, "block " + block.id);
        if (block.textures.albedo.has_value()) {
            validatePath("block " + block.id + " albedo texture", *block.textures.albedo);
        }
        if (block.textures.normal.has_value()) {
            validatePath("block " + block.id + " normal texture", *block.textures.normal);
        }
        if (block.textures.roughness.has_value()) {
            validatePath("block " + block.id + " roughness texture", *block.textures.roughness);
        }
        if (block.textures.emissive.has_value()) {
            validatePath("block " + block.id + " emissive texture", *block.textures.emissive);
        }
    }

    for (const auto& [stateId, modelPath] : data.stateModelPathById) {
        const auto blockIt = data.blockIdByStateId.find(stateId);
        const std::string blockId = blockIt != data.blockIdByStateId.end() ? blockIt->second : std::string{};
        const std::string context = blockId.empty() ? "block state " + std::to_string(stateId)
                                                    : "block state " + blockId;
        validatePath(context + " model", modelPath);
        if (!modelPath.empty()) {
            validateModelAssetRecursive(packManager, modelPath, errors, seenModels, context);
        }
    }

    for (const auto& [id, item] : data.items) {
        (void)id;
        validatePath("item " + item.id + " icon", item.icon);
    }
}

voxel::ScriptManager::ScriptTimer* findTimer(std::vector<voxel::ScriptManager::ScriptTimer>& timers, std::uint64_t id) {
    for (auto& timer : timers) {
        if (timer.id == id) {
            return &timer;
        }
    }
    return nullptr;
}

voxel::TagDefinition* findPendingTag(std::vector<voxel::TagDefinition>& tags, const std::string& id);
const voxel::TagDefinition* findLoadedTag(const voxel::GameData* gameData, const std::string& id);
const voxel::BlockDefinition* findLoadedBlock(const voxel::GameData* gameData, const std::string& id);
const voxel::ItemDefinition* findLoadedItem(const voxel::GameData* gameData, const std::string& id);
const voxel::BiomeDefinition* findLoadedBiome(const voxel::GameData* gameData, const std::string& id);
const voxel::RecipeDefinition* findLoadedRecipe(const voxel::GameData* gameData, const std::string& id);
void mergeTagDefinition(voxel::TagDefinition& destination, const voxel::TagDefinition& source);
bool isTagMemberRegistered(const voxel::TagDefinition& tag, const std::string& member);

voxel::TagDefinition* findPendingTag(std::vector<voxel::TagDefinition>& tags, const std::string& id) {
    for (auto& tag : tags) {
        if (tag.id == id) {
            return &tag;
        }
    }
    return nullptr;
}

const voxel::TagDefinition* findLoadedTag(const voxel::GameData* gameData, const std::string& id) {
    if (!gameData) {
        return nullptr;
    }
    const auto it = gameData->tags.find(id);
    if (it == gameData->tags.end()) {
        return nullptr;
    }
    return &it->second;
}

const voxel::BlockDefinition* findLoadedBlock(const voxel::GameData* gameData, const std::string& id) {
    if (!gameData) {
        return nullptr;
    }
    const auto it = gameData->blocks.find(id);
    return it == gameData->blocks.end() ? nullptr : &it->second;
}

const voxel::ItemDefinition* findLoadedItem(const voxel::GameData* gameData, const std::string& id) {
    if (!gameData) {
        return nullptr;
    }
    const auto it = gameData->items.find(id);
    return it == gameData->items.end() ? nullptr : &it->second;
}

const voxel::BiomeDefinition* findLoadedBiome(const voxel::GameData* gameData, const std::string& id) {
    if (!gameData) {
        return nullptr;
    }
    const auto it = gameData->biomes.find(id);
    return it == gameData->biomes.end() ? nullptr : &it->second;
}

const voxel::RecipeDefinition* findLoadedRecipe(const voxel::GameData* gameData, const std::string& id) {
    if (!gameData) {
        return nullptr;
    }
    const auto it = gameData->recipes.find(id);
    return it == gameData->recipes.end() ? nullptr : &it->second;
}

void mergeTagDefinition(voxel::TagDefinition& destination, const voxel::TagDefinition& source) {
    if (!source.description.empty()) {
        destination.description = source.description;
    }
    for (const auto& member : source.members) {
        if (std::find(destination.members.begin(), destination.members.end(), member) == destination.members.end()) {
            destination.members.push_back(member);
        }
    }
}

bool isTagMemberRegistered(const voxel::TagDefinition& tag, const std::string& member) {
    return std::find(tag.members.begin(), tag.members.end(), member) != tag.members.end();
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
    for (auto& timer : timers_) {
        JS_FreeValue(context_, timer.callback);
    }
    for (auto& command : pendingCommands_) {
        JS_FreeValue(context_, command.handler);
    }
    if (context_) JS_FreeContext(context_);
    if (runtime_) JS_FreeRuntime(runtime_);
}

// ── loadGameData ─────────────────────────────────────────────────────────────

GameData ScriptManager::loadGameData(const PackManager& packManager,
                                     const std::filesystem::path& engineScriptsPath) {
    engineScriptsPath_ = engineScriptsPath;
    packManager_ = &packManager;
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
    for (auto& [locale, entries] : pendingLocalizations_) {
        auto& dest = data.localizations[locale];
        for (auto& [key, value] : entries) {
            dest.insert_or_assign(key, std::move(value));
        }
    }
    for (auto& bs : pendingBlockStates_) insertUnique(data.blockStates,   std::move(bs), "block state");
    for (auto& r  : pendingRecipes_)     insertUnique(data.recipes,      std::move(r),  "recipe");

    validateGameData(data);
    finalizeGameData(data);
    std::vector<std::string> assetErrors;
    validateAssetReferences(data, packManager, assetErrors);
    if (!assetErrors.empty()) {
        std::ostringstream stream;
        stream << "[Pack Validation Error]\n";
        for (const auto& error : assetErrors) {
            stream << error << '\n';
        }
        throw std::runtime_error(stream.str());
    }
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
    setFn("__registerRecipe", jsRegisterRecipe);
    setFn("__registerTag",   jsRegisterTag);
    setFn("__getBlock",      jsGetBlock);
    setFn("__getItem",       jsGetItem);
    setFn("__getBiome",      jsGetBiome);
    setFn("__getTag",        jsGetTag);
    setFn("__modifyBlock",   jsModifyBlock);
    setFn("__logInfo",       jsLogInfo);
    setFn("__logWarn",       jsLogWarn);
    setFn("__logError",      jsLogError);
    setFn("__platform_isClient",      jsPlatformIsClient);
    setFn("__platform_isServer",      jsPlatformIsServer);
    setFn("__platform_isDevelopment", jsPlatformIsDevelopment);
    setFn("__platform_getGameVersion", jsPlatformGetGameVersion);
    setFn("__platform_isPackLoaded",   jsPlatformIsPackLoaded);
    setFn("__resourceExists",         jsResourceExists);
    setFn("__resourceReadText",       jsResourceReadText);
    setFn("__resourceList",           jsResourceList);
    setFn("__tagAdd",    jsTagAdd);
    setFn("__tagRemove",  jsTagRemove);
    setFn("__tagHas",     jsTagHas);
    setFn("__locAdd",     jsLocalizationAdd);
    setFn("__locGet",     jsLocalizationGet);
    setFn("__dataGetBlock",        jsDataGetBlock);
    setFn("__dataGetItem",         jsDataGetItem);
    setFn("__dataGetBiome",        jsDataGetBiome);
    setFn("__dataGetTag",          jsDataGetTag);
    setFn("__dataGetRecipe",       jsDataGetRecipe);
    setFn("__dataGetLocalization", jsDataGetLocalization);
    setFn("__timerSetTimeout",     jsTimerSetTimeout);
    setFn("__timerSetInterval",    jsTimerSetInterval);
    setFn("__timerClear",          jsTimerClear);
    setFn("__commandRegister",     jsCommandRegister);
    setFn("__commandList",         jsCommandList);
    setFn("__modelExists",         jsModelExists);
    setFn("__modelReadText",       jsModelReadText);
    setFn("__modelReadJson",       jsModelReadJson);
    setFn("__modelList",           jsModelList);

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

JSValue ScriptManager::jsRegisterRecipe(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* self = static_cast<ScriptManager*>(JS_GetContextOpaque(ctx));
    if (!self || !self->requirePhase(ctx, ScriptPhase::Startup, "Recipes.register")) {
        return JS_EXCEPTION;
    }
    if (argc < 1 || !JS_IsObject(argv[0])) {
        return JS_ThrowTypeError(ctx, "Recipes.register: expected an object argument");
    }

    RecipeDefinition recipe;
    try {
        recipe = voxel::generated::parseRecipeDefinition(ctx, argv[0]);
    } catch (const std::exception& e) {
        return JS_ThrowTypeError(ctx, "Recipes.register: %s", e.what());
    }
    if (recipe.id.empty()) {
        return JS_ThrowTypeError(ctx, "Recipes.register: recipe must have an 'id' field");
    }

    auto it = std::find_if(self->pendingRecipes_.begin(), self->pendingRecipes_.end(),
                           [&](const RecipeDefinition& pending) { return pending.id == recipe.id; });
    if (it == self->pendingRecipes_.end()) {
        self->pendingRecipes_.push_back(std::move(recipe));
    } else {
        *it = std::move(recipe);
    }
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
        if (TagDefinition* existing = findPendingTag(self->pendingTags_, tag.id)) {
            mergeTagDefinition(*existing, tag);
        } else {
            self->pendingTags_.push_back(std::move(tag));
        }
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
        if (TagDefinition* existing = findPendingTag(self->pendingTags_, tag.id)) {
            mergeTagDefinition(*existing, tag);
        } else {
            self->pendingTags_.push_back(std::move(tag));
        }
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
    const TagDefinition* tag = self->gameData_ ? findLoadedTag(self->gameData_, tagId) : nullptr;
    if (!tag) {
        tag = findPendingTag(self->pendingTags_, tagId);
    }
    if (tag) {
        JSValue obj = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, obj, "id",          JS_NewString(ctx, tag->id.c_str()));
        JS_SetPropertyStr(ctx, obj, "description", JS_NewString(ctx, tag->description.c_str()));
        JSValue members = JS_NewArray(ctx);
        for (uint32_t i = 0; i < tag->members.size(); ++i) {
            JS_SetPropertyUint32(ctx, members, i, JS_NewString(ctx, tag->members[i].c_str()));
        }
        JS_SetPropertyStr(ctx, obj, "members", members);
        return obj;
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

JSValue ScriptManager::jsDataGetBlock(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "Data.getBlock: expected a block ID string");
    }

    const char* id = JS_ToCString(ctx, argv[0]);
    if (!id) return JS_NULL;
    std::string blockId(id);
    JS_FreeCString(ctx, id);

    auto* self = static_cast<ScriptManager*>(JS_GetContextOpaque(ctx));
    if (!self) {
        return JS_NULL;
    }
    if (const BlockDefinition* block = findLoadedBlock(self ? self->gameData_ : nullptr, blockId)) {
        return self->blockToJs(*block);
    }
    for (const auto& b : self->pendingBlocks_) {
        if (b.id == blockId) return self->blockToJs(b);
    }
    return JS_NULL;
}

JSValue ScriptManager::jsDataGetItem(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "Data.getItem: expected an item ID string");
    }

    const char* id = JS_ToCString(ctx, argv[0]);
    if (!id) return JS_NULL;
    std::string itemId(id);
    JS_FreeCString(ctx, id);

    auto* self = static_cast<ScriptManager*>(JS_GetContextOpaque(ctx));
    if (!self) {
        return JS_NULL;
    }
    if (const ItemDefinition* item = findLoadedItem(self ? self->gameData_ : nullptr, itemId)) {
        return self->itemToJs(*item);
    }
    for (const auto& i : self->pendingItems_) {
        if (i.id == itemId) return self->itemToJs(i);
    }
    return JS_NULL;
}

JSValue ScriptManager::jsDataGetBiome(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "Data.getBiome: expected a biome ID string");
    }

    const char* id = JS_ToCString(ctx, argv[0]);
    if (!id) return JS_NULL;
    std::string biomeId(id);
    JS_FreeCString(ctx, id);

    auto* self = static_cast<ScriptManager*>(JS_GetContextOpaque(ctx));
    if (!self) {
        return JS_NULL;
    }
    if (const BiomeDefinition* biome = findLoadedBiome(self ? self->gameData_ : nullptr, biomeId)) {
        return self->biomeToJs(*biome);
    }
    for (const auto& b : self->pendingBiomes_) {
        if (b.id == biomeId) return self->biomeToJs(b);
    }
    return JS_NULL;
}

JSValue ScriptManager::jsDataGetTag(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    return jsGetTag(ctx, JS_UNDEFINED, argc, argv);
}

JSValue ScriptManager::jsDataGetRecipe(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "Data.getRecipe: expected a recipe ID string");
    }

    const char* id = JS_ToCString(ctx, argv[0]);
    if (!id) return JS_NULL;
    std::string recipeId(id);
    JS_FreeCString(ctx, id);

    auto* self = static_cast<ScriptManager*>(JS_GetContextOpaque(ctx));
    if (!self) {
        return JS_NULL;
    }
    const RecipeDefinition* recipe = findLoadedRecipe(self ? self->gameData_ : nullptr, recipeId);
    if (!recipe) {
        const auto it = std::find_if(self->pendingRecipes_.begin(), self->pendingRecipes_.end(),
                                     [&](const RecipeDefinition& pending) { return pending.id == recipeId; });
        if (it != self->pendingRecipes_.end()) {
            recipe = &*it;
        }
    }
    if (!recipe) {
        return JS_NULL;
    }

    JSValue obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "id", JS_NewString(ctx, recipe->id.c_str()));
    JS_SetPropertyStr(ctx, obj, "type", JS_NewString(ctx, recipe->type.c_str()));
    JS_SetPropertyStr(ctx, obj, "output", JS_NewString(ctx, recipe->output.c_str()));
    JS_SetPropertyStr(ctx, obj, "count", JS_NewInt32(ctx, recipe->count));
    JSValue ingredients = JS_NewArray(ctx);
    for (uint32_t i = 0; i < recipe->ingredients.size(); ++i) {
        JS_SetPropertyUint32(ctx, ingredients, i, JS_NewString(ctx, recipe->ingredients[i].c_str()));
    }
    JS_SetPropertyStr(ctx, obj, "ingredients", ingredients);
    return obj;
}

JSValue ScriptManager::jsDataGetLocalization(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    if (argc < 2) {
        return JS_ThrowTypeError(ctx, "Data.getLocalization: expected (locale, key)");
    }

    const char* localeText = JS_ToCString(ctx, argv[0]);
    const char* keyText = JS_ToCString(ctx, argv[1]);
    if (!localeText || !keyText) {
        JS_FreeCString(ctx, localeText);
        JS_FreeCString(ctx, keyText);
        return JS_NULL;
    }
    std::string locale(localeText);
    std::string key(keyText);
    JS_FreeCString(ctx, localeText);
    JS_FreeCString(ctx, keyText);

    auto* self = static_cast<ScriptManager*>(JS_GetContextOpaque(ctx));
    if (!self) {
        return JS_NULL;
    }
    const auto findValue = [&](const auto& source) -> const std::string* {
        const auto localeIt = source.find(locale);
        if (localeIt == source.end()) {
            return nullptr;
        }
        const auto entryIt = localeIt->second.find(key);
        return entryIt == localeIt->second.end() ? nullptr : &entryIt->second;
    };

    if (self) {
        if (const auto* pending = findValue(self->pendingLocalizations_)) {
            return JS_NewString(ctx, pending->c_str());
        }
        if (const auto* loaded = findValue(self->gameData_ ? self->gameData_->localizations : decltype(self->pendingLocalizations_){ })) {
            return JS_NewString(ctx, loaded->c_str());
        }
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

JSValue ScriptManager::jsLogInfo(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    std::cout << "[Logger][info] " << joinLogArguments(ctx, argc, argv) << '\n';
    return JS_UNDEFINED;
}

JSValue ScriptManager::jsLogWarn(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    std::cerr << "[Logger][warn] " << joinLogArguments(ctx, argc, argv) << '\n';
    return JS_UNDEFINED;
}

JSValue ScriptManager::jsLogError(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    std::cerr << "[Logger][error] " << joinLogArguments(ctx, argc, argv) << '\n';
    return JS_UNDEFINED;
}

JSValue ScriptManager::jsPlatformIsClient(JSContext* ctx, JSValueConst, int, JSValueConst*) {
    auto* self = static_cast<ScriptManager*>(JS_GetContextOpaque(ctx));
    return JS_NewBool(ctx, self && self->host_ == ScriptHost::Client);
}

JSValue ScriptManager::jsPlatformIsServer(JSContext* ctx, JSValueConst, int, JSValueConst*) {
    auto* self = static_cast<ScriptManager*>(JS_GetContextOpaque(ctx));
    return JS_NewBool(ctx, self && self->host_ == ScriptHost::Server);
}

JSValue ScriptManager::jsPlatformIsDevelopment(JSContext* ctx, JSValueConst, int, JSValueConst*) {
#ifdef NDEBUG
    return JS_NewBool(ctx, false);
#else
    return JS_NewBool(ctx, true);
#endif
}

JSValue ScriptManager::jsPlatformGetGameVersion(JSContext* ctx, JSValueConst, int, JSValueConst*) {
    return JS_NewString(ctx, kGameVersion);
}

JSValue ScriptManager::jsPlatformIsPackLoaded(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* self = static_cast<ScriptManager*>(JS_GetContextOpaque(ctx));
    if (!self || !self->packManager_ || argc < 1 || !JS_IsString(argv[0])) {
        return JS_FALSE;
    }

    const char* id = JS_ToCString(ctx, argv[0]);
    const bool loaded = id && self->packManager_->findPack(id) != nullptr;
    JS_FreeCString(ctx, id);
    return JS_NewBool(ctx, loaded);
}

JSValue ScriptManager::jsResourceExists(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* self = static_cast<ScriptManager*>(JS_GetContextOpaque(ctx));
    if (!self || !self->packManager_ || argc < 1 || !JS_IsString(argv[0])) {
        return JS_FALSE;
    }

    const char* path = JS_ToCString(ctx, argv[0]);
    const bool exists = path && self->packManager_->readFile(path).has_value();
    JS_FreeCString(ctx, path);
    return JS_NewBool(ctx, exists);
}

JSValue ScriptManager::jsResourceReadText(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* self = static_cast<ScriptManager*>(JS_GetContextOpaque(ctx));
    if (!self || !self->packManager_ || argc < 1 || !JS_IsString(argv[0])) {
        return JS_NULL;
    }

    const char* path = JS_ToCString(ctx, argv[0]);
    const auto text = path ? self->packManager_->readFile(path) : std::nullopt;
    JS_FreeCString(ctx, path);
    if (!text.has_value()) {
        return JS_NULL;
    }
    return JS_NewString(ctx, text->c_str());
}

JSValue ScriptManager::jsResourceList(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* self = static_cast<ScriptManager*>(JS_GetContextOpaque(ctx));
    if (!self || !self->packManager_ || argc < 1 || !JS_IsString(argv[0])) {
        return JS_NewArray(ctx);
    }

    const char* path = JS_ToCString(ctx, argv[0]);
    const auto files = path ? self->packManager_->listFiles(path) : std::vector<std::string>{};
    JS_FreeCString(ctx, path);

    JSValue arr = JS_NewArray(ctx);
    for (uint32_t i = 0; i < files.size(); ++i) {
        JS_SetPropertyUint32(ctx, arr, i, JS_NewString(ctx, files[i].c_str()));
    }
    return arr;
}

JSValue ScriptManager::jsLocalizationAdd(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* self = static_cast<ScriptManager*>(JS_GetContextOpaque(ctx));
    if (!self || !self->requirePhase(ctx, ScriptPhase::Startup, "Localization.add")) {
        return JS_EXCEPTION;
    }
    if (argc < 2 || !JS_IsString(argv[0]) || !JS_IsObject(argv[1])) {
        return JS_ThrowTypeError(ctx, "Localization.add: expected (locale, entriesObject)");
    }

    const char* localeC = JS_ToCString(ctx, argv[0]);
    const std::string locale = localeC ? localeC : "";
    JS_FreeCString(ctx, localeC);
    if (!isLocaleCodeLike(locale)) {
        return JS_ThrowTypeError(ctx, "Localization.add: invalid locale code");
    }

    auto& dest = self->pendingLocalizations_[locale];
    js::jsForEachProp(ctx, argv[1], [&](const char* key, JSValue val) {
        const char* text = JS_ToCString(ctx, val);
        if (key && text) {
            dest[key] = text;
        }
        JS_FreeCString(ctx, text);
    });
    return JS_UNDEFINED;
}

JSValue ScriptManager::jsLocalizationGet(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* self = static_cast<ScriptManager*>(JS_GetContextOpaque(ctx));
    if (!self || argc < 2 || !JS_IsString(argv[0]) || !JS_IsString(argv[1])) {
        return JS_NULL;
    }

    const char* localeC = JS_ToCString(ctx, argv[0]);
    const char* keyC = JS_ToCString(ctx, argv[1]);
    const std::string locale = localeC ? localeC : "";
    const std::string key = keyC ? keyC : "";
    JS_FreeCString(ctx, localeC);
    JS_FreeCString(ctx, keyC);
    if (!isLocaleCodeLike(locale) || key.empty()) {
        return JS_NULL;
    }

    const auto findValue = [&](const auto& table) -> std::optional<std::string> {
        const auto localeIt = table.find(locale);
        if (localeIt == table.end()) {
            return std::nullopt;
        }
        const auto valueIt = localeIt->second.find(key);
        if (valueIt == localeIt->second.end()) {
            return std::nullopt;
        }
        return valueIt->second;
    };

    if (const auto pending = findValue(self->pendingLocalizations_)) {
        return JS_NewString(ctx, pending->c_str());
    }
    if (self->gameData_) {
        if (const auto loaded = findValue(self->gameData_->localizations)) {
            return JS_NewString(ctx, loaded->c_str());
        }
    }
    return JS_NULL;
}

JSValue ScriptManager::jsTimerSetTimeout(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* self = static_cast<ScriptManager*>(JS_GetContextOpaque(ctx));
    if (!self || !self->requirePhase(ctx, ScriptPhase::Runtime, "Timers.setTimeout")) {
        return JS_EXCEPTION;
    }
    if (argc < 1 || !JS_IsFunction(ctx, argv[0])) {
        return JS_ThrowTypeError(ctx, "Timers.setTimeout: expected (callback, delayMs?)");
    }

    double delayMs = 0.0;
    if (argc >= 2) {
        JS_ToFloat64(ctx, &delayMs, argv[1]);
    }
    ScriptManager::ScriptTimer timer;
    timer.id = self->nextTimerId_++;
    timer.callback = JS_DupValue(ctx, argv[0]);
    timer.dueTime = self->runtimeTimeSeconds_ + std::max(0.0, delayMs) / 1000.0;
    timer.repeating = false;
    self->timers_.push_back(timer);
    return JS_NewInt64(ctx, static_cast<std::int64_t>(timer.id));
}

JSValue ScriptManager::jsTimerSetInterval(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* self = static_cast<ScriptManager*>(JS_GetContextOpaque(ctx));
    if (!self || !self->requirePhase(ctx, ScriptPhase::Runtime, "Timers.setInterval")) {
        return JS_EXCEPTION;
    }
    if (argc < 1 || !JS_IsFunction(ctx, argv[0])) {
        return JS_ThrowTypeError(ctx, "Timers.setInterval: expected (callback, intervalMs)");
    }

    double intervalMs = 0.0;
    if (argc >= 2) {
        JS_ToFloat64(ctx, &intervalMs, argv[1]);
    }
    const double intervalSeconds = std::max(0.0, intervalMs) / 1000.0;
    if (intervalSeconds <= 0.0) {
        return JS_ThrowRangeError(ctx, "Timers.setInterval: interval must be positive");
    }

    ScriptManager::ScriptTimer timer;
    timer.id = self->nextTimerId_++;
    timer.callback = JS_DupValue(ctx, argv[0]);
    timer.dueTime = self->runtimeTimeSeconds_ + intervalSeconds;
    timer.interval = intervalSeconds;
    timer.repeating = true;
    self->timers_.push_back(timer);
    return JS_NewInt64(ctx, static_cast<std::int64_t>(timer.id));
}

JSValue ScriptManager::jsTimerClear(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* self = static_cast<ScriptManager*>(JS_GetContextOpaque(ctx));
    if (!self || !self->requirePhase(ctx, ScriptPhase::Runtime, "Timers.clear")) {
        return JS_EXCEPTION;
    }
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "Timers.clear: expected a timer id");
    }

    std::int64_t id = 0;
    if (JS_ToInt64(ctx, &id, argv[0]) < 0) {
        return JS_EXCEPTION;
    }
    for (auto& timer : self->timers_) {
        if (timer.id == static_cast<std::uint64_t>(id)) {
            timer.cancelled = true;
            break;
        }
    }
    return JS_UNDEFINED;
}

JSValue ScriptManager::jsCommandRegister(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* self = static_cast<ScriptManager*>(JS_GetContextOpaque(ctx));
    if (!self || !self->requirePhase(ctx, ScriptPhase::Runtime, "Commands.register")) {
        return JS_EXCEPTION;
    }
    if (argc < 2 || !JS_IsString(argv[0]) || !JS_IsFunction(ctx, argv[1])) {
        return JS_ThrowTypeError(ctx, "Commands.register: expected (name, handler)");
    }

    const char* nameC = JS_ToCString(ctx, argv[0]);
    const std::string name = nameC ? nameC : "";
    JS_FreeCString(ctx, nameC);
    if (name.empty()) {
        return JS_ThrowTypeError(ctx, "Commands.register: name must be non-empty");
    }

    auto it = std::find_if(self->pendingCommands_.begin(), self->pendingCommands_.end(),
                           [&](const CommandEntry& entry) { return entry.name == name; });
    if (it != self->pendingCommands_.end()) {
        JS_FreeValue(ctx, it->handler);
        it->handler = JS_DupValue(ctx, argv[1]);
    } else {
        self->pendingCommands_.push_back(CommandEntry{name, JS_DupValue(ctx, argv[1])});
    }
    return JS_UNDEFINED;
}

JSValue ScriptManager::jsCommandList(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* self = static_cast<ScriptManager*>(JS_GetContextOpaque(ctx));
    if (!self || !self->requirePhase(ctx, ScriptPhase::Runtime, "Commands.list")) {
        return JS_EXCEPTION;
    }
    if (argc > 0) {
        return JS_ThrowTypeError(ctx, "Commands.list: expected no arguments");
    }

    JSValue arr = JS_NewArray(ctx);
    for (uint32_t i = 0; i < self->pendingCommands_.size(); ++i) {
        JS_SetPropertyUint32(ctx, arr, i, JS_NewString(ctx, self->pendingCommands_[i].name.c_str()));
    }
    return arr;
}

JSValue ScriptManager::jsModelExists(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* self = static_cast<ScriptManager*>(JS_GetContextOpaque(ctx));
    if (!self || !self->packManager_ || argc < 1 || !JS_IsString(argv[0])) {
        return JS_NewBool(ctx, false);
    }

    const char* path = JS_ToCString(ctx, argv[0]);
    const bool exists = path && self->packManager_->readFile(assetPath(path)).has_value();
    JS_FreeCString(ctx, path);
    return JS_NewBool(ctx, exists);
}

JSValue ScriptManager::jsModelReadText(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* self = static_cast<ScriptManager*>(JS_GetContextOpaque(ctx));
    if (!self || !self->packManager_ || argc < 1 || !JS_IsString(argv[0])) {
        return JS_NULL;
    }

    const char* path = JS_ToCString(ctx, argv[0]);
    const auto text = path ? self->packManager_->readFile(assetPath(path)) : std::nullopt;
    JS_FreeCString(ctx, path);
    if (!text.has_value()) {
        return JS_NULL;
    }
    return JS_NewString(ctx, text->c_str());
}

JSValue ScriptManager::jsModelReadJson(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* self = static_cast<ScriptManager*>(JS_GetContextOpaque(ctx));
    if (!self || !self->packManager_ || argc < 1 || !JS_IsString(argv[0])) {
        return JS_NULL;
    }

    const char* path = JS_ToCString(ctx, argv[0]);
    const auto text = path ? self->packManager_->readFile(assetPath(path)) : std::nullopt;
    JS_FreeCString(ctx, path);
    if (!text.has_value()) {
        return JS_NULL;
    }

    try {
        voxel::JsonValue json = voxel::parseJson(*text);
        return jsonValueToJs(ctx, json);
    } catch (const std::exception&) {
        return JS_NULL;
    }
}

JSValue ScriptManager::jsModelList(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* self = static_cast<ScriptManager*>(JS_GetContextOpaque(ctx));
    if (!self || !self->packManager_ || argc < 1 || !JS_IsString(argv[0])) {
        return JS_NewArray(ctx);
    }

    const char* path = JS_ToCString(ctx, argv[0]);
    const auto files = path ? self->packManager_->listFiles(assetPath(path)) : std::vector<std::string>{};
    JS_FreeCString(ctx, path);

    JSValue arr = JS_NewArray(ctx);
    for (uint32_t i = 0; i < files.size(); ++i) {
        JS_SetPropertyUint32(ctx, arr, i, JS_NewString(ctx, files[i].c_str()));
    }
    return arr;
}

JSValue ScriptManager::jsTagAdd(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* self = static_cast<ScriptManager*>(JS_GetContextOpaque(ctx));
    if (!self || !self->requirePhase(ctx, ScriptPhase::Startup, "Tags.add")) {
        return JS_EXCEPTION;
    }
    if (argc < 2 || !JS_IsString(argv[0]) || !JS_IsString(argv[1])) {
        return JS_ThrowTypeError(ctx, "Tags.add: expected (tagId, memberId)");
    }

    const char* tagIdC = JS_ToCString(ctx, argv[0]);
    const char* memberC = JS_ToCString(ctx, argv[1]);
    const std::string tagId = tagIdC ? tagIdC : "";
    const std::string member = memberC ? memberC : "";
    JS_FreeCString(ctx, tagIdC);
    JS_FreeCString(ctx, memberC);

    if (!isNamespacedIdLike(tagId) || !isNamespacedIdLike(member)) {
        return JS_ThrowTypeError(ctx, "Tags.add: expected namespaced ids");
    }

    TagDefinition* tag = findPendingTag(self->pendingTags_, tagId);
    if (!tag) {
        self->pendingTags_.push_back(TagDefinition{.id = tagId});
        tag = &self->pendingTags_.back();
    }
    if (!isTagMemberRegistered(*tag, member)) {
        tag->members.push_back(member);
    }
    return JS_UNDEFINED;
}

JSValue ScriptManager::jsTagRemove(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* self = static_cast<ScriptManager*>(JS_GetContextOpaque(ctx));
    if (!self || !self->requirePhase(ctx, ScriptPhase::Startup, "Tags.remove")) {
        return JS_EXCEPTION;
    }
    if (argc < 2 || !JS_IsString(argv[0]) || !JS_IsString(argv[1])) {
        return JS_ThrowTypeError(ctx, "Tags.remove: expected (tagId, memberId)");
    }

    const char* tagIdC = JS_ToCString(ctx, argv[0]);
    const char* memberC = JS_ToCString(ctx, argv[1]);
    const std::string tagId = tagIdC ? tagIdC : "";
    const std::string member = memberC ? memberC : "";
    JS_FreeCString(ctx, tagIdC);
    JS_FreeCString(ctx, memberC);

    if (!isNamespacedIdLike(tagId) || !isNamespacedIdLike(member)) {
        return JS_ThrowTypeError(ctx, "Tags.remove: expected namespaced ids");
    }

    if (TagDefinition* tag = findPendingTag(self->pendingTags_, tagId)) {
        tag->members.erase(std::remove(tag->members.begin(), tag->members.end(), member), tag->members.end());
    }
    return JS_UNDEFINED;
}

JSValue ScriptManager::jsTagHas(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    auto* self = static_cast<ScriptManager*>(JS_GetContextOpaque(ctx));
    if (!self || argc < 2 || !JS_IsString(argv[0]) || !JS_IsString(argv[1])) {
        return JS_FALSE;
    }

    const char* tagIdC = JS_ToCString(ctx, argv[0]);
    const char* memberC = JS_ToCString(ctx, argv[1]);
    const std::string tagId = tagIdC ? tagIdC : "";
    const std::string member = memberC ? memberC : "";
    JS_FreeCString(ctx, tagIdC);
    JS_FreeCString(ctx, memberC);

    if (!isNamespacedIdLike(tagId) || !isNamespacedIdLike(member)) {
        return JS_FALSE;
    }

    const TagDefinition* tag = self->gameData_ ? findLoadedTag(self->gameData_, tagId) : findPendingTag(self->pendingTags_, tagId);
    if (!tag) {
        return JS_FALSE;
    }
    return JS_NewBool(ctx, isTagMemberRegistered(*tag, member));
}

void ScriptManager::loadRuntimeScripts(const PackManager& packManager) {
    packManager_ = &packManager;
    currentPhase_ = ScriptPhase::Runtime;
    runtimeTimeSeconds_ = 0.0;
    for (auto& timer : timers_) {
        JS_FreeValue(context_, timer.callback);
    }
    timers_.clear();
    for (auto& command : pendingCommands_) {
        JS_FreeValue(context_, command.handler);
    }
    pendingCommands_.clear();

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
    runtimeTimeSeconds_ += static_cast<double>(deltaTime);
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

    for (auto& timer : timers_) {
        if (timer.cancelled || runtimeTimeSeconds_ < timer.dueTime) {
            continue;
        }

        JSValue result = JS_Call(context_, timer.callback, JS_UNDEFINED, 0, nullptr);
        if (JS_IsException(result)) {
            checkException(context_, -1, "timer callback", ScriptPhase::Runtime);
        }
        JS_FreeValue(context_, result);

        if (timer.repeating && !timer.cancelled) {
            timer.dueTime = runtimeTimeSeconds_ + timer.interval;
        } else {
            timer.cancelled = true;
        }
    }

    auto it = std::remove_if(timers_.begin(), timers_.end(), [&](ScriptTimer& timer) {
        if (!timer.cancelled) {
            return false;
        }
        JS_FreeValue(context_, timer.callback);
        return true;
    });
    timers_.erase(it, timers_.end());
}

std::vector<std::string> ScriptManager::executeCommand(std::uint32_t senderId, const std::string& input) {
    std::vector<std::string> messages;
    if (input.empty() || input[0] != '/') {
        return messages;
    }

    const std::string raw = input.substr(1);
    const auto parts = splitCommandArgs(raw);
    if (parts.empty()) {
        messages.push_back("Usage: /help");
        return messages;
    }

    const std::string commandName = parts[0];
    auto it = std::find_if(pendingCommands_.begin(), pendingCommands_.end(),
                           [&](const CommandEntry& entry) { return entry.name == commandName; });
    if (it == pendingCommands_.end()) {
        messages.push_back("Unknown command: " + commandName);
        return messages;
    }

    JSContext* ctx = context_;
    JSValue handler = JS_DupValue(ctx, it->handler);
    JSValue contextObj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, contextObj, "playerId", JS_NewInt32(ctx, static_cast<std::int32_t>(senderId)));
    JS_SetPropertyStr(ctx, contextObj, "raw", JS_NewString(ctx, raw.c_str()));
    JS_SetPropertyStr(ctx, contextObj, "name", JS_NewString(ctx, commandName.c_str()));
    JSValue args = JS_NewArray(ctx);
    for (std::size_t i = 1; i < parts.size(); ++i) {
        JS_SetPropertyUint32(ctx, args, static_cast<std::uint32_t>(i - 1), JS_NewString(ctx, parts[i].c_str()));
    }
    JS_SetPropertyStr(ctx, contextObj, "args", args);

    JSValue result = JS_Call(ctx, handler, JS_UNDEFINED, 1, &contextObj);
    if (JS_IsException(result)) {
        checkException(ctx, -1, "command /" + commandName, ScriptPhase::Runtime, input);
        JS_FreeValue(ctx, contextObj);
        JS_FreeValue(ctx, handler);
        return {"Command failed: /" + commandName};
    }

    collectCommandResult(ctx, result, messages);

    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, contextObj);
    JS_FreeValue(ctx, handler);

    return messages;
}

}  // namespace voxel
