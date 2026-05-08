#include "data/GameData.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <limits>
#include <set>
#include <unordered_set>
#include <sstream>
#include <stdexcept>

namespace voxel {
namespace {

bool isNamespacedId(const std::string& value) {
    const auto colon = value.find(':');
    return !value.empty() && colon != std::string::npos && colon > 0 && colon + 1 < value.size();
}

bool isRelativeAssetPath(const std::string& value) {
    if (value.empty()) {
        return true;
    }
    if (value.starts_with('/') || value.starts_with("../")) {
        return false;
    }
    return value.find("://") == std::string::npos;
}

bool isLocaleCode(const std::string& value) {
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

std::string joinErrors(const std::vector<std::string>& errors) {
    std::ostringstream stream;
    for (const auto& error : errors) {
        stream << error << '\n';
    }
    return stream.str();
}

void appendError(std::vector<std::string>& errors, const std::string& message) {
    errors.push_back(message);
}

template <typename T>
void validateRange(std::vector<std::string>& errors,
                   const std::string& prefix,
                   const std::string& field,
                   T value,
                   T minValue,
                   T maxValue) {
    if (value < minValue || value > maxValue) {
        appendError(errors, prefix + field + " must be in range [" + std::to_string(minValue) +
                               ", " + std::to_string(maxValue) + "]");
    }
}

std::string propertyToString(const BlockProperty& property) {
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

void validateBlockStateDefinition(const BlockStateDefinition& stateDef,
                                  std::vector<std::string>& errors) {
    if (!isNamespacedId(stateDef.id)) {
        appendError(errors, "block state id must be namespaced: " + stateDef.id);
    }

    std::set<std::string> seenProps;
    for (const auto& [propName, propDef] : stateDef.props) {
        if (propName.empty()) {
            appendError(errors, "block state " + stateDef.id + " has an empty property name");
        }
        if (!seenProps.insert(propName).second) {
            appendError(errors, "block state " + stateDef.id + " has duplicate property: " + propName);
        }
        if (propDef.values.empty()) {
            appendError(errors, "block state " + stateDef.id + "." + propName + " has no values");
        }

        std::set<std::string> seenValues;
        bool defaultFound = false;
        const std::string defaultString = propertyToString(propDef.defaultValue);
        for (const auto& value : propDef.values) {
            const std::string text = propertyToString(value);
            if (!seenValues.insert(text).second) {
                appendError(errors, "block state " + stateDef.id + "." + propName +
                                      " contains duplicate value: " + text);
            }
            if (text == defaultString) {
                defaultFound = true;
            }
        }
        if (!defaultFound) {
            appendError(errors, "block state " + stateDef.id + "." + propName +
                                  " default value is not present in the allowed values");
        }
    }

    for (const auto& [variantKey, variant] : stateDef.variants) {
        if (variantKey.empty()) {
            appendError(errors, "block state " + stateDef.id + " has an empty variant key");
        }
        if (variant.modelPath.has_value() && !isRelativeAssetPath(*variant.modelPath)) {
            appendError(errors, "block state " + stateDef.id + " variant " + variantKey +
                                  " uses an invalid model path: " + *variant.modelPath);
        }
    }
}

void validateGameDataImpl(const GameData& data, std::vector<std::string>& errors) {
    for (const auto& [id, block] : data.blocks) {
        if (id != block.id) {
            appendError(errors, "block map key does not match block.id for " + id);
        }
        if (!isNamespacedId(block.id)) {
            appendError(errors, "invalid block id: " + block.id);
        }
        if (block.runtimeOrder < 0) {
            appendError(errors, "block " + block.id + " runtimeOrder must be non-negative");
        }
        validateRange(errors, "block " + block.id + ": ", "opacity", block.opacity, 0.0f, 1.0f);
        for (std::size_t i = 0; i < block.color.size(); ++i) {
            validateRange(errors, "block " + block.id + ": ", "color[" + std::to_string(i) + "]", block.color[i], 0.0f, 1.0f);
        }
        if (!isRelativeAssetPath(block.modelPath)) {
            appendError(errors, "block " + block.id + " has invalid model path: " + block.modelPath);
        }
        if (block.textures.albedo.has_value() && !isRelativeAssetPath(*block.textures.albedo)) {
            appendError(errors, "block " + block.id + " has invalid albedo texture path: " + *block.textures.albedo);
        }
        if (block.textures.normal.has_value() && !isRelativeAssetPath(*block.textures.normal)) {
            appendError(errors, "block " + block.id + " has invalid normal texture path: " + *block.textures.normal);
        }
        if (block.textures.roughness.has_value() && !isRelativeAssetPath(*block.textures.roughness)) {
            appendError(errors, "block " + block.id + " has invalid roughness texture path: " + *block.textures.roughness);
        }
        if (block.textures.emissive.has_value() && !isRelativeAssetPath(*block.textures.emissive)) {
            appendError(errors, "block " + block.id + " has invalid emissive texture path: " + *block.textures.emissive);
        }
        for (const auto& drop : block.drops) {
            if (!isNamespacedId(drop.item)) {
                appendError(errors, "block " + block.id + " has invalid drop item id: " + drop.item);
            } else if (!data.items.contains(drop.item)) {
                appendError(errors, "block " + block.id + " drop references missing item: " + drop.item);
            }
            if (drop.count <= 0) {
                appendError(errors, "block " + block.id + " drop count must be positive for item " + drop.item);
            }
        }
        for (const auto& [key, value] : block.properties) {
            (void)key;
            if (std::holds_alternative<std::string>(value)) {
                const auto& text = std::get<std::string>(value);
                if (!text.empty() && !isNamespacedId(text) && key.find("id") != std::string::npos) {
                    appendError(errors, "block " + block.id + " property " + key + " should be namespaced: " + text);
                }
            }
        }
    }

    for (const auto& [id, item] : data.items) {
        if (id != item.id) {
            appendError(errors, "item map key does not match item.id for " + id);
        }
        if (!isNamespacedId(item.id)) {
            appendError(errors, "invalid item id: " + item.id);
        }
        if (item.stackSize <= 0) {
            appendError(errors, "item " + item.id + " stackSize must be positive");
        }
        if (!isRelativeAssetPath(item.icon)) {
            appendError(errors, "item " + item.id + " has invalid icon path: " + item.icon);
        }
        if (item.placeableBlock.has_value() && !data.blocks.contains(*item.placeableBlock)) {
            appendError(errors, "item " + item.id + " references missing placeable block: " + *item.placeableBlock);
        }
    }

    for (const auto& [id, tag] : data.tags) {
        if (id != tag.id) {
            appendError(errors, "tag map key does not match tag.id for " + id);
        }
        if (!isNamespacedId(tag.id)) {
            appendError(errors, "invalid tag id: " + tag.id);
        }
        std::unordered_set<std::string> uniqueMembers;
        for (const auto& member : tag.members) {
            if (!isNamespacedId(member)) {
                appendError(errors, "tag " + tag.id + " contains invalid member id: " + member);
            }
            uniqueMembers.insert(member);
        }
        if (uniqueMembers.size() != tag.members.size()) {
            appendError(errors, "tag " + tag.id + " contains duplicate members");
        }
    }

    for (const auto& [locale, entries] : data.localizations) {
        if (!isLocaleCode(locale)) {
            appendError(errors, "invalid localization locale: " + locale);
        }
        for (const auto& [key, value] : entries) {
            if (key.empty()) {
                appendError(errors, "localization " + locale + " contains an empty key");
            }
            if (value.empty()) {
                appendError(errors, "localization " + locale + " has an empty translation for key: " + key);
            }
        }
    }

    for (const auto& [id, biome] : data.biomes) {
        if (id != biome.id) {
            appendError(errors, "biome map key does not match biome.id for " + id);
        }
        if (!isNamespacedId(biome.id)) {
            appendError(errors, "invalid biome id: " + biome.id);
        }
        validateRange(errors, "biome " + biome.id + ": ", "priority", biome.priority, 0.0f, std::numeric_limits<float>::max());
        validateRange(errors, "biome " + biome.id + ": ", "rarity", biome.rarity, 0.0f, std::numeric_limits<float>::max());
        const auto validateRangeField = [&](const char* fieldName, const BiomeClimateRange& range) {
            validateRange(errors, "biome " + biome.id + ": " + std::string(fieldName) + ".",
                          "min", range.min, 0.0f, 1.0f);
            validateRange(errors, "biome " + biome.id + ": " + std::string(fieldName) + ".",
                          "max", range.max, 0.0f, 1.0f);
            if (range.min > range.max) {
                appendError(errors, "biome " + biome.id + ": " + fieldName + " min must be <= max");
            }
        };
        validateRangeField("temperature", biome.climate.temperature);
        validateRangeField("humidity", biome.climate.humidity);
        validateRangeField("rainfall", biome.climate.rainfall);
        validateRangeField("elevation", biome.climate.elevation);
        validateRangeField("drainage", biome.climate.drainage);
        validateRangeField("waterTable", biome.climate.waterTable);
        for (const auto& [name, color] : biome.colors) {
            (void)name;
            for (std::size_t i = 0; i < color.size(); ++i) {
                validateRange(errors, "biome " + biome.id + ": color " + name + " ",
                               "[" + std::to_string(i) + "]", color[i], 0.0f, 1.0f);
            }
        }
        for (const auto& reference : {biome.surface.top, biome.surface.middle, biome.surface.base}) {
            if (!reference.empty() && !data.blocks.contains(reference)) {
                appendError(errors, "biome " + biome.id + " references missing block: " + reference);
            }
        }
        for (const auto field : {biome.fertility.nitrogen, biome.fertility.phosphorus, biome.fertility.potassium,
                                 biome.fertility.magnesium, biome.fertility.calcium, biome.fertility.sulfur}) {
            validateRange(errors, "biome " + biome.id + ": ", "fertility", field, 0.0f, 1.0f);
        }
    }

    for (const auto& [id, recipe] : data.recipes) {
        if (id != recipe.id) {
            appendError(errors, "recipe map key does not match recipe.id for " + id);
        }
        if (!isNamespacedId(recipe.id)) {
            appendError(errors, "invalid recipe id: " + recipe.id);
        }
        if (recipe.count <= 0) {
            appendError(errors, "recipe " + recipe.id + " count must be positive");
        }
        if (!data.items.contains(recipe.output)) {
            appendError(errors, "recipe " + recipe.id + " output references missing item: " + recipe.output);
        }
        if (recipe.ingredients.empty()) {
            appendError(errors, "recipe " + recipe.id + " has no ingredients");
        }
        for (const auto& ingredient : recipe.ingredients) {
            if (!data.items.contains(ingredient)) {
                appendError(errors, "recipe " + recipe.id + " ingredient references missing item: " + ingredient);
            }
        }
    }

    for (const auto& [id, stateDef] : data.blockStates) {
        if (id != stateDef.id) {
            appendError(errors, "block state map key does not match blockState.id for " + id);
        }
        if (!data.blocks.contains(stateDef.id)) {
            appendError(errors, "block state references missing block definition: " + stateDef.id);
        }
        validateBlockStateDefinition(stateDef, errors);
    }
}

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

void validateGameData(const GameData& data) {
    std::vector<std::string> errors;
    validateGameDataImpl(data, errors);
    if (!errors.empty()) {
        throw std::runtime_error("[Pack Validation Error]\n" + joinErrors(errors));
    }
}

void finalizeGameData(GameData& data) {
    // Assign state ID ranges in runtimeOrder order so packs can steer which
    // blocks receive the lowest state IDs without hardcoding specific ids.
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

    std::vector<const BlockDefinition*> orderedBlocks;
    orderedBlocks.reserve(data.blocks.size());
    for (const auto& [_, block] : data.blocks) {
        orderedBlocks.push_back(&block);
    }
    std::sort(orderedBlocks.begin(), orderedBlocks.end(), [](const BlockDefinition* a, const BlockDefinition* b) {
        if (a->runtimeOrder != b->runtimeOrder) {
            return a->runtimeOrder < b->runtimeOrder;
        }
        return a->id < b->id;
    });
    for (const auto* block : orderedBlocks) {
        assignBlock(block->id);
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
