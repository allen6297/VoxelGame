#pragma once

#include <array>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "data/BiomeDefinition.hpp"

namespace voxel {

// A single block property value: bool, integer, float, or string enum
using BlockProperty = std::variant<bool, int, float, std::string>;

// Definition of one state property on a block (e.g. "facing", "open")
struct BlockStatePropDef {
    std::vector<BlockProperty> values;  // all possible values; default is first
    BlockProperty defaultValue;
};

struct BlockStateVariant {
    std::optional<std::string> modelPath;
};

// Loaded from data/states/blocks/<id>.json — defines what states a block type has
struct BlockStateDefinition {
    std::string id;  // matches BlockDefinition::id
    // Sorted by name for consistent state ID enumeration across loads
    std::vector<std::pair<std::string, BlockStatePropDef>> props;
    std::unordered_map<std::string, BlockStateVariant> variants;
};

struct BlockDrop {
    std::string item;
    int count = 0;
};

// An axis-aligned box in 0–1 block-local space, derived from model elements.
struct CollisionBox {
    float minX = 0.0f, minY = 0.0f, minZ = 0.0f;
    float maxX = 1.0f, maxY = 1.0f, maxZ = 1.0f;
};

struct BlockTextures {
    std::optional<std::string> albedo;
    std::optional<std::string> normal;
    std::optional<std::string> roughness;
    std::optional<std::string> emissive;
};

struct BlockDefinition {
    std::uint16_t runtimeId = 0;  // default state ID for this block type
    std::string id;
    std::string name;
    int runtimeOrder = 1000;
    bool solid = false;
    bool translucent = false;
    std::string material;
    std::array<float, 3> color {1.0f, 1.0f, 1.0f};
    float opacity = 1.0f;
    BlockTextures textures;
    std::vector<BlockDrop> drops;
    std::unordered_map<std::string, BlockProperty> properties;  // from data/blocks/ "properties" field
    std::string renderType = "cube";  // "cube" or "model"
    std::string modelPath;            // relative to assets root
    bool tintKey = false;             // true = use biome tint color; false = use block.color
    // Per-face albedo texture paths derived from model at load time.
    // Keys: "up", "down", "north", "south", "east", "west"
    std::unordered_map<std::string, std::string> faceTextures;
    // Collision boxes in 0–1 block-local space, derived from model elements.
    // Empty = full block AABB. Populated by Game after models are loaded.
    std::vector<CollisionBox> collisionBoxes;
};

struct ItemDefinition {
    std::string id;
    std::string name;
    int stackSize = 0;
    std::optional<std::string> placeableBlock;
    std::string icon;
};

struct TagDefinition {
    std::string id;           // namespaced, e.g. "base:flammable"
    std::string description;  // optional human-readable note
};

struct RecipeDefinition
{
    std::string id;
    std::string type;
    std::string output;
    int count = 1;
    std::vector<std::string> ingredients;
};

struct GameData {
    std::unordered_map<std::string, BlockDefinition> blocks;
    std::unordered_map<std::string, ItemDefinition> items;
    std::unordered_map<std::string, TagDefinition> tags;
    std::unordered_map<std::string, BlockStateDefinition> blockStates;
    std::unordered_map<std::string, BiomeDefinition> biomes;
    std::unordered_map<std::string, RecipeDefinition> recipes;

    // State ID → block string ID (covers every state ID in every block's range)
    std::unordered_map<std::uint16_t, std::string> blockIdByStateId;
    // State ID → property values for that specific combination (empty = all defaults)
    std::unordered_map<std::uint16_t, std::unordered_map<std::string, BlockProperty>> stateValuesById;
    // State ID → model override selected from blockstate variants
    std::unordered_map<std::uint16_t, std::string> stateModelPathById;
    // State ID → per-face albedo textures resolved from a state-specific model override
    std::unordered_map<std::uint16_t, std::unordered_map<std::string, std::string>> stateFaceTexturesById;
    // State ID → collision boxes derived from the state-specific model override
    std::unordered_map<std::uint16_t, std::vector<CollisionBox>> stateCollisionBoxesById;

    // Hot-path flat arrays indexed by state ID (65536 entries)
    std::array<bool, 65536> solidByRuntimeId {};
    std::array<bool, 65536> liquidByRuntimeId {};

    // Extra blocks to search beyond the player AABB when testing collision.
    // Recomputed from model extents after models are loaded; minimum 1.
    int collisionSearchExpansion = 1;
};

// Assigns state IDs, populates derived look-up tables, and fills the flat
// solid/liquid arrays.  Call after blocks/items/biomes/blockStates are populated.
void validateGameData(const GameData& data);
void finalizeGameData(GameData& data);
std::uint16_t runtimeIdForBlock(const GameData& gameData, const std::string& blockId);
const BlockDefinition* findBlockDefinitionForBlockType(const GameData& gameData, std::uint16_t stateId);
const ItemDefinition* findItemDefinition(const GameData& gameData, const std::string& itemId);
const BlockDefinition* findBlockDefinition(const GameData& gameData, const std::string& blockId);
std::optional<std::uint16_t> blockTypeForItemId(const GameData& gameData, const std::string& itemId);
std::optional<std::uint16_t> runtimeIdForBlockState(
    const GameData& gameData,
    const std::string& blockId,
    const std::unordered_map<std::string, BlockProperty>& properties
);
const std::optional<std::string>& texturePathForType(const BlockDefinition& block, const std::string& textureType);
bool isLiquidBlockType(const GameData& gameData, std::uint16_t stateId);
std::optional<BlockProperty> getBlockProperty(const BlockDefinition& def, const std::string& key);
std::optional<BlockProperty> getStateProperty(const GameData& gameData, std::uint16_t stateId, const std::string& key);
const std::string* modelPathForState(const GameData& gameData, std::uint16_t stateId);
const std::vector<CollisionBox>* collisionBoxesForState(const GameData& gameData, std::uint16_t stateId);

}  // namespace voxel
