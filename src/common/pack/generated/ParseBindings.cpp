// GENERATED FILE — do not edit by hand.
// Source:      tools/codegen/schema/{block,item,biome}.js
// Regenerate:  node tools/codegen/generate.js

#include "pack/generated/ParseBindings.hpp"
#include "pack/JsHelpers.hpp"
#include "data/GameData.hpp"
#include "data/BiomeDefinition.hpp"
#include <quickjs.h>

namespace voxel {
// Custom parsers — defined in ScriptBindingsCustom.cpp
extern BlockTextures parseBlockTextures(JSContext*, JSValueConst);
extern std::vector<BlockDrop> parseBlockDrops(JSContext*, JSValueConst);
extern std::unordered_map<std::string, BlockProperty> parseBlockProperties(JSContext*, JSValueConst);
extern BiomeClimateRange parseClimateRange(JSContext*, JSValueConst, const char*);
extern std::unordered_map<std::string, std::array<float, 3>> parseBiomeColors(JSContext*, JSValueConst);
extern std::vector<std::string> parseStringArray(JSContext*, JSValueConst);

namespace generated {

BlockDefinition parseBlockDefinition(JSContext* ctx, JSValueConst obj) {
    BlockDefinition out{};

    out.id = voxel::js::jsStr  (ctx, obj, "id");
    out.name = voxel::js::jsStr  (ctx, obj, "name");
    out.drops = parseBlockDrops(ctx, obj);
    out.properties = parseBlockProperties(ctx, obj);

    // voxel
    {
        JSValue sub = JS_GetPropertyStr(ctx, obj, "voxel");
        out.solid = voxel::js::jsBool (ctx, sub, "solid", false);
        out.translucent = voxel::js::jsBool (ctx, sub, "translucent", false);
        out.material = voxel::js::jsStr  (ctx, sub, "material", "terrain");
        JS_FreeValue(ctx, sub);
    }

    // render
    {
        JSValue sub = JS_GetPropertyStr(ctx, obj, "render");
        out.color = voxel::js::jsColor(ctx, sub, "color", {1.0f, 1.0f, 1.0f});
        out.opacity = voxel::js::jsFloat(ctx, sub, "opacity", 1.0f);
        out.tintKey = voxel::js::jsBool (ctx, sub, "tintKey", false);
        out.renderType = voxel::js::jsStr  (ctx, sub, "type", "cube");
        out.modelPath = voxel::js::jsStr  (ctx, sub, "model", "");
        out.textures = parseBlockTextures(ctx, sub);
        JS_FreeValue(ctx, sub);
    }

    return out;
}

ItemDefinition parseItemDefinition(JSContext* ctx, JSValueConst obj) {
    ItemDefinition out{};

    out.id = voxel::js::jsStr  (ctx, obj, "id");
    out.name = voxel::js::jsStr  (ctx, obj, "name");
    out.stackSize = voxel::js::jsInt  (ctx, obj, "stackSize", 1);
    out.icon = voxel::js::jsStr  (ctx, obj, "icon", "");
    out.placeableBlock = voxel::js::jsOptStr(ctx, obj, "placeableBlock");

    return out;
}

BiomeDefinition parseBiomeDefinition(JSContext* ctx, JSValueConst obj) {
    BiomeDefinition out{};

    out.id = voxel::js::jsStr  (ctx, obj, "id");
    out.name = voxel::js::jsStr  (ctx, obj, "name");
    out.priority = voxel::js::jsFloat(ctx, obj, "priority", 0.0f);
    out.rarity = voxel::js::jsFloat(ctx, obj, "rarity", 1.0f);
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
        out.surface.middle = voxel::js::jsStr  (ctx, sub, "middle", "base:dirt");
        out.surface.base = voxel::js::jsStr  (ctx, sub, "base", "base:stone");
        out.surface.middleDepth = voxel::js::jsInt  (ctx, sub, "middleDepth", 3);
        JS_FreeValue(ctx, sub);
    }

    // atmosphere
    {
        JSValue sub = JS_GetPropertyStr(ctx, obj, "atmosphere");
        out.atmosphere.skyColor = voxel::js::jsColor(ctx, sub, "skyColor", {0.58f, 0.78f, 0.98f});
        out.atmosphere.fogColor = voxel::js::jsColor(ctx, sub, "fogColor", {0.75f, 0.85f, 0.95f});
        out.atmosphere.waterColor = voxel::js::jsColor(ctx, sub, "waterColor", {0.2f, 0.45f, 0.8f});
        JS_FreeValue(ctx, sub);
    }

    // fertility
    {
        JSValue sub = JS_GetPropertyStr(ctx, obj, "fertility");
        out.fertility.nitrogen = voxel::js::jsFloat(ctx, sub, "nitrogen", 0.5f);
        out.fertility.phosphorus = voxel::js::jsFloat(ctx, sub, "phosphorus", 0.5f);
        out.fertility.potassium = voxel::js::jsFloat(ctx, sub, "potassium", 0.5f);
        out.fertility.magnesium = voxel::js::jsFloat(ctx, sub, "magnesium", 0.5f);
        out.fertility.calcium = voxel::js::jsFloat(ctx, sub, "calcium", 0.5f);
        out.fertility.sulfur = voxel::js::jsFloat(ctx, sub, "sulfur", 0.2f);
        JS_FreeValue(ctx, sub);
    }

    return out;
}

RecipeDefinition parseRecipeDefinition(JSContext* ctx, JSValueConst obj) {
    RecipeDefinition out{};

    out.id = voxel::js::jsStr  (ctx, obj, "id");
    out.type = voxel::js::jsStr  (ctx, obj, "type");
    out.output = voxel::js::jsStr  (ctx, obj, "output");
    out.count = voxel::js::jsInt  (ctx, obj, "count", 1);
    out.ingredients = parseStringArray(ctx, obj);

    return out;
}

} // namespace generated
} // namespace voxel
