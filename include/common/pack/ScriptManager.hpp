#pragma once

#include "data/GameData.hpp"
#include "pack/PackManager.hpp"

// QuickJS-NG: JSValue is a tagged union, so we need the full header.
#include <quickjs.h>

#include <filesystem>

namespace voxel {

enum class ScriptPhase {
    Startup = 0,
    Runtime = 1,
};

enum class ScriptHost {
    Unknown = 0,
    Client = 1,
    Server = 2,
};

// Embeds a QuickJS runtime, exposes Registry and StartupEvents globals to JavaScript,
// executes all pack scripts in load order, and produces a populated GameData.
//
// JS globals (C++ primitives — don't call these directly from pack scripts):
//   __registerBlock(def)    __registerItem(def)  __registerBiome(def)
//   __getBlock(id)          __getItem(id)         __getBiome(id)
//   __modifyBlock(id, patch) — partial-update a pending block registration
//   __logInfo(...args)      __platform_isClient()  __resourceReadText(path)
//   __registerRecipe(def)
//   __locAdd(locale, map)    __locGet(locale, key)
//   __dataGetBlock(id)       __dataGetItem(id)     __dataGetBiome(id)
//   __dataGetTag(id)         __dataGetRecipe(id)   __dataGetLocalization(locale, key)
//   __timerSetTimeout(fn, ms) __timerSetInterval(fn, ms) __timerClear(id)
//   __commandRegister(name, fn)
//   __commandList()
//   __modelExists(path)      __modelReadText(path)  __modelReadJson(path)
//   __modelList(path)
//
// High-level JS globals (defined in engine/scripts/):
//   Logger.info(...)        Platform.isClient()    Resources.readText(path)
//   Recipes.crafting(...)   Recipes.smelting(...)
//   Localization.add(...)   Localization.get(...)
//   Data.getBlock(...)      Data.getItem(...)       Data.getBiome(...)
//   Data.getTag(...)        Data.getRecipe(...)     Data.getLocalization(...)
//   Timers.setTimeout(...)  Timers.setInterval(...) Timers.clear(...)
//   Commands.register(...)
//   Commands.list()
//   Models.exists(...)      Models.readText(...)    Models.readJson(...)
//   Models.list(...)
//   StartupEvents.registry(type, fn) Registry.getBlock(id)
//                                    Registry.getItem(id)
//                                    Registry.getBiome(id)
//                                    Registry.modifyBlock(id, fn)
class WorldSimulation;
struct Player;

class ScriptManager {
public:
    struct ScriptTimer {
        std::uint64_t id = 0;
        JSValue callback = JS_UNDEFINED;
        double dueTime = 0.0;
        double interval = 0.0;
        bool repeating = false;
        bool cancelled = false;
    };

    ScriptManager();
    ~ScriptManager();

    // Context for runtime scripts
    void setWorldSimulation(WorldSimulation* simulation) { worldSimulation_ = simulation; }
    void setCurrentPlayer(Player* player) { currentPlayer_ = player; }
    void setGameData(const GameData* gameData) { gameData_ = gameData; }
    void setPackManager(const PackManager* packManager) { packManager_ = packManager; }
    void setHostKind(ScriptHost host) { host_ = host; }

    // Execute engine scripts first, then for every pack in load order:
    //   1. Load block/item definitions from blocks/*.json and items/*.json
    //   2. Run scripts/startup/main.js (or all .js files in scripts/startup/)
    // engineScriptsPath — path to the engine/scripts/ directory.
    GameData loadGameData(const PackManager& packManager,
                          const std::filesystem::path& engineScriptsPath);

    void loadRuntimeScripts(const PackManager& packManager);
    void tick(float deltaTime);
    std::vector<std::string> executeCommand(std::uint32_t senderId, const std::string& input);

private:
    JSRuntime* runtime_ = nullptr;
    JSContext* context_ = nullptr;
    ScriptPhase currentPhase_ = ScriptPhase::Startup;
    double runtimeTimeSeconds_ = 0.0;
    std::uint64_t nextTimerId_ = 1;

