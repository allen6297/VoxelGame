## Notes
Feel free to divide and separate screens into separate files, as needed.
I'm not a fan of having everything in one file.
I'm also not a fan of having everything in one class.
feel free to add directories to organize code.
I'm not a fan of having everything in one header.

---

## Biomes

### Maps

#### Main maps
Each is a separate noise layer sampled at (X, Z), normalized to [0, 1].

- **Temperature** — continental scale (slow variation)
- **Humidity** — regional scale (medium variation)
- **Rainfall** — local scale (faster variation)
- **Elevation** — also drives terrain height directly
- **Drainage** — how quickly water leaves the surface
- **Water table** — how deep underground water sits

#### Derived maps
Computed from the main maps and terrain shape rather than stored as separate primary noise layers.

- **Slope / roughness** — derived from neighboring elevation samples
  - Distinguishes flat plateau, rolling hills, cliffs, and sharp mountains
  - Useful for biome filtering, terrain materials, and feature placement
  - Prevents high-elevation grasslands from looking identical to steep alpine terrain

- **Continentalness** — optional large-scale landmass field
  - Helps separate coast, inland plains, interior basins, and remote mountain chains
  - Keeps elevation focused on local terrain height instead of also deciding how close land is to oceans
  - Useful later for shoreline, beach, and ocean biome placement

#### Sub maps
Independent noise layers that influence geology and farming. Not used for biome selection directly, but biomes declare a preferred range for each.

- **Stone type**
  - Only affects underground fill block (granite, limestone, basalt, sandstone, etc.)
  - Biome declares a preferred stone type range
  - Drives ore generation — certain ores only appear in certain stone types
  - Changes cave shapes and underground aesthetics

- **Mineral richness** — `poor / normal / rich`
  - Separate frequency map (how often veins appear) and size map (how large each vein is)
  - Can be independent: sparse + huge veins vs dense + tiny ones
  - Ore generation = `stone_type × mineral_richness × depth`

- **Fertility** — N, P, K, Mg, Ca, S tracked individually
  - Biome declares preferred baseline values
  - Affects plant/crop growth speed and yield
  - Each nutrient correlates naturally with another map (see below)

---

### Drainage + Water table interactions

| | Low Drainage | High Drainage |
|---|---|---|
| **Shallow water table** | Swamp / bog / marsh | Dry surface, flooded caves |
| **Deep water table** | Damp soil, no surface water | True desert / arid |

- Low drainage + high rainfall = permanent surface flooding, wetlands
- High drainage + high rainfall = rainforest with dry topsoil
- Shallow water table drives where underground rivers and aquifer blocks generate
- Deep water table = dry caves, desert aquifers far underground

---

### Fertility nutrient sources

Rather than 6 fully independent noise maps, each nutrient correlates with an existing map:

| Nutrient | Drives | Natural noise source |
|---|---|---|
| N — Nitrogen | Leaf / stem growth | Rainfall + organic matter |
| P — Phosphorus | Root / fruit growth | Mineral richness of soil |
| K — Potassium | Overall plant hardiness | Stone type weathering |
| Mg — Magnesium | Chlorophyll production | Humidity |
| Ca — Calcium | Cell structure | Limestone-heavy stone types |
| S — Sulfur | Amino acids | Tectonic / volcanic proximity |

Two-layer system per nutrient:
1. **Biome baseline** — declared in the JSON (desert = low N, rainforest = high N)
2. **Local noise offset** — small ± variation on top so fertility isn't perfectly uniform within a biome

`fertilityAt(x, z)` returns `{ N, P, K, Mg, Ca, S }`.

---

### Data-Driven Biome System

Biomes follow the same pattern as blocks and items — each biome is a JSON file in `data/biomes/`.

#### File structure
```
data/
  biomes/
    temperate_forest.json
    desert.json
    tundra.json
    ocean.json
    ...
```

