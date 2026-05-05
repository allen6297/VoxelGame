#pragma once

#include "data/GameData.hpp"
#include "pack/PackManager.hpp"

// QuickJS-NG: JSValue is a tagged union, so we need the full header.
#include <quickjs.h>

#include <filesystem>

namespace voxel {

// Embeds a QuickJS runtime, exposes Startup and Registry globals to JavaScript,
// executes all pack scripts in load order, and produces a populated GameData.
//
// JS globals (C++ primitives — don't call these directly from pack scripts):
//   __registerBlock(def)    __registerItem(def)  __registerBiome(def)
//   __getBlock(id)          __getItem(id)         __getBiome(id)
//   __modifyBlock(id, patch) — partial-update a pending block registration
//
// High-level JS globals (defined in engine/scripts/):
//   Startup.registerBlock(def)       Registry.getBlock(id)
//   Startup.registerItem(def)        Registry.getItem(id)
//   Startup.registerBiome(def)       Registry.getBiome(id)
//   StartupEvents.registry(type, fn) Registry.modifyBlock(id, fn)
class WorldSimulation;
struct Player;

class ScriptManager {
public:
    ScriptManager();
    ~ScriptManager();

    // Context for runtime scripts
    void setWorldSimulation(WorldSimulation* simulation) { worldSimulation_ = simulation; }
    void setCurrentPlayer(Player* player) { currentPlayer_ = player; }
    void setGameData(const GameData* gameData) { gameData_ = gameData; }

    // Execute engine scripts first, then for every pack in load order:
    //   1. Load block/item definitions from blocks/*.json and items/*.json
    //   2. Run scripts/startup/main.js (or all .js files in scripts/startup/)
    // engineScriptsPath — path to the engine/scripts/ directory.
    GameData loadGameData(const PackManager& packManager,
                          const std::filesystem::path& engineScriptsPath);

    void loadRuntimeScripts(const PackManager& packManager);
    void tick(float deltaTime);

private:
    JSRuntime* runtime_ = nullptr;
    JSContext* context_ = nullptr;

    // Accumulated during script execution
    std::vector<BlockDefinition>      pendingBlocks_;
    std::vector<ItemDefinition>       pendingItems_;
    std::vector<BiomeDefinition>      pendingBiomes_;
    std::vector<RecipeDefinition>     pendingRecipes_;
    std::vector<TagDefinition>        pendingTags_;
    std::vector<BlockStateDefinition> pendingBlockStates_;

    void setupGlobals();
    void executeScript(const std::string& source, const std::string& filename);
    bool checkException(JSContext* ctx, int result, const std::string& context);

    // Startup.* — write-side callbacks (called during pack script execution)
    static JSValue jsRegisterBlock(JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue jsRegisterItem (JSContext*, JSValueConst, int, JSValueConst*);
    static JSValue jsRegisterBiome(JSContext*, JSValueConst, int, JSValueConst*);
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

    WorldSimulation* worldSimulation_ = nullptr;
    Player* currentPlayer_ = nullptr;
    const GameData* gameData_ = nullptr;
};

}  // namespace voxel
