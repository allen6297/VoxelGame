#include "data/DataComponents.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace voxel {
namespace {

const JsonValue::Object& requireObject(const JsonValue& value, const std::string& context) {
    if (!value.isObject()) {
        throw std::runtime_error(context + " must be a JSON object");
    }
    return value.asObject();
}

const JsonValue::Array& requireArray(const JsonValue& value, const std::string& context) {
    if (!value.isArray()) {
        throw std::runtime_error(context + " must be a JSON array");
    }
    return value.asArray();
}

const JsonValue* findField(const JsonValue::Object& object, const std::string& key) {
    const auto it = object.find(key);
    if (it == object.end()) {
        return nullptr;
    }
    return &it->second;
}

std::string readString(const JsonValue::Object& object, const std::string& key, const std::string& fallback = {}) {
    const auto* field = findField(object, key);
    if (field == nullptr) {
        return fallback;
    }
    return field->asString();
}

int readInt(const JsonValue::Object& object, const std::string& key, const int fallback = 0) {
    const auto* field = findField(object, key);
    if (field == nullptr) {
        return fallback;
    }
    return static_cast<int>(std::lround(field->asNumber()));
}

float readFloat(const JsonValue::Object& object, const std::string& key, const float fallback = 0.0f) {
    const auto* field = findField(object, key);
    if (field == nullptr) {
        return fallback;
    }
    return static_cast<float>(field->asNumber());
}

bool readBool(const JsonValue::Object& object, const std::string& key, const bool fallback = false) {
    const auto* field = findField(object, key);
    if (field == nullptr) {
        return fallback;
    }
    return field->asBool();
}

JsonValue object(std::initializer_list<std::pair<const std::string, JsonValue>> fields) {
    return JsonValue {JsonValue::Object(fields)};
}

JsonValue array(std::initializer_list<JsonValue> values) {
    return JsonValue {JsonValue::Array(values)};
}

std::array<float, 3> readColor(const JsonValue::Object& object) {
    std::array<float, 3> color {1.0f, 1.0f, 1.0f};
    const auto* field = findField(object, "color");
    if (field == nullptr) {
        return color;
    }
    const auto& values = requireArray(*field, "color");
    if (values.size() != color.size()) {
        throw std::runtime_error("color must contain exactly three numbers");
    }
    for (std::size_t i = 0; i < values.size(); ++i) {
        color[i] = static_cast<float>(values[i].asNumber());
    }
    return color;
}

JsonValue colorToJson(const std::array<float, 3>& color) {
    return array({JsonValue {static_cast<double>(color[0])},
                  JsonValue {static_cast<double>(color[1])},
                  JsonValue {static_cast<double>(color[2])}});
}

RuntimeComponentSet readRuntimeState(const JsonValue::Object& object) {
    const auto* field = findField(object, "state");
    if (field == nullptr) {
        return {};
    }
    return RuntimeComponentSet::fromJson(*field, ComponentScope::Runtime);
}

JsonValue runtimeStateJson(const RuntimeComponentSet& state) {
    return state.toJson();
}

}  // namespace

ComponentRegistry& ComponentRegistry::instance() {
    static ComponentRegistry registry;
    return registry;
}

const ComponentDescriptor* ComponentRegistry::findById(const ComponentTypeId id) const {
    const auto it = descriptorsById_.find(id);
    if (it == descriptorsById_.end()) {
        return nullptr;
    }
    return &it->second;
}

const ComponentDescriptor* ComponentRegistry::FindByName(const std::string& name) {
    const auto& registry = instance();
    const auto it = registry.idsByName_.find(name);
    if (it == registry.idsByName_.end()) {
        return nullptr;
    }
    return registry.findById(it->second);
}

const ComponentDescriptor* ComponentRegistry::FindById(const ComponentTypeId id) {
    return instance().findById(id);
}

void ComponentRegistry::ClearForTests() {
    instance() = ComponentRegistry {};
}

