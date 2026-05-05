#pragma once

#include <vector>
#include <unordered_map>
#include "Math.hpp"
#include "Entity.hpp"

namespace voxel {

/**
 * A simple 2D spatial hash for entities. 
 * Since voxel worlds are often flat-ish or verticality is manageable, 
 * 2D hashing on X and Z is often sufficient and more efficient.
 */
class SpatialHash {
public:
    struct CellCoord {
        int x, z;

        bool operator==(const CellCoord& other) const {
            return x == other.x && z == other.z;
        }
    };

    struct CellHash {
        std::size_t operator()(const CellCoord& c) const {
            std::size_t h = std::hash<int>{}(c.x);
            h ^= std::hash<int>{}(c.z) + 0x9e3779b9u + (h << 6) + (h >> 2);
            return h;
        }
    };

    explicit SpatialHash(float cellSize = 16.0f) : cellSize_(cellSize) {}

    void update(Entity* entity, const Vec3& oldPos) {
        CellCoord oldCell = posToCell(oldPos);
        CellCoord newCell = posToCell(entity->position);

        if (oldCell.x != newCell.x || oldCell.z != newCell.z) {
            removeFromCell(oldCell, entity);
            addToCell(newCell, entity);
        }
    }

    void add(Entity* entity) {
        addToCell(posToCell(entity->position), entity);
    }

    void remove(Entity* entity) {
        removeFromCell(posToCell(entity->position), entity);
    }

    /**
     * Finds all entities within a square area defined by center and radius.
     * Note: radius is in world units.
     */
    void query(const Vec3& center, float radius, std::vector<Entity*>& outEntities) const {
        int minX = static_cast<int>(std::floor((center.x - radius) / cellSize_));
        int maxX = static_cast<int>(std::floor((center.x + radius) / cellSize_));
        int minZ = static_cast<int>(std::floor((center.z - radius) / cellSize_));
        int maxZ = static_cast<int>(std::floor((center.z + radius) / cellSize_));

        float radiusSq = radius * radius;

        for (int x = minX; x <= maxX; ++x) {
            for (int z = minZ; z <= maxZ; ++z) {
                auto it = cells_.find({x, z});
                if (it != cells_.end()) {
                    for (Entity* entity : it->second) {
                        Vec3 diff = entity->position - center;
                        if (diff.lengthSquared() <= radiusSq) {
                            outEntities.push_back(entity);
                        }
                    }
                }
            }
        }
    }

    void clear() {
        cells_.clear();
    }

private:
    CellCoord posToCell(const Vec3& pos) const {
        return {
            static_cast<int>(std::floor(pos.x / cellSize_)),
            static_cast<int>(std::floor(pos.z / cellSize_))
        };
    }

    void addToCell(const CellCoord& cell, Entity* entity) {
        cells_[cell].push_back(entity);
    }

    void removeFromCell(const CellCoord& cell, Entity* entity) {
        auto it = cells_.find(cell);
        if (it != cells_.end()) {
            auto& vec = it->second;
            auto entry = std::find(vec.begin(), vec.end(), entity);
            if (entry != vec.end()) {
                vec.erase(entry);
            }
            if (vec.empty()) {
                cells_.erase(it);
            }
        }
    }

    float cellSize_;
    std::unordered_map<CellCoord, std::vector<Entity*>, CellHash> cells_;
};

} // namespace voxel
