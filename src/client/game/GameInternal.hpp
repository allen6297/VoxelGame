#pragma once

#include <optional>
#include <string>

#include "game/Game.hpp"

namespace voxel::game_internal {

inline constexpr char kWheatSeedsItemId[] = "wheat_seeds";
inline constexpr char kWheatBlockId[] = "wheat";

inline bool boolProperty(const BlockDefinition& def, const std::string& key, const bool fallback = false) {
    const auto value = getBlockProperty(def, key);
    if (!value.has_value() || !std::holds_alternative<bool>(*value)) {
        return fallback;
    }
    return std::get<bool>(*value);
}

inline std::optional<std::string> stringProperty(const BlockDefinition& def, const std::string& key) {
    const auto value = getBlockProperty(def, key);
    if (!value.has_value() || !std::holds_alternative<std::string>(*value)) {
        return std::nullopt;
    }
    return std::get<std::string>(*value);
}

inline std::optional<int> intProperty(const BlockDefinition& def, const std::string& key) {
    const auto value = getBlockProperty(def, key);
    if (!value.has_value() || !std::holds_alternative<int>(*value)) {
        return std::nullopt;
    }
    return std::get<int>(*value);
}

inline bool isCropBlock(const BlockDefinition& def) {
    return boolProperty(def, "crop");
}

inline bool isCropSoil(const GameData& gameData, const std::uint16_t stateId) {
    const BlockDefinition* def = findBlockDefinitionForBlockType(gameData, stateId);
    return def != nullptr && (def->id == "dirt" || def->id == "grass");
}

inline std::string blockTickKey(const Int3& block) {
    return std::to_string(block.x) + ":" + std::to_string(block.y) + ":" + std::to_string(block.z);
}

inline std::optional<float> tickIntervalSeconds(const BlockDefinition& def) {
    const auto interval = intProperty(def, "tickInterval");
    if (!interval.has_value() || *interval <= 0) {
        return std::nullopt;
    }
    return static_cast<float>(*interval);
}

inline int cropAge(const GameData& gameData, const std::uint16_t stateId) {
    const auto age = getStateProperty(gameData, stateId, "age");
    if (!age.has_value() || !std::holds_alternative<int>(*age)) {
        return 0;
    }
    return std::get<int>(*age);
}

inline int cropMaxAge(const BlockDefinition& def) {
    const auto maxAge = intProperty(def, "maxAge");
    return (maxAge.has_value() && *maxAge >= 0) ? *maxAge : 0;
}

inline std::vector<BlockDrop> cropDropsForState(const GameData& gameData, const BlockDefinition& def, const std::uint16_t stateId) {
    if (!isCropBlock(def)) {
        return def.drops;
    }

    const std::string seedItem = stringProperty(def, "seedItem").value_or(kWheatSeedsItemId);
    const int immatureSeeds = intProperty(def, "immatureSeedCount").value_or(1);
    if (cropAge(gameData, stateId) < cropMaxAge(def)) {
        return {{seedItem, immatureSeeds}};
    }

    std::vector<BlockDrop> drops;
    if (const auto produceItem = stringProperty(def, "produceItem"); produceItem.has_value()) {
        drops.push_back({*produceItem, intProperty(def, "produceCount").value_or(1)});
    }
    drops.push_back({seedItem, intProperty(def, "matureSeedCount").value_or(2)});
    return drops;
}

}  // namespace voxel::game_internal
