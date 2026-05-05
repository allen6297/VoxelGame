#include "data/GameData.hpp"

#include <algorithm>
#include <iostream>
#include <map>
#include <sstream>

namespace voxel {
namespace {

constexpr std::array<const char*, 4> kPreferredBlockOrder = {
    "base:grass",
    "base:dirt",
    "base:water",
    "base:stone"
};

std::string blockPropertyToString(const BlockProperty& property) {
    if (std::holds_alternative<bool>(property)) {
        return std::get<bool>(property) ? "true" : "false";
    }
    if (std::holds_alternative<int>(property)) {
        return std::to_string(std::get<int>(property));
    }
    if (std::holds_alternative<float>(property)) {
        std::ostringstream stream;
        stream << std::get<float>(property);
        return stream.str();
    }
    return std::get<std::string>(property);
}

std::string canonicalStateKey(const std::unordered_map<std::string, BlockProperty>& stateValues) {
    std::vector<std::string> parts;
    parts.reserve(stateValues.size());
    for (const auto& [key, value] : stateValues) {
        parts.push_back(key + "=" + blockPropertyToString(value));
    }
    std::sort(parts.begin(), parts.end());

    std::ostringstream joined;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i != 0) joined << ",";
        joined << parts[i];
    }
    return joined.str();
}

// Returns all state combinations for a block state definition.
// The first entry is always the default state (all properties at their default value).
std::vector<std::unordered_map<std::string, BlockProperty>> enumerateStates(
    const BlockStateDefinition& stateDef
) {
    std::vector<std::unordered_map<std::string, BlockProperty>> result;
    result.push_back({});

    for (const auto& [propName, propDef] : stateDef.props) {
        std::vector<std::unordered_map<std::string, BlockProperty>> expanded;
        expanded.reserve(result.size() * propDef.values.size());
        for (const auto& existing : result) {
            for (const auto& val : propDef.values) {
                auto copy = existing;
                copy[propName] = val;
                expanded.push_back(std::move(copy));
            }
        }
        result = std::move(expanded);
    }

    return result;
}

}  // namespace

void finalizeGameData(GameData& data) {
    // Assign state ID ranges — preferred blocks get lower IDs
    std::map<std::string, std::uint16_t> defaultStateIds;
    std::uint16_t nextId = 1;

    const auto assignBlock = [&](const std::string& blockId) {
        if (defaultStateIds.contains(blockId)) return;
        std::size_t count = 1;
        const auto stateIt = data.blockStates.find(blockId);
        if (stateIt != data.blockStates.end()) {
            for (const auto& [_, propDef] : stateIt->second.props) {
                count *= propDef.values.size();
            }
        }
        if (static_cast<std::size_t>(nextId) + count > 65535) {
            throw std::runtime_error("Ran out of state IDs assigning block: " + blockId);
        }
        defaultStateIds[blockId] = nextId;
        nextId += static_cast<std::uint16_t>(count);
    };

    for (const char* preferredId : kPreferredBlockOrder) {
        if (data.blocks.contains(preferredId)) assignBlock(preferredId);
    }
    for (const auto& [blockId, _] : data.blocks) {
        assignBlock(blockId);
    }

    // Populate block definitions and state registry
    for (auto& [blockId, block] : data.blocks) {
        block.runtimeId = defaultStateIds.at(blockId);

        const auto stateIt = data.blockStates.find(blockId);
        if (stateIt != data.blockStates.end()) {
            const auto states = enumerateStates(stateIt->second);
            for (std::size_t i = 0; i < states.size(); ++i) {
                const std::uint16_t stateId = block.runtimeId + static_cast<std::uint16_t>(i);
                data.blockIdByStateId[stateId] = blockId;
                data.stateValuesById[stateId]  = states[i];
                const auto variantIt = stateIt->second.variants.find(canonicalStateKey(states[i]));
                if (variantIt != stateIt->second.variants.end() && variantIt->second.modelPath.has_value()) {
                    data.stateModelPathById[stateId] = *variantIt->second.modelPath;
                }
                data.solidByRuntimeId[stateId]  = block.solid;
                data.liquidByRuntimeId[stateId] = (block.material == "liquid");
            }
        } else {
            data.blockIdByStateId[block.runtimeId] = blockId;
            data.solidByRuntimeId[block.runtimeId]  = block.solid;
            data.liquidByRuntimeId[block.runtimeId] = (block.material == "liquid");
        }
    }
}

