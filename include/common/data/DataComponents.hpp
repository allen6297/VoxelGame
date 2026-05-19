#pragma once

#include <any>
#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

#include "data/JsonValue.hpp"

namespace voxel {

using ComponentTypeId = std::uint16_t;

enum class ComponentScope : std::uint8_t {
    Definition,
    Runtime
};

struct ComponentDescriptor {
    ComponentTypeId id = 0;
    std::string name;
    std::type_index type = typeid(void);
    ComponentScope scope = ComponentScope::Definition;
    std::function<std::any(const JsonValue&)> fromJson;
    std::function<JsonValue(const std::any&)> toJson;
};

template <typename T>
struct ComponentJson {
    static T fromJson(const JsonValue& value);
    static JsonValue toJson(const T& component);
};

class ComponentRegistry {
public:
    template <typename T>
    static ComponentTypeId Register(const std::string& name, ComponentScope scope = ComponentScope::Definition) {
        return instance().registerType<T>(name, scope);
    }

    template <typename T>
    static ComponentTypeId TypeId() {
        return instance().typeId<T>();
    }

    template <typename T>
    static bool IsRegistered() {
        return instance().typeIdsByType_.contains(std::type_index(typeid(T)));
    }

    static const ComponentDescriptor* FindByName(const std::string& name);
    static const ComponentDescriptor* FindById(ComponentTypeId id);
    static void ClearForTests();

private:
    template <typename T>
    ComponentTypeId registerType(const std::string& name, ComponentScope scope) {
        const std::type_index type(typeid(T));
        const auto typeIt = typeIdsByType_.find(type);
        if (typeIt != typeIdsByType_.end()) {
            const auto* descriptor = findById(typeIt->second);
            if (descriptor == nullptr || descriptor->name != name || descriptor->scope != scope) {
                throw std::runtime_error("component type registered with conflicting metadata: " + name);
            }
            return typeIt->second;
        }
        if (idsByName_.contains(name)) {
            throw std::runtime_error("component name registered for a different type: " + name);
        }

        const ComponentTypeId id = nextId_++;
        ComponentDescriptor descriptor;
        descriptor.id = id;
        descriptor.name = name;
        descriptor.type = type;
        descriptor.scope = scope;
        descriptor.fromJson = [](const JsonValue& value) -> std::any {
            return ComponentJson<T>::fromJson(value);
        };
        descriptor.toJson = [](const std::any& value) -> JsonValue {
            return ComponentJson<T>::toJson(std::any_cast<const T&>(value));
        };

        idsByName_.emplace(name, id);
        typeIdsByType_.emplace(type, id);
        descriptorsById_.emplace(id, std::move(descriptor));
        return id;
    }

    template <typename T>
    ComponentTypeId typeId() const {
        const auto it = typeIdsByType_.find(std::type_index(typeid(T)));
        if (it == typeIdsByType_.end()) {
            throw std::runtime_error("component type is not registered");
        }
        return it->second;
    }

    const ComponentDescriptor* findById(ComponentTypeId id) const;
    static ComponentRegistry& instance();

    ComponentTypeId nextId_ = 1;
    std::unordered_map<std::string, ComponentTypeId> idsByName_;
    std::unordered_map<std::type_index, ComponentTypeId> typeIdsByType_;
    std::unordered_map<ComponentTypeId, ComponentDescriptor> descriptorsById_;
};

class ComponentSet {
public:
    template <typename T>
    void set(T component) {
        values_[ComponentRegistry::TypeId<T>()] = std::move(component);
    }

    template <typename T>
    bool has() const {
        return values_.contains(ComponentRegistry::TypeId<T>());
    }

    template <typename T>
    const T* get() const {
        const auto it = values_.find(ComponentRegistry::TypeId<T>());
        if (it == values_.end()) {
            return nullptr;
        }
        return &std::any_cast<const T&>(it->second);
    }