    // Accumulated during script execution
    std::vector<BlockDefinition>      pendingBlocks_;
    std::vector<ItemDefinition>       pendingItems_;
    std::vector<BiomeDefinition>      pendingBiomes_;
    std::vector<RecipeDefinition>     pendingRecipes_;
    std::vector<TagDefinition>        pendingTags_;
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> pendingLocalizations_;
    struct CommandEntry {
        std::string name;
        JSValue handler = JS_UNDEFINED;
    };
    std::vector<CommandEntry> pendingCommands_;
    std::vector<BlockStateDefinition> pendingBlockStates_;

    void setupGlobals();
    void executeScript(const std::string& source, const std::string& filename, ScriptPhase phase);
    bool checkException(JSContext* ctx, int result, const std::string& context, ScriptPhase phase, const std::string& source = {});
    static const char* scriptPhaseName(ScriptPhase phase);
    bool requirePhase(JSContext* ctx, ScriptPhase expected, const char* apiName) const;

    // __register* — private write-side callbacks used by StartupEvents.
    static JSValue jsRegisterBlock(JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue jsRegisterItem (JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue jsRegisterBiome(JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue jsRegisterRecipe(JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue jsRegisterTag  (JSContext*, JSValueConst, int, JSValueConst*);

    // Registry.* — read-side callbacks (query what has already been registered)
    static JSValue jsGetBlock (JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue jsGetItem  (JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue jsGetBiome (JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue jsGetTag   (JSContext*, JSValueConst, int, JSValueConst*);

    // Mutate a pending registration (used by Registry.modifyBlock in JS).
    // argv[0] = namespaced block id (string)
    // argv[1] = patch object — only fields present are applied
    static JSValue jsModifyBlock(JSContext*, JSValueConst, int, JSValueConst*);

    // Load block / item definitions from a pack's JSON data directories.
    // Called once per pack before its scripts run so that scripts can call
    // Registry.modifyBlock on JSON-defined blocks in the same startup pass.
    void loadPackJsonBlocks(const Pack& pack);
    void loadPackJsonItems (const Pack& pack);
    void loadPackJsonRecipes(const Pack& pack);

    // JS object → C++ struct helpers (use context_ internally)
    BlockStateDefinition parseBlockState(JSValueConst obj, const std::string& blockId);

    // C++ → JS object helpers (used by Registry.get*)
    JSValue blockToJs (const BlockDefinition&  block)  const;
    JSValue itemToJs  (const ItemDefinition&   item)   const;
    JSValue biomeToJs (const BiomeDefinition&  biome)  const;

    // Runtime bindings
    static JSValue jsWorldGetBlock(JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue jsWorldSetBlock(JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue jsPlayerGetPosition(JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue jsPlayerSetPosition(JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue jsPlayerGetInventory(JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue jsLogInfo(JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue jsLogWarn(JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue jsLogError(JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue jsPlatformIsClient(JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue jsPlatformIsServer(JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue jsPlatformIsDevelopment(JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue jsPlatformGetGameVersion(JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue jsPlatformIsPackLoaded(JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue jsResourceExists(JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue jsResourceReadText(JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue jsResourceList(JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue jsTagAdd(JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue jsTagRemove(JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue jsTagHas(JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue jsLocalizationAdd(JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue jsLocalizationGet(JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue jsDataGetBlock(JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue jsDataGetItem(JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue jsDataGetBiome(JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue jsDataGetTag(JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue jsDataGetRecipe(JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue jsDataGetLocalization(JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue jsTimerSetTimeout(JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue jsTimerSetInterval(JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue jsTimerClear(JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue jsCommandRegister(JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue jsCommandList(JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue jsModelExists(JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue jsModelReadText(JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue jsModelReadJson(JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue jsModelList(JSContext*, JSValueConst, int, JSValueConst*);

    WorldSimulation* worldSimulation_ = nullptr;
    Player* currentPlayer_ = nullptr;
    const GameData* gameData_ = nullptr;
    const PackManager* packManager_ = nullptr;
    ScriptHost host_ = ScriptHost::Unknown;
    std::filesystem::path engineScriptsPath_;
    std::vector<ScriptTimer> timers_;
};

}  // namespace voxel