bool ComponentSet::has(const ComponentTypeId id) const {
    return values_.contains(id);
}

const std::unordered_map<ComponentTypeId, std::any>& ComponentSet::values() const {
    return values_;
}

JsonValue ComponentSet::toJson() const {
    JsonValue::Object result;
    for (const auto& [id, value] : values_) {
        const auto* descriptor = ComponentRegistry::FindById(id);
        if (descriptor == nullptr) {
            throw std::runtime_error("cannot serialize unknown component id");
        }
        result.emplace(descriptor->name, descriptor->toJson(value));
    }
    return JsonValue {result};
}

ComponentSet ComponentSet::fromJson(const JsonValue& value, const ComponentScope expectedScope) {
    ComponentSet set;
    const auto& objectValue = requireObject(value, "components");
    for (const auto& [name, componentJson] : objectValue) {
        const auto* descriptor = ComponentRegistry::FindByName(name);
        if (descriptor == nullptr) {
            throw std::runtime_error("unknown component: " + name);
        }
        if (descriptor->scope != expectedScope) {
            throw std::runtime_error("component used in the wrong scope: " + name);
        }
        set.values_.emplace(descriptor->id, descriptor->fromJson(componentJson));
    }
    return set;
}

template <>
struct ComponentJson<FoodComponent> {
    static FoodComponent fromJson(const JsonValue& value) {
        const auto& objectValue = requireObject(value, "food");
        return {
            readInt(objectValue, "nutrition"),
            readFloat(objectValue, "saturation")
        };
    }

    static JsonValue toJson(const FoodComponent& component) {
        return object({{"nutrition", JsonValue {static_cast<double>(component.nutrition)}},
                       {"saturation", JsonValue {static_cast<double>(component.saturation)}}});
    }
};

template <>
struct ComponentJson<ToolComponent> {
    static ToolComponent fromJson(const JsonValue& value) {
        const auto& objectValue = requireObject(value, "tool");
        return {
            readString(objectValue, "toolType"),
            readInt(objectValue, "harvestLevel"),
            readFloat(objectValue, "miningSpeed", 1.0f),
            readInt(objectValue, "maxDurability")
        };
    }

    static JsonValue toJson(const ToolComponent& component) {
        return object({{"toolType", JsonValue {component.toolType}},
                       {"harvestLevel", JsonValue {static_cast<double>(component.harvestLevel)}},
                       {"miningSpeed", JsonValue {static_cast<double>(component.miningSpeed)}},
                       {"maxDurability", JsonValue {static_cast<double>(component.maxDurability)}}});
    }
};

template <>
struct ComponentJson<LightEmitterComponent> {
    static LightEmitterComponent fromJson(const JsonValue& value) {
        const auto& objectValue = requireObject(value, "light_emitter");
        return {
            static_cast<std::uint8_t>(readInt(objectValue, "level")),
            readColor(objectValue)
        };
    }

    static JsonValue toJson(const LightEmitterComponent& component) {
        return object({{"level", JsonValue {static_cast<double>(component.level)}},
                       {"color", colorToJson(component.color)}});
    }
};

template <>
struct ComponentJson<HealthDefinitionComponent> {
    static HealthDefinitionComponent fromJson(const JsonValue& value) {
        const auto& objectValue = requireObject(value, "health");
        return {
            readInt(objectValue, "maxHealth", 20),
            readFloat(objectValue, "regenerationPerSecond")
        };
    }

    static JsonValue toJson(const HealthDefinitionComponent& component) {
        return object({{"maxHealth", JsonValue {static_cast<double>(component.maxHealth)}},
                       {"regenerationPerSecond", JsonValue {static_cast<double>(component.regenerationPerSecond)}}});
    }
};

template <>
struct ComponentJson<AIBehaviorDefinitionComponent> {
    static AIBehaviorDefinitionComponent fromJson(const JsonValue& value) {
        const auto& objectValue = requireObject(value, "ai_behavior");
        return {
            readString(objectValue, "behavior"),
            readFloat(objectValue, "aggroRange")
        };
    }

