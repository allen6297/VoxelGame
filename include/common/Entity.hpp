#pragma once

#include <string>
#include <cstdint>
#include "Math.hpp"

namespace voxel {

enum class EntityType : std::uint8_t {
    Item = 0,
    Player = 1, // We might want to unify players as entities later
};

struct Entity {
    std::uint32_t id = 0;
    EntityType type;
    Vec3 position {0.0f, 0.0f, 0.0f};
    Vec3 velocity {0.0f, 0.0f, 0.0f};
    float yaw = 0.0f;
    float pitch = 0.0f;
    bool grounded = false;

    Entity(std::uint32_t id, EntityType type) : id(id), type(type) {}
    virtual ~Entity() = default;
};

struct ItemEntity : public Entity {
    std::string itemId;
    int count = 1;
    float age = 0.0f; // To handle despawning and pick-up delay

    ItemEntity(std::uint32_t id, std::string itemId, int count = 1)
        : Entity(id, EntityType::Item), itemId(std::move(itemId)), count(count) {}
};

} // namespace voxel
