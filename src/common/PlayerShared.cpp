#include "Player.hpp"

#include <algorithm>
#include <cmath>

namespace voxel {

bool playerCollidesAt(const World& world, const GameData& gameData, const Vec3& position) {
    const float minX = position.x - kPlayerRadius;
    const float maxX = position.x + kPlayerRadius;
    const float minY = position.y;
    const float maxY = position.y + kPlayerHeight;
    const float minZ = position.z - kPlayerRadius;
    const float maxZ = position.z + kPlayerRadius;

    const int x0 = static_cast<int>(std::floor(minX));
    const int x1 = static_cast<int>(std::floor(maxX));
    const int y0 = static_cast<int>(std::floor(minY));
    const int y1 = static_cast<int>(std::floor(maxY));
    const int z0 = static_cast<int>(std::floor(minZ));
    const int z1 = static_cast<int>(std::floor(maxZ));

    const int exp = gameData.collisionSearchExpansion;
    for (int x = x0 - exp; x <= x1 + exp; ++x) {
        for (int y = y0 - exp; y <= y1 + exp; ++y) {
            for (int z = z0 - exp; z <= z1 + exp; ++z) {
                const std::uint16_t stateId = getBlock(world, x, y, z);
                if (stateId == 0 || !gameData.solidByRuntimeId[stateId]) continue;

                const BlockDefinition* def = findBlockDefinitionForBlockType(gameData, stateId);
                const std::vector<CollisionBox>* stateBoxes = collisionBoxesForState(gameData, stateId);

                if (def == nullptr || ((stateBoxes == nullptr || stateBoxes->empty()) && def->collisionBoxes.empty())) {
                    return true;
                }

                const float bx = static_cast<float>(x);
                const float by = static_cast<float>(y);
                const float bz = static_cast<float>(z);
                const std::vector<CollisionBox>& boxes =
                    (stateBoxes != nullptr && !stateBoxes->empty()) ? *stateBoxes : def->collisionBoxes;
                for (const auto& box : boxes) {
                    if (minX < bx + box.maxX && maxX > bx + box.minX &&
                        minY < by + box.maxY && maxY > by + box.minY &&
                        minZ < bz + box.maxZ && maxZ > bz + box.minZ) {
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

Vec3 getLookDirection(const Player& player) {
    const float yawRad = toRadians(player.yaw);
    const float pitchRad = toRadians(player.pitch);
    return normalize({
        std::sin(yawRad) * std::cos(pitchRad),
        std::sin(pitchRad),
        -std::cos(yawRad) * std::cos(pitchRad)
    });
}

Vec3 getEyePosition(const Player& player) {
    return {player.position.x, player.position.y + kEyeHeight, player.position.z};
}

}  // namespace voxel