    static JsonValue toJson(const AIBehaviorDefinitionComponent& component) {
        return object({{"behavior", JsonValue {component.behavior}},
                       {"aggroRange", JsonValue {static_cast<double>(component.aggroRange)}}});
    }
};

template <>
struct ComponentJson<DurabilityStateComponent> {
    static DurabilityStateComponent fromJson(const JsonValue& value) {
        return {readInt(requireObject(value, "durability_state"), "damage")};
    }

    static JsonValue toJson(const DurabilityStateComponent& component) {
        return object({{"damage", JsonValue {static_cast<double>(component.damage)}}});
    }
};

template <>
struct ComponentJson<InventoryStateComponent> {
    static InventoryStateComponent fromJson(const JsonValue& value) {
        InventoryStateComponent component;
        const auto& objectValue = requireObject(value, "inventory_state");
        const auto* slots = findField(objectValue, "slots");
        if (slots == nullptr) {
            return component;
        }
        for (const auto& slotJson : requireArray(*slots, "inventory_state.slots")) {
            const auto& slotObject = requireObject(slotJson, "inventory slot");
            component.slots.push_back({
                readString(slotObject, "itemId"),
                readInt(slotObject, "count")
            });
        }
        return component;
    }

    static JsonValue toJson(const InventoryStateComponent& component) {
        JsonValue::Array slots;
        for (const auto& slot : component.slots) {
            slots.push_back(object({{"itemId", JsonValue {slot.itemId}},
                                    {"count", JsonValue {static_cast<double>(slot.count)}}}));
        }
        return object({{"slots", JsonValue {slots}}});
    }
};

template <>
struct ComponentJson<HealthStateComponent> {
    static HealthStateComponent fromJson(const JsonValue& value) {
        return {readInt(requireObject(value, "health_state"), "currentHealth", 20)};
    }

    static JsonValue toJson(const HealthStateComponent& component) {
        return object({{"currentHealth", JsonValue {static_cast<double>(component.currentHealth)}}});
    }
};

template <>
struct ComponentJson<AIStateComponent> {
    static AIStateComponent fromJson(const JsonValue& value) {
        const auto& objectValue = requireObject(value, "ai_state");
        return {
            readString(objectValue, "currentGoal"),
            readFloat(objectValue, "decisionCooldown")
        };
    }

    static JsonValue toJson(const AIStateComponent& component) {
        return object({{"currentGoal", JsonValue {component.currentGoal}},
                       {"decisionCooldown", JsonValue {static_cast<double>(component.decisionCooldown)}}});
    }
};

const ComposableItemDefinition* DefinitionCatalog::findItem(const std::string& id) const {
    const auto it = items.find(id);
    return it == items.end() ? nullptr : &it->second;
}

const ComposableBlockDefinition* DefinitionCatalog::findBlock(const std::string& id) const {
    const auto it = blocks.find(id);
    return it == blocks.end() ? nullptr : &it->second;
}

const ComposableEntityDefinition* DefinitionCatalog::findEntity(const std::string& id) const {
    const auto it = entities.find(id);
    return it == entities.end() ? nullptr : &it->second;
}

void DefinitionCatalog::freeze() {
    frozen = true;
}

void registerBuiltinDataComponents() {
    ComponentRegistry::Register<FoodComponent>("food", ComponentScope::Definition);
    ComponentRegistry::Register<ToolComponent>("tool", ComponentScope::Definition);
    ComponentRegistry::Register<LightEmitterComponent>("light_emitter", ComponentScope::Definition);
    ComponentRegistry::Register<HealthDefinitionComponent>("health", ComponentScope::Definition);
    ComponentRegistry::Register<AIBehaviorDefinitionComponent>("ai_behavior", ComponentScope::Definition);
    ComponentRegistry::Register<DurabilityStateComponent>("durability_state", ComponentScope::Runtime);
    ComponentRegistry::Register<InventoryStateComponent>("inventory_state", ComponentScope::Runtime);
    ComponentRegistry::Register<HealthStateComponent>("health_state", ComponentScope::Runtime);
    ComponentRegistry::Register<AIStateComponent>("ai_state", ComponentScope::Runtime);
}