#### Biome definition format
```json
{
  "id": "temperate_forest",
  "name": "Temperate Forest",
  "priority": 10,
  "rarity": 1.0,

  "climate": {
    "temperature": { "min": 0.4, "max": 0.7 },
    "humidity":    { "min": 0.5, "max": 0.8 },
    "rainfall":    { "min": 0.4, "max": 0.9 },
    "elevation":   { "min": 0.3, "max": 0.7 },
    "drainage":    { "min": 0.4, "max": 0.8 },
    "waterTable":  { "min": 0.3, "max": 0.6 },
    "slope":       { "min": 0.2, "max": 0.6 }
  },

  "geology": {
    "stoneType":      { "min": 0.3, "max": 0.6 },
    "mineralRichness": { "min": 0.4, "max": 0.7 }
  },

  "terrain": {
    "baseHeight": 48,
    "heightVariation": 12
  },

  "surface": {
    "top":         "grass",
    "middle":      "dirt",
    "middleDepth": 3,
    "base":        "stone"
  },

  "atmosphere": {
    "skyColor":   [0.58, 0.78, 0.98],
    "fogColor":   [0.75, 0.85, 0.95],
    "waterColor": [0.20, 0.45, 0.80]
  },

  "fertility": {
    "nitrogen":   0.6,
    "phosphorus": 0.4,
    "potassium":  0.5,
    "magnesium":  0.5,
    "calcium":    0.4,
    "sulfur":     0.2
  },

  "features": [
    { "type": "tree",        "density": 0.05, "variant": "oak" },
    { "type": "grass_patch", "density": 0.20 }
  ]
}
```

- `priority` breaks ties between otherwise similar biome scores
- `rarity` acts as a multiplier so rare biomes can stay rare even if their climate band overlaps common ones
- `slope` is optional at first, but becomes valuable once mountains and plateaus need different biome behavior
- `continentalness` can be added later if coastlines and inland regions need stronger separation

---

### Climate matching

At each world column (X, Z), all main maps are sampled and normalized to [0, 1].
The biome whose declared ranges best fit those values is selected.

**Selection strategy: range-aware weighted score**
For each climate field:

- Score highly when the sampled value is inside the biome's preferred range
- Apply a smooth penalty when the sampled value falls outside the range
- Weight important axes more heavily than flavor axes
- Multiply the final score by biome `rarity`
- Use biome `priority` as a tie-breaker

This works better than pure midpoint distance because it does not over-favor "average" biomes whose midpoint happens to sit near the center of climate space.

Example shape:

```cpp
score = sum(axisWeight * axisFitness(sample, min, max))
finalScore = score * rarity
```

Where `axisFitness(...)` returns:

- `1.0` inside the preferred range
- smoothly decreasing values outside the range
- `0.0` only when far enough away that the biome should not be considered

---

### Biome blending at borders

Sample the biome at 5 points per column (center + 4 cardinal offsets ~4 blocks out).
Blend terrain height and atmosphere values by distance weight.
Surface blocks (top/middle/base) use the dominant biome only — avoids checkerboard patterns.

---

### Terrain generation flow (planned)

```
1. Sample all main noise maps at (wx, wz)
2. Derive secondary values such as slope / roughness from nearby elevation samples
3. Select / blend biomes for this column
4. Compute surface height from biome terrain params + elevation noise
5. Fill surface blocks using biome surface config (top / middle / base)
6. Fill underground using stone type map
7. Scatter ores using stone_type × mineral_richness × depth
8. Run feature generators (trees, grass patches, structures, etc.)
```

---

### Progressive unlock path

| Milestone | What it enables |
|---|---|
| Biome-driven terrain height | Varied landscape |
| Biome surface blocks | Visual biome identity |
| Stone type map | Varied underground, geology |
| Static atmosphere colors | Sky / fog / water per biome |
| Mineral richness + ore gen | Meaningful mining |
| Biome blending | Smooth transitions |
| Fertility map | Farming / soil system |
| Seasonal surface changes | Winter snow, autumn leaves |
| Day/night atmosphere blending | Dynamic sky |
| Per-block fertility storage | Soil depletion / enrichment |
| Full feature/structure generation | Trees, villages, caves |
