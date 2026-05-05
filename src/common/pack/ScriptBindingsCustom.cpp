// Custom parse helpers called by generated/ParseBindings.cpp.
// These are defined here (not in ScriptManager.cpp) so they can be linked
// into the same executable as ParseBindings.cpp as non-static functions.

#include "pack/JsHelpers.hpp"
#include "data/GameData.hpp"
#include "data/BiomeDefinition.hpp"
#include <quickjs.h>

#include <array>
#include <string>
#include <unordered_map>
#include <vector>

namespace voxel {

BlockTextures parseBlockTextures(JSContext* ctx, JSValueConst render) {
    using namespace voxel::js;
    BlockTextures t;
    JSValue tex = JS_GetPropertyStr(ctx, render, "texture");
    if (!JS_IsUndefined(tex) && !JS_IsNull(tex)) {
        if (JS_IsString(tex)) {
            const char* s = JS_ToCString(ctx, tex);
            if (s) t.albedo = std::string(s);
            JS_FreeCString(ctx, s);
        } else {
            std::string albedo    = jsStr(ctx, tex, "albedo");
            std::string normal    = jsStr(ctx, tex, "normal");
            std::string roughness = jsStr(ctx, tex, "roughness");
            std::string emissive  = jsStr(ctx, tex, "emissive");
            if (!albedo.empty())    t.albedo    = albedo;
            if (!normal.empty())    t.normal    = normal;
            if (!roughness.empty()) t.roughness = roughness;
            if (!emissive.empty())  t.emissive  = emissive;
        }
    }
    JS_FreeValue(ctx, tex);
    return t;
}

std::vector<BlockDrop> parseBlockDrops(JSContext* ctx, JSValueConst obj) {
    using namespace voxel::js;
    std::vector<BlockDrop> drops;
    JSValue arr = JS_GetPropertyStr(ctx, obj, "drops");
    if (!JS_IsUndefined(arr) && !JS_IsNull(arr)) {
        uint32_t len = jsArrayLen(ctx, arr);
        for (uint32_t i = 0; i < len; ++i) {
            JSValue entry = JS_GetPropertyUint32(ctx, arr, i);
            BlockDrop drop;
            drop.item  = jsStr(ctx, entry, "item");
            drop.count = jsInt(ctx, entry, "count", 1);
            drops.push_back(std::move(drop));
            JS_FreeValue(ctx, entry);
        }
    }
    JS_FreeValue(ctx, arr);
    return drops;
}

std::unordered_map<std::string, BlockProperty> parseBlockProperties(JSContext* ctx, JSValueConst obj) {
    using namespace voxel::js;
    std::unordered_map<std::string, BlockProperty> props;
    JSValue propsVal = JS_GetPropertyStr(ctx, obj, "properties");
    if (!JS_IsUndefined(propsVal) && !JS_IsNull(propsVal)) {
        jsForEachProp(ctx, propsVal, [&](const char* key, JSValue val) {
            if (JS_IsBool(val)) {
                props[key] = JS_ToBool(ctx, val) > 0;
            } else if (JS_IsString(val)) {
                const char* s = JS_ToCString(ctx, val);
                props[key] = s ? std::string(s) : std::string();
                JS_FreeCString(ctx, s);
            } else {
                double d = 0.0;
                JS_ToFloat64(ctx, &d, val);
                if (d == static_cast<double>(static_cast<int>(d)))
                    props[key] = static_cast<int>(d);
                else
                    props[key] = static_cast<float>(d);
            }
        });
    }
    JS_FreeValue(ctx, propsVal);
    return props;
}

BiomeClimateRange parseClimateRange(JSContext* ctx, JSValueConst obj, const char* key) {
    using namespace voxel::js;
    JSValue v = JS_GetPropertyStr(ctx, obj, key);
    BiomeClimateRange r;
    if (!JS_IsUndefined(v) && !JS_IsNull(v)) {
        r.min = jsFloat(ctx, v, "min", 0.0f);
        r.max = jsFloat(ctx, v, "max", 1.0f);
    }
    JS_FreeValue(ctx, v);
    return r;
}

std::unordered_map<std::string, std::array<float, 3>> parseBiomeColors(JSContext* ctx, JSValueConst obj) {
    using namespace voxel::js;
    std::unordered_map<std::string, std::array<float, 3>> colors;
    JSValue colorsVal = JS_GetPropertyStr(ctx, obj, "colors");
    if (!JS_IsUndefined(colorsVal) && !JS_IsNull(colorsVal)) {
        jsForEachProp(ctx, colorsVal, [&](const char* key, JSValue arr) {
            std::array<float, 3> c = {1.0f, 1.0f, 1.0f};
            for (uint32_t j = 0; j < 3; ++j) {
                JSValue e = JS_GetPropertyUint32(ctx, arr, j);
                double d = 0.0;
                JS_ToFloat64(ctx, &d, e);
                c[j] = static_cast<float>(d);
                JS_FreeValue(ctx, e);
            }
            colors[key] = c;
        });
    }
    JS_FreeValue(ctx, colorsVal);
    return colors;
}

} // namespace voxel
