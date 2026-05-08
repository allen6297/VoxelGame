// GENERATED FILE — do not edit by hand.
// Source:      tools/codegen/schema/{biome,block,item,recipe,tag}.js
// Regenerate:  node tools/codegen/generate.js

#include "pack/generated/ParseBindings.hpp"
#include "pack/JsHelpers.hpp"
#include "data/GameData.hpp"
#include "data/BiomeDefinition.hpp"
#include <quickjs.h>

namespace voxel {
// Custom parsers — defined in ScriptBindingsCustom.cpp
extern BiomeClimateRange parseClimateRange(JSContext*, JSValueConst, const char*);
extern std::unordered_map<std::string, std::array<float, 3>> parseBiomeColors(JSContext*, JSValueConst);
extern BlockTextures parseBlockTextures(JSContext*, JSValueConst);
extern std::vector<BlockDrop> parseBlockDrops(JSContext*, JSValueConst);
extern std::unordered_map<std::string, BlockProperty> parseBlockProperties(JSContext*, JSValueConst);

namespace generated {

BiomeDefinition parseBiomeDefinition(JSContext* ctx, JSValueConst obj) {
    BiomeDefinition out{};

    voxel::js::jsRequire(ctx, obj, "id");
    out.id = voxel::js::jsStr  (ctx, obj, "id");
    voxel::js::jsValidate(ctx, "id", out.id, "biome_id");
    voxel::js::jsRequire(ctx, obj, "name");
    out.name = voxel::js::jsStr  (ctx, obj, "name");
    out.priority = voxel::js::jsFloat(ctx, obj, "priority", 0.0f);
    voxel::js::jsValidate(ctx, "priority", out.priority, "non_negative_float");
    out.rarity = voxel::js::jsFloat(ctx, obj, "rarity", 1.0f);
    voxel::js::jsValidate(ctx, "rarity", out.rarity, "non_negative_float");
    out.colors = parseBiomeColors(ctx, obj);

    // climate
    {
        JSValue sub = JS_GetPropertyStr(ctx, obj, "climate");
        out.climate.temperature = parseClimateRange(ctx, sub, "temperature");
        out.climate.humidity = parseClimateRange(ctx, sub, "humidity");
        out.climate.rainfall = parseClimateRange(ctx, sub, "rainfall");
        out.climate.elevation = parseClimateRange(ctx, sub, "elevation");
        out.climate.drainage = parseClimateRange(ctx, sub, "drainage");
        out.climate.waterTable = parseClimateRange(ctx, sub, "waterTable");
        JS_FreeValue(ctx, sub);
    }

    // terrain
    {
        JSValue sub = JS_GetPropertyStr(ctx, obj, "terrain");
        out.terrain.baseHeight = voxel::js::jsFloat(ctx, sub, "baseHeight", 48.0f);
        out.terrain.heightVariation = voxel::js::jsFloat(ctx, sub, "heightVariation", 12.0f);
        JS_FreeValue(ctx, sub);
    }

    // surface
    {
        JSValue sub = JS_GetPropertyStr(ctx, obj, "surface");
        out.surface.top = voxel::js::jsStr  (ctx, sub, "top", "base:grass");
        voxel::js::jsValidate(ctx, "surface.top", out.surface.top, "block_id");
        out.surface.middle = voxel::js::jsStr  (ctx, sub, "middle", "base:dirt");
        voxel::js::jsValidate(ctx, "surface.middle", out.surface.middle, "block_id");
        out.surface.base = voxel::js::jsStr  (ctx, sub, "base", "base:stone");
        voxel::js::jsValidate(ctx, "surface.base", out.surface.base, "block_id");
        out.surface.middleDepth = voxel::js::jsInt  (ctx, sub, "middleDepth", 3);
        voxel::js::jsValidate(ctx, "surface.middleDepth", out.surface.middleDepth, "positive_int");
        JS_FreeValue(ctx, sub);
    }

    // atmosphere
    {
        JSValue sub = JS_GetPropertyStr(ctx, obj, "atmosphere");
        out.atmosphere.skyColor = voxel::js::jsColor(ctx, sub, "skyColor", {0.58f, 0.78f, 0.98f});
        voxel::js::jsValidate(ctx, "atmosphere.skyColor", out.atmosphere.skyColor, "rgb_0_1");
        out.atmosphere.fogColor = voxel::js::jsColor(ctx, sub, "fogColor", {0.75f, 0.85f, 0.95f});
        voxel::js::jsValidate(ctx, "atmosphere.fogColor", out.atmosphere.fogColor, "rgb_0_1");
        out.atmosphere.waterColor = voxel::js::jsColor(ctx, sub, "waterColor", {0.2f, 0.45f, 0.8f});
        voxel::js::jsValidate(ctx, "atmosphere.waterColor", out.atmosphere.waterColor, "rgb_0_1");
        JS_FreeValue(ctx, sub);
    }

    // fertility
    {
        JSValue sub = JS_GetPropertyStr(ctx, obj, "fertility");
        out.fertility.nitrogen = voxel::js::jsFloat(ctx, sub, "nitrogen", 0.5f);
        voxel::js::jsValidate(ctx, "fertility.nitrogen", out.fertility.nitrogen, "non_negative_float");
        out.fertility.phosphorus = voxel::js::jsFloat(ctx, sub, "phosphorus", 0.5f);
        voxel::js::jsValidate(ctx, "fertility.phosphorus", out.fertility.phosphorus, "non_negative_float");
        out.fertility.potassium = voxel::js::jsFloat(ctx, sub, "potassium", 0.5f);
        voxel::js::jsValidate(ctx, "fertility.potassium", out.fertility.potassium, "non_negative_float");
        out.fertility.magnesium = voxel::js::jsFloat(ctx, sub, "magnesium", 0.5f);
        voxel::js::jsValidate(ctx, "fertility.magnesium", out.fertility.magnesium, "non_negative_float");
        out.fertility.calcium = voxel::js::jsFloat(ctx, sub, "calcium", 0.5f);
        voxel::js::jsValidate(ctx, "fertility.calcium", out.fertility.calcium, "non_negative_float");
        out.fertility.sulfur = voxel::js::jsFloat(ctx, sub, "sulfur", 0.2f);
        voxel::js::jsValidate(ctx, "fertility.sulfur", out.fertility.sulfur, "non_negative_float");
        JS_FreeValue(ctx, sub);
    }

    return out;
}

BlockDefinition parseBlockDefinition(JSContext* ctx, JSValueConst obj) {
    BlockDefinition out{};

    voxel::js::jsRequire(ctx, obj, "id");
    out.id = voxel::js::jsStr  (ctx, obj, "id");
    voxel::js::jsValidate(ctx, "id", out.id, "block_id");
    voxel::js::jsRequire(ctx, obj, "name");
    out.name = voxel::js::jsStr  (ctx, obj, "name");
    out.runtimeOrder = voxel::js::jsInt  (ctx, obj, "runtimeOrder", 1000);
    out.drops = parseBlockDrops(ctx, obj);
    out.properties = parseBlockProperties(ctx, obj);

    // voxel
    {
        JSValue sub = JS_GetPropertyStr(ctx, obj, "voxel");
        out.solid = voxel::js::jsBool (ctx, sub, "solid", false);
        out.translucent = voxel::js::jsBool (ctx, sub, "translucent", false);
        out.material = voxel::js::jsEnum (ctx, sub, "material", {"terrain", "rock", "liquid", "plant"}, "terrain");
        JS_FreeValue(ctx, sub);
    }

    // render
    {
        JSValue sub = JS_GetPropertyStr(ctx, obj, "render");
        out.color = voxel::js::jsColor(ctx, sub, "color", {1.0f, 1.0f, 1.0f});
        voxel::js::jsValidate(ctx, "render.color", out.color, "rgb_0_1");
        out.opacity = voxel::js::jsFloat(ctx, sub, "opacity", 1.0f);
        voxel::js::jsValidate(ctx, "render.opacity", out.opacity, "opacity_0_1");
        out.tintKey = voxel::js::jsBool (ctx, sub, "tintKey", false);
        out.renderType = voxel::js::jsEnum (ctx, sub, "type", {"cube", "model"}, "cube");
        out.modelPath = voxel::js::jsStr  (ctx, sub, "model", "");
        voxel::js::jsValidate(ctx, "render.model", out.modelPath, "model_path");
        out.textures = parseBlockTextures(ctx, sub);
        JS_FreeValue(ctx, sub);
    }

    return out;
}

ItemDefinition parseItemDefinition(JSContext* ctx, JSValueConst obj) {
    ItemDefinition out{};

    voxel::js::jsRequire(ctx, obj, "id");
    out.id = voxel::js::jsStr  (ctx, obj, "id");
    voxel::js::jsValidate(ctx, "id", out.id, "item_id");
    voxel::js::jsRequire(ctx, obj, "name");
    out.name = voxel::js::jsStr  (ctx, obj, "name");
    out.stackSize = voxel::js::jsInt  (ctx, obj, "stackSize", 1);
    voxel::js::jsValidate(ctx, "stackSize", out.stackSize, "positive_int");
    out.icon = voxel::js::jsStr  (ctx, obj, "icon", "");
    voxel::js::jsValidate(ctx, "icon", out.icon, "texture_path");
    out.placeableBlock = voxel::js::jsOptStr(ctx, obj, "placeableBlock");
    voxel::js::jsValidate(ctx, "placeableBlock", out.placeableBlock, "block_id");

    return out;
}

RecipeDefinition parseRecipeDefinition(JSContext* ctx, JSValueConst obj) {
    RecipeDefinition out{};

    voxel::js::jsRequire(ctx, obj, "id");
    out.id = voxel::js::jsStr  (ctx, obj, "id");
    voxel::js::jsValidate(ctx, "id", out.id, "recipe_id");
    voxel::js::jsRequire(ctx, obj, "type");
    out.type = voxel::js::jsEnum (ctx, obj, "type", {"crafting", "smelting"}, "");
    voxel::js::jsRequire(ctx, obj, "output");
    out.output = voxel::js::jsStr  (ctx, obj, "output");
    voxel::js::jsValidate(ctx, "output", out.output, "item_id");
    out.count = voxel::js::jsInt  (ctx, obj, "count", 1);
    voxel::js::jsValidate(ctx, "count", out.count, "positive_int");
    out.ingredients = voxel::js::jsStringArray(ctx, obj, "ingredients");
    voxel::js::jsValidate(ctx, "ingredients", out.ingredients, "item_id");

    return out;
}

TagDefinition parseTagDefinition(JSContext* ctx, JSValueConst obj) {
    TagDefinition out{};

    voxel::js::jsRequire(ctx, obj, "id");
    out.id = voxel::js::jsStr  (ctx, obj, "id");
    voxel::js::jsValidate(ctx, "id", out.id, "tag_id");
    out.description = voxel::js::jsStr  (ctx, obj, "description", "");
    out.members = voxel::js::jsStringArray(ctx, obj, "members");

    return out;
}

} // namespace generated
} // namespace voxel
