#include "player/Inventory.hpp"

namespace voxel {
bool addItem(Inventory& inventory, const GameData& gameData, const std::string& itemId, int count) {
    const ItemDefinition* item = findItemDefinition(gameData, itemId);
    if (item == nullptr || item->stackSize <= 0) {
        return false;
    }

    int remaining = count;

    for (auto& slot : inventory.slots) {
        if (slot.itemId == itemId && slot.count > 0 && slot.count < item->stackSize) {
            const int room = item->stackSize - slot.count;
            const int moved = remaining < room ? remaining : room;
            slot.count += moved;
            remaining -= moved;
            if (remaining == 0) {
                return true;
            }
        }
    }

    for (auto& slot : inventory.slots) {
        if (slot.count == 0) {
            slot.itemId = itemId;
            const int moved = remaining < item->stackSize ? remaining : item->stackSize;
            slot.count = moved;
            remaining -= moved;
            if (remaining == 0) {
                return true;
            }
        }
    }

    return remaining == 0;
}

bool removeItem(Inventory& inventory, const std::string& itemId, int count) {
    if (!hasItem(inventory, itemId, count)) return false;

    int remaining = count;
    for (auto& slot : inventory.slots) {
        if (slot.itemId == itemId) {
            int toRemove = std::min(remaining, slot.count);
            slot.count -= toRemove;
            remaining -= toRemove;
            if (slot.count == 0) {
                slot.itemId.clear();
            }
            if (remaining == 0) break;
        }
    }
    return true;
}

bool hasItem(const Inventory& inventory, const std::string& itemId, int count) {
    int total = 0;
    for (const auto& slot : inventory.slots) {
        if (slot.itemId == itemId) {
            total += slot.count;
        }
    }
    return total >= count;
}

bool removeSelectedItem(Inventory& inventory, const int count) {
    auto& slot = inventory.slots[static_cast<std::size_t>(inventory.selectedIndex)];
    if (slot.count < count) {
        return false;
    }

    slot.count -= count;
    if (slot.count == 0) {
        slot.itemId.clear();
    }
    return true;
}

const InventorySlot& selectedSlot(const Inventory& inventory) {
    return inventory.slots[static_cast<std::size_t>(inventory.selectedIndex)];
}

std::optional<std::string> selectedItemId(const Inventory& inventory) {
    const auto& slot = selectedSlot(inventory);
    if (slot.count <= 0 || slot.itemId.empty()) {
        return std::nullopt;
    }
    return slot.itemId;
}

void selectSlot(Inventory& inventory, const int index) {
    if (index >= 0 && index < kInventorySlots) {
        inventory.selectedIndex = index;
    }
}
}  // namespace voxel