    template <typename T>
    T* getMutable() {
        const auto it = values_.find(ComponentRegistry::TypeId<T>());
        if (it == values_.end()) {
            return nullptr;
        }
        return &std::any_cast<T&>(it->second);
    }

    bool has(ComponentTypeId id) const;
    const std::unordered_map<ComponentTypeId, std::any>& values() const;
    JsonValue toJson() const;
    static ComponentSet fromJson(const JsonValue& value, ComponentScope expectedScope);

private:
    std::unordered_map<ComponentTypeId, std::any> values_;
};

using DefinitionComponentSet = ComponentSet;
using RuntimeComponentSet = ComponentSet;

struct FoodComponent {
    int nutrition = 0;
    float saturation = 0.0f;
};

struct ToolComponent {
    std::string toolType;
    int harvestLevel = 0;
    float miningSpeed = 1.0f;
    int maxDurability = 0;
};

struct LightEmitterComponent {
    std::uint8_t level = 0;
    std::array<float, 3> color {1.0f, 1.0f, 1.0f};
};

struct HealthDefinitionComponent {
    int maxHealth = 20;
    float regenerationPerSecond = 0.0f;
};

struct AIBehaviorDefinitionComponent {
    std::string behavior;
    float aggroRange = 0.0f;
};

struct DurabilityStateComponent {
    int damage = 0;
};

struct InventoryStateComponent {
    struct Slot {
        std::string itemId;
        int count = 0;
    };
    std::vector<Slot> slots;
};

struct HealthStateComponent {
    int currentHealth = 20;
};

struct AIStateComponent {
    std::string currentGoal;
    float decisionCooldown = 0.0f;
};

struct ComposableItemDefinition {
    std::string id;
    std::string name;
    int stackSize = 64;
    DefinitionComponentSet components;
};

struct ComposableBlockDefinition {
    std::string id;
    std::string name;
    bool solid = true;
    DefinitionComponentSet components;
};

struct ComposableEntityDefinition {
    std::string id;
    std::string name;
    DefinitionComponentSet components;
};

struct DefinitionCatalog {
    std::unordered_map<std::string, ComposableItemDefinition> items;
    std::unordered_map<std::string, ComposableBlockDefinition> blocks;
    std::unordered_map<std::string, ComposableEntityDefinition> entities;
    bool frozen = false;

    const ComposableItemDefinition* findItem(const std::string& id) const;
    const ComposableBlockDefinition* findBlock(const std::string& id) const;
    const ComposableEntityDefinition* findEntity(const std::string& id) const;
    void freeze();
};

struct ItemStack {
    std::string definitionId;
    int count = 1;
    RuntimeComponentSet state;
};

struct RuntimeEntity {
    std::uint64_t id = 0;
    std::string definitionId;
    RuntimeComponentSet state;
};

struct BlockEntity {
    int x = 0;
    int y = 0;
    int z = 0;
    std::string blockId;
    RuntimeComponentSet state;
};

void registerBuiltinDataComponents();
DefinitionCatalog loadDefinitionCatalogFromJson(const JsonValue& value);
JsonValue serializeItemStack(const ItemStack& stack);
ItemStack deserializeItemStack(const JsonValue& value);
JsonValue serializeRuntimeEntity(const RuntimeEntity& entity);
RuntimeEntity deserializeRuntimeEntity(const JsonValue& value);

bool consumeFoodSystem(const DefinitionCatalog& catalog, ItemStack& stack, RuntimeEntity& consumer);
bool damageToolSystem(const DefinitionCatalog& catalog, ItemStack& stack, int amount);
void healthRegenerationSystem(const DefinitionCatalog& catalog, RuntimeEntity& entity, float dt);
void aiDecisionSystem(const DefinitionCatalog& catalog, RuntimeEntity& entity, float dt);
std::vector<std::string> collectLightEmitterBlocks(const DefinitionCatalog& catalog);

const char* exampleDefinitionJson();

}  // namespace voxel
