#include "data/DataComponents.hpp"

#include <iostream>
#include <string>

namespace {

bool expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "[FAIL] " << message << '\n';
        return false;
    }
    return true;
}

}  // namespace

int main() {
    bool ok = true;

    voxel::ComponentRegistry::ClearForTests();
    voxel::ComponentRegistry::Register<voxel::FoodComponent>("food");
    voxel::ComponentRegistry::Register<voxel::ToolComponent>("tool");
    voxel::ComponentRegistry::Register<voxel::LightEmitterComponent>("light_emitter");
    voxel::ComponentRegistry::Register<voxel::HealthDefinitionComponent>("health");
    voxel::ComponentRegistry::Register<voxel::AIBehaviorDefinitionComponent>("ai_behavior");
    voxel::ComponentRegistry::Register<voxel::DurabilityStateComponent>("durability_state", voxel::ComponentScope::Runtime);
    voxel::ComponentRegistry::Register<voxel::InventoryStateComponent>("inventory_state", voxel::ComponentScope::Runtime);
    voxel::ComponentRegistry::Register<voxel::HealthStateComponent>("health_state", voxel::ComponentScope::Runtime);
    voxel::ComponentRegistry::Register<voxel::AIStateComponent>("ai_state", voxel::ComponentScope::Runtime);

    const voxel::DefinitionCatalog catalog =
        voxel::loadDefinitionCatalogFromJson(voxel::parseJson(voxel::exampleDefinitionJson()));

    ok &= expect(catalog.frozen, "loaded definition catalog should be frozen after startup");

    const auto* apple = catalog.findItem("base:apple");
    ok &= expect(apple != nullptr && apple->components.has<voxel::FoodComponent>(),
                 "apple should load a typed food component");
    ok &= expect(apple != nullptr && apple->components.get<voxel::FoodComponent>()->nutrition == 4,
                 "food component should expose strongly typed fields");

    const auto* pickaxe = catalog.findItem("base:iron_pickaxe");
    ok &= expect(pickaxe != nullptr && pickaxe->components.has<voxel::ToolComponent>(),
                 "pickaxe should load a typed tool component");

    const auto lightBlocks = voxel::collectLightEmitterBlocks(catalog);
    ok &= expect(lightBlocks.size() == 1 && lightBlocks[0] == "base:glowstone",
                 "light system should query block definition components");

    voxel::RuntimeEntity zombie;
    zombie.id = 42;
    zombie.definitionId = "base:zombie";
    zombie.state.set(voxel::HealthStateComponent {10});
    zombie.state.set(voxel::AIStateComponent {});

    voxel::ItemStack appleStack;
    appleStack.definitionId = "base:apple";
    appleStack.count = 2;

    ok &= expect(voxel::consumeFoodSystem(catalog, appleStack, zombie),
                 "food system should consume an item with FoodComponent");
    ok &= expect(appleStack.count == 1, "food system should decrement stack count");
    ok &= expect(zombie.state.get<voxel::HealthStateComponent>()->currentHealth == 14,
                 "food system should mutate runtime health state");

    voxel::aiDecisionSystem(catalog, zombie, 1.0f);
    ok &= expect(zombie.state.get<voxel::AIStateComponent>()->currentGoal == "hostile_melee",
                 "AI system should derive behavior from immutable entity definition");

    voxel::ItemStack pickaxeStack;
    pickaxeStack.definitionId = "base:iron_pickaxe";
    pickaxeStack.count = 1;
    pickaxeStack.state.set(voxel::DurabilityStateComponent {240});
    ok &= expect(voxel::damageToolSystem(catalog, pickaxeStack, 10),
                 "tool system should report a broken tool at max damage");
    ok &= expect(pickaxeStack.state.get<voxel::DurabilityStateComponent>()->damage == 250,
                 "tool system should clamp durability damage");

    const voxel::JsonValue serializedStack = voxel::serializeItemStack(pickaxeStack);
    const voxel::ItemStack restoredStack = voxel::deserializeItemStack(serializedStack);
    ok &= expect(restoredStack.definitionId == "base:iron_pickaxe" &&
                     restoredStack.state.get<voxel::DurabilityStateComponent>()->damage == 250,
                 "item stack serialization should preserve runtime components");

    const voxel::JsonValue serializedEntity = voxel::serializeRuntimeEntity(zombie);
    const voxel::RuntimeEntity restoredEntity = voxel::deserializeRuntimeEntity(serializedEntity);
    ok &= expect(restoredEntity.id == 42 &&
                     restoredEntity.state.get<voxel::HealthStateComponent>()->currentHealth == 14 &&
                     restoredEntity.state.get<voxel::AIStateComponent>()->currentGoal == "hostile_melee",
                 "entity serialization should preserve mutable runtime state");

    if (!ok) {
        return 1;
    }

    std::cout << "Data component tests passed.\n";
    return 0;
}