DefinitionCatalog loadDefinitionCatalogFromJson(const JsonValue& value) {
    DefinitionCatalog catalog;
    const auto& root = requireObject(value, "definition catalog");

    if (const auto* items = findField(root, "items")) {
        for (const auto& itemJson : requireArray(*items, "items")) {
            const auto& itemObject = requireObject(itemJson, "item definition");
            ComposableItemDefinition item;
            item.id = readString(itemObject, "id");
            item.name = readString(itemObject, "name", item.id);
            item.stackSize = readInt(itemObject, "stackSize", 64);
            if (const auto* components = findField(itemObject, "components")) {
                item.components = DefinitionComponentSet::fromJson(*components, ComponentScope::Definition);
            }
            catalog.items.emplace(item.id, std::move(item));
        }
    }

    if (const auto* blocks = findField(root, "blocks")) {
        for (const auto& blockJson : requireArray(*blocks, "blocks")) {
            const auto& blockObject = requireObject(blockJson, "block definition");
            ComposableBlockDefinition block;
            block.id = readString(blockObject, "id");
            block.name = readString(blockObject, "name", block.id);
            block.solid = readBool(blockObject, "solid", true);
            if (const auto* components = findField(blockObject, "components")) {
                block.components = DefinitionComponentSet::fromJson(*components, ComponentScope::Definition);
            }
            catalog.blocks.emplace(block.id, std::move(block));
        }
    }

    if (const auto* entities = findField(root, "entities")) {
        for (const auto& entityJson : requireArray(*entities, "entities")) {
            const auto& entityObject = requireObject(entityJson, "entity definition");
            ComposableEntityDefinition entity;
            entity.id = readString(entityObject, "id");
            entity.name = readString(entityObject, "name", entity.id);
            if (const auto* components = findField(entityObject, "components")) {
                entity.components = DefinitionComponentSet::fromJson(*components, ComponentScope::Definition);
            }
            catalog.entities.emplace(entity.id, std::move(entity));
        }
    }

    catalog.freeze();
    return catalog;
}

JsonValue serializeItemStack(const ItemStack& stack) {
    return object({{"definitionId", JsonValue {stack.definitionId}},
                   {"count", JsonValue {static_cast<double>(stack.count)}},
                   {"state", runtimeStateJson(stack.state)}});
}

ItemStack deserializeItemStack(const JsonValue& value) {
    const auto& objectValue = requireObject(value, "item stack");
    return {
        readString(objectValue, "definitionId"),
        readInt(objectValue, "count", 1),
        readRuntimeState(objectValue)
    };
}

JsonValue serializeRuntimeEntity(const RuntimeEntity& entity) {
    return object({{"id", JsonValue {static_cast<double>(entity.id)}},
                   {"definitionId", JsonValue {entity.definitionId}},
                   {"state", runtimeStateJson(entity.state)}});
}

RuntimeEntity deserializeRuntimeEntity(const JsonValue& value) {
    const auto& objectValue = requireObject(value, "runtime entity");
    return {
        static_cast<std::uint64_t>(readInt(objectValue, "id")),
        readString(objectValue, "definitionId"),
        readRuntimeState(objectValue)
    };
}

