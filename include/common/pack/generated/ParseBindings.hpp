// GENERATED FILE — do not edit by hand.
// Regenerate:  node tools/codegen/generate.js

#pragma once
#include "data/GameData.hpp"
#include "data/BiomeDefinition.hpp"
#include <quickjs.h>

namespace voxel::generated {

BlockDefinition parseBlockDefinition(JSContext* ctx, JSValueConst obj);
ItemDefinition parseItemDefinition(JSContext* ctx, JSValueConst obj);
BiomeDefinition parseBiomeDefinition(JSContext* ctx, JSValueConst obj);
RecipeDefinition parseRecipeDefinition(JSContext* ctx, JSValueConst obj);

} // namespace voxel::generated