std::uint16_t runtimeIdForBlock(const GameData& gameData, const std::string& blockId) {
    const auto it = gameData.blocks.find(blockId);
    return it != gameData.blocks.end() ? it->second.runtimeId : 0;
}

const BlockDefinition* findBlockDefinitionForBlockType(const GameData& gameData, const std::uint16_t stateId) {
    const auto idIt = gameData.blockIdByStateId.find(stateId);
    if (idIt == gameData.blockIdByStateId.end()) return nullptr;
    const auto it = gameData.blocks.find(idIt->second);
    return it == gameData.blocks.end() ? nullptr : &it->second;
}

const ItemDefinition* findItemDefinition(const GameData& gameData, const std::string& itemId) {
    const auto it = gameData.items.find(itemId);
    return it == gameData.items.end() ? nullptr : &it->second;
}

const BlockDefinition* findBlockDefinition(const GameData& gameData, const std::string& blockId) {
    const auto it = gameData.blocks.find(blockId);
    return it == gameData.blocks.end() ? nullptr : &it->second;
}

std::optional<std::uint16_t> blockTypeForItemId(const GameData& gameData, const std::string& itemId) {
    const ItemDefinition* item = findItemDefinition(gameData, itemId);
    if (item == nullptr || !item->placeableBlock.has_value()) return std::nullopt;
    const BlockDefinition* block = findBlockDefinition(gameData, *item->placeableBlock);
    return block ? std::optional{block->runtimeId} : std::nullopt;
}

std::optional<std::uint16_t> runtimeIdForBlockState(
    const GameData& gameData,
    const std::string& blockId,
    const std::unordered_map<std::string, BlockProperty>& properties
) {
    const BlockDefinition* block = findBlockDefinition(gameData, blockId);
    if (block == nullptr) return std::nullopt;

    std::size_t stateCount = 1;
    if (const auto stateIt = gameData.blockStates.find(blockId); stateIt != gameData.blockStates.end()) {
        for (const auto& [_, propDef] : stateIt->second.props) {
            stateCount *= propDef.values.size();
        }
    }

    for (std::size_t i = 0; i < stateCount; ++i) {
        const std::uint16_t stateId = block->runtimeId + static_cast<std::uint16_t>(i);
        const auto valuesIt = gameData.stateValuesById.find(stateId);
        if (valuesIt == gameData.stateValuesById.end()) {
            if (properties.empty()) return stateId;
            continue;
        }
        bool matches = true;
        for (const auto& [key, value] : properties) {
            const auto statePropIt = valuesIt->second.find(key);
            if (statePropIt == valuesIt->second.end() || statePropIt->second != value) {
                matches = false;
                break;
            }
        }
        if (matches) return stateId;
    }

    return std::nullopt;
}

const std::optional<std::string>& texturePathForType(const BlockDefinition& block, const std::string& textureType) {
    if (textureType == "albedo")    return block.textures.albedo;
    if (textureType == "normal")    return block.textures.normal;
    if (textureType == "roughness") return block.textures.roughness;
    return block.textures.emissive;
}

bool isLiquidBlockType(const GameData& gameData, const std::uint16_t stateId) {
    return stateId != 0 && gameData.liquidByRuntimeId[stateId];
}

std::optional<BlockProperty> getBlockProperty(const BlockDefinition& def, const std::string& key) {
    const auto it = def.properties.find(key);
    return it == def.properties.end() ? std::nullopt : std::optional{it->second};
}

std::optional<BlockProperty> getStateProperty(const GameData& gameData, const std::uint16_t stateId, const std::string& key) {
    const auto stateIt = gameData.stateValuesById.find(stateId);
    if (stateIt == gameData.stateValuesById.end()) return std::nullopt;
    const auto propIt = stateIt->second.find(key);
    return propIt == stateIt->second.end() ? std::nullopt : std::optional{propIt->second};
}

const std::string* modelPathForState(const GameData& gameData, const std::uint16_t stateId) {
    const auto it = gameData.stateModelPathById.find(stateId);
    return it == gameData.stateModelPathById.end() ? nullptr : &it->second;
}

const std::vector<CollisionBox>* collisionBoxesForState(const GameData& gameData, const std::uint16_t stateId) {
    const auto it = gameData.stateCollisionBoxesById.find(stateId);
    return it == gameData.stateCollisionBoxesById.end() ? nullptr : &it->second;
}

}  // namespace voxel