bool consumeFoodSystem(const DefinitionCatalog& catalog, ItemStack& stack, RuntimeEntity& consumer) {
    const auto* item = catalog.findItem(stack.definitionId);
    if (item == nullptr || stack.count <= 0) {
        return false;
    }
    const auto* food = item->components.get<FoodComponent>();
    auto* health = consumer.state.getMutable<HealthStateComponent>();
    if (food == nullptr || health == nullptr) {
        return false;
    }

    const auto* entityDef = catalog.findEntity(consumer.definitionId);
    const auto* healthDef = entityDef == nullptr ? nullptr : entityDef->components.get<HealthDefinitionComponent>();
    const int maxHealth = healthDef == nullptr ? health->currentHealth + food->nutrition : healthDef->maxHealth;
    health->currentHealth = std::min(maxHealth, health->currentHealth + food->nutrition);
    --stack.count;
    return true;
}

bool damageToolSystem(const DefinitionCatalog& catalog, ItemStack& stack, const int amount) {
    const auto* item = catalog.findItem(stack.definitionId);
    if (item == nullptr) {
        return false;
    }
    const auto* tool = item->components.get<ToolComponent>();
    auto* durability = stack.state.getMutable<DurabilityStateComponent>();
    if (tool == nullptr || durability == nullptr || tool->maxDurability <= 0) {
        return false;
    }
    durability->damage = std::min(tool->maxDurability, durability->damage + std::max(0, amount));
    return durability->damage >= tool->maxDurability;
}

void healthRegenerationSystem(const DefinitionCatalog& catalog, RuntimeEntity& entity, const float dt) {
    const auto* entityDef = catalog.findEntity(entity.definitionId);
    if (entityDef == nullptr) {
        return;
    }
    const auto* healthDef = entityDef->components.get<HealthDefinitionComponent>();
    auto* health = entity.state.getMutable<HealthStateComponent>();
    if (healthDef == nullptr || health == nullptr || healthDef->regenerationPerSecond <= 0.0f) {
        return;
    }
    const int restored = static_cast<int>(std::floor(healthDef->regenerationPerSecond * dt));
    health->currentHealth = std::min(healthDef->maxHealth, health->currentHealth + restored);
}

void aiDecisionSystem(const DefinitionCatalog& catalog, RuntimeEntity& entity, const float dt) {
    const auto* entityDef = catalog.findEntity(entity.definitionId);
    if (entityDef == nullptr) {
        return;
    }
    const auto* behavior = entityDef->components.get<AIBehaviorDefinitionComponent>();
    auto* state = entity.state.getMutable<AIStateComponent>();
    if (behavior == nullptr || state == nullptr) {
        return;
    }
    state->decisionCooldown = std::max(0.0f, state->decisionCooldown - dt);
    if (state->decisionCooldown == 0.0f && state->currentGoal.empty()) {
        state->currentGoal = behavior->behavior;
        state->decisionCooldown = 1.0f;
    }
}

std::vector<std::string> collectLightEmitterBlocks(const DefinitionCatalog& catalog) {
    std::vector<std::string> result;
    for (const auto& [id, block] : catalog.blocks) {
        if (block.components.has<LightEmitterComponent>()) {
            result.push_back(id);
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

const char* exampleDefinitionJson() {
    return R"json({
  "items": [
    {
      "id": "base:apple",
      "name": "Apple",
      "stackSize": 64,
      "components": {
        "food": { "nutrition": 4, "saturation": 2.4 }
      }
    },
    {
      "id": "base:iron_pickaxe",
      "name": "Iron Pickaxe",
      "stackSize": 1,
      "components": {
        "tool": {
          "toolType": "pickaxe",
          "harvestLevel": 2,
          "miningSpeed": 6.0,
          "maxDurability": 250
        }
      }
    }
  ],
  "blocks": [
    {
      "id": "base:glowstone",
      "name": "Glowstone",
      "solid": true,
      "components": {
        "light_emitter": { "level": 15, "color": [1.0, 0.86, 0.55] }
      }
    }
  ],
  "entities": [
    {
      "id": "base:zombie",
      "name": "Zombie",
      "components": {
        "health": { "maxHealth": 20, "regenerationPerSecond": 0.0 },
        "ai_behavior": { "behavior": "hostile_melee", "aggroRange": 16.0 }
      }
    }
  ]
})json";
}

}  // namespace voxel
