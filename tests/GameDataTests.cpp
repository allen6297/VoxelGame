#include "data/GameData.hpp"
#include "pack/PackManager.hpp"
#include "pack/ScriptManager.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

#ifndef TERRALITE_SOURCE_DIR
#define TERRALITE_SOURCE_DIR "."
#endif

namespace {

using voxel::BlockDefinition;
using voxel::BlockProperty;
using voxel::BlockStateDefinition;
using voxel::BlockStatePropDef;
using voxel::BlockStateVariant;
using voxel::GameData;
using voxel::ItemDefinition;

BlockDefinition makeBlock(const std::string& id, const std::string& name, bool solid = true) {
    BlockDefinition block;
    block.id = id;
    block.name = name;
    block.solid = solid;
    return block;
}

ItemDefinition makeItem(const std::string& id, const std::string& name) {
    ItemDefinition item;
    item.id = id;
    item.name = name;
    item.stackSize = 64;
    return item;
}

BlockStateDefinition makeStoneState() {
    BlockStateDefinition state;
    state.id = "base:stone";
    state.props.push_back({
        "facing",
        BlockStatePropDef {
            {std::string("north"), std::string("south")},
            std::string("north")
        }
    });
    state.variants["facing=north"] = BlockStateVariant{std::string("models/blocks/stone_north.json")};
    state.variants["facing=south"] = BlockStateVariant{std::string("models/blocks/stone_south.json")};
    return state;
}

bool expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "[FAIL] " << message << '\n';
        return false;
    }
    return true;
}

bool expectThrowsValidation(const GameData& data, const std::string& needle) {
    try {
        voxel::validateGameData(data);
        std::cerr << "[FAIL] expected validateGameData to throw\n";
        return false;
    } catch (const std::exception& error) {
        const std::string message = error.what();
        if (message.find(needle) == std::string::npos) {
            std::cerr << "[FAIL] validation error did not contain '" << needle << "':\n"
                      << message << '\n';
            return false;
        }
    }
    return true;
}

}  // namespace

int main() {
    bool ok = true;

    GameData data;
    data.blocks["base:stone"] = makeBlock("base:stone", "Stone");
    data.blocks["base:dirt"] = makeBlock("base:dirt", "Dirt");
    data.blocks["custom:marble"] = makeBlock("custom:marble", "Marble");
    data.items["base:stone"] = makeItem("base:stone", "Stone");
    data.items["base:dirt"] = makeItem("base:dirt", "Dirt");
    data.blockStates["base:stone"] = makeStoneState();

    try {
        voxel::validateGameData(data);
        voxel::finalizeGameData(data);
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] valid game data should not throw:\n" << error.what() << '\n';
        return 1;
    }

    ok &= expect(voxel::runtimeIdForBlock(data, "base:dirt") < voxel::runtimeIdForBlock(data, "custom:marble"),
                 "preferred blocks should receive lower runtime ids");
    ok &= expect(voxel::runtimeIdForBlock(data, "base:grass") < voxel::runtimeIdForBlock(data, "base:dirt"),
                 "runtime order should place grass before dirt");
    ok &= expect(voxel::runtimeIdForBlock(data, "base:water") < voxel::runtimeIdForBlock(data, "base:stone"),
                 "runtime order should place water before stone");
    ok &= expect(voxel::runtimeIdForBlock(data, "base:stone") < voxel::runtimeIdForBlock(data, "custom:marble"),
                 "preferred blocks should receive lower runtime ids");
    ok &= expect(data.blockIdByStateId.count(data.blocks.at("base:stone").runtimeId) == 1,
                 "stone default state should be registered");
    ok &= expect(data.blockIdByStateId.count(data.blocks.at("base:stone").runtimeId + 1) == 1,
                 "stone variant state should be registered");
    ok &= expect(data.blockIdByStateId.at(data.blocks.at("base:stone").runtimeId) == "base:stone",
                 "default stone state should map back to stone");
    ok &= expect(std::get<std::string>(data.stateValuesById.at(data.blocks.at("base:stone").runtimeId).at("facing")) == "north",
                 "default stone state should use the north variant");
    ok &= expect(std::get<std::string>(data.stateValuesById.at(data.blocks.at("base:stone").runtimeId + 1).at("facing")) == "south",
                 "second stone state should use the south variant");
    ok &= expect(data.solidByRuntimeId[data.blocks.at("base:stone").runtimeId],
                 "solid flag should be propagated to runtime state ids");

    GameData invalid;
    invalid.blocks["base:stone"] = makeBlock("base:stone", "Stone");
    invalid.items["base:stone"] = makeItem("base:stone", "Stone");
    invalid.blocks["base:stone"].drops.push_back({"base:missing_item", 1});
    ok &= expectThrowsValidation(invalid, "missing item");

    try {
        const std::filesystem::path projectRoot = std::filesystem::path(TERRALITE_SOURCE_DIR);
        voxel::PackManager packManager;
        packManager.discover(projectRoot / "packs");

        voxel::ScriptManager scripts;
        scripts.setHostKind(voxel::ScriptHost::Server);
        GameData loadedData = scripts.loadGameData(packManager, projectRoot / "engine" / "scripts");
        ok &= expect(loadedData.recipes.contains("base:oak_planks"),
                     "base startup scripts should register oak planks recipe");
        ok &= expect(loadedData.recipes.contains("base:crafting_table"),
                     "base startup scripts should register crafting table recipe");
        scripts.setGameData(&loadedData);
        scripts.loadRuntimeScripts(packManager);

        const auto help = scripts.executeCommand(1, "/help");
        const bool hasHelpHeader = std::find(help.begin(), help.end(), "Available commands:") != help.end();
        const bool hasHelpCommand = std::find(help.begin(), help.end(), "/help") != help.end();
        const bool hasPingCommand = std::find(help.begin(), help.end(), "/ping") != help.end();
        ok &= expect(hasHelpHeader && hasHelpCommand && hasPingCommand,
                     "runtime scripts should register /help and /ping commands");
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] runtime command registration should not throw:\n"
                  << error.what() << '\n';
        ok = false;
    }

    if (!ok) {
        return 1;
    }

    std::cout << "GameData tests passed.\n";
    return 0;
}
