#pragma once

#include <array>
#include <optional>
#include <string>

#include "data/GameData.hpp"

namespace voxel {
constexpr int kInventorySlots = 9;

struct InventorySlot {
    std::string itemId;
    int count = 0;
};

struct Inventory {
    std::array<InventorySlot, kInventorySlots> slots {};
    int selectedIndex = 0;
};

bool addItem(Inventory& inventory, const GameData& gameData, const std::string& itemId, int count);
bool removeItem(Inventory& inventory, const std::string& itemId, int count);
bool hasItem(const Inventory& inventory, const std::string& itemId, int count);
bool removeSelectedItem(Inventory& inventory, int count);
const InventorySlot& selectedSlot(const Inventory& inventory);
std::optional<std::string> selectedItemId(const Inventory& inventory);
void selectSlot(Inventory& inventory, int index);
}  // namespace voxel
