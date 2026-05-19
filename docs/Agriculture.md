## Notes
Feel free to divide and separate screens into separate files, as needed.
I'm not a fan of having everything in one file.
I'm also not a fan of having everything in one class.
feel free to add directories to organize code.
I'm not a fan of having everything in one header.

---

## Agriculture

### Goals

The agriculture system should:

- Build on the existing biome and fertility maps instead of replacing them
- Reward choosing the right crop for the right land
- Let players improve bad land, not only search for perfect land
- Stay readable enough that players can reason about why a crop is thriving or failing
- Start simple enough to ship, with room for deeper soil simulation later

---

## Core model

Agriculture should be driven by three layers:

1. **Regional baseline**
   - Comes from biome, rainfall, humidity, drainage, water table, and fertility maps
   - Determines what land is naturally good for farming

2. **Local block state**
   - Tilled or untilled
   - Moisture
   - Organic matter
   - Compaction or soil quality
   - Optional later: per-block nutrient depletion

3. **Crop requirements**
   - Each crop has ideal temperature, moisture, fertility, and season ranges
   - Growth rate and yield are based on how well the current block matches those needs

This gives a clean split:

- World generation decides where good farming regions naturally exist
- Player actions decide how much they can improve a specific field
- Crops decide whether that field is actually suitable

---

## Regional farmability

Use the world maps to produce a derived `farmabilityAt(x, z)` value and a more detailed `soilContextAt(x, z)`.

### Inputs from world generation

- **Rainfall**
  - Controls how often land gets passive moisture
  - Supports rain-fed farming vs irrigation-dependent farming

- **Drainage**
  - Controls whether water lingers or escapes too quickly
  - Low drainage favors rice, reeds, boggy crops, and root rot risk for dry-soil crops

- **Water table**
  - Controls natural subsoil moisture
  - Shallow water table helps wells, wet fields, and moisture retention
  - Too shallow can cause flooding or crop stress in some biomes

- **Humidity**
  - Affects evaporation rate and fungal risk

- **Temperature**
  - Affects which crops can grow and how long the growing season lasts

- **Fertility**
  - Acts as the natural nutrient baseline for the area

### Derived regional values

`soilContextAt(x, z)` should return something like:

```cpp
struct SoilContext {
    float temperature;
    float rainfall;
    float humidity;
    float drainage;
    float waterTable;

    float nitrogen;
    float phosphorus;
    float potassium;
    float magnesium;
    float calcium;
    float sulfur;

    float moistureRetention;
    float floodRisk;
    float droughtRisk;
    float farmability;
};
```

Recommended derived formulas:

- `moistureRetention = rainfall + waterTable - drainage`
- `floodRisk = rainfall + waterTable + lowDrainageBias`
- `droughtRisk = highDrainageBias + lowRainfallBias + deepWaterTableBias`
- `farmability = weighted score from moisture + fertility + temperature stability`

Do not expose all of this to players directly at first. Most of it should remain simulation-side until UI exists.

---

## Soil model

### Phase 1: simple soil system

Start with a small set of per-block soil values:

- `moisture`
- `organicMatter`
- `tilled`
- `soilQuality`

This is enough to support:

- watering and drying
- compost / manure gameplay
- good vs poor farmland
- crop growth modifiers

### Phase 2: richer soil simulation

Later, add:

- nutrient depletion per block
- salinity
- pH
- compaction
- disease / contamination

That should be an upgrade path, not the starting requirement.

### Soil block types

Keep the surface material meaningful:

- topsoil
- sandy soil
- clay-heavy soil
- peat / bog soil
- rocky soil
- silt / river soil

These do not need to be separate blocks immediately. They can begin as hidden tags or derived properties from biome + stone + drainage.

Each soil type can modify:

- moisture retention
- till speed
- fertility cap
- root depth support
- erosion risk

---

## Moisture system

Moisture should be one of the main gameplay loops because it connects weather, terrain, and farming directly.

### Moisture sources

- rainfall events
- nearby water blocks
- irrigation channels
- shallow water table
- player watering

### Moisture loss

- evaporation from temperature + humidity
- high drainage
- crop uptake
- exposed dry seasons

### Design rule

Do not make hydration binary if you can avoid it.

Prefer:

- `0.0 - 1.0` soil moisture value
- crop-specific preferred moisture ranges
- soft penalties outside the range

That will support:

- dryland crops
- flood-tolerant crops
- irrigated farming
- climate-driven regional differences

---

## Fertility model

The biome plan already defines:

- N, P, K, Mg, Ca, S as regional baseline values
- small local offsets from noise

For agriculture, use that as the natural soil baseline and layer player management on top.

### Recommended implementation path

#### Phase 1

Collapse the six nutrients into three derived crop-facing channels:

- **Green growth** = mostly N + Mg
- **Root / fruit growth** = mostly P + Ca
- **Hardiness** = mostly K + S

This keeps the simulation tied to the biome system without forcing the player to manage six separate bars immediately.

#### Phase 2

Expose full nutrient simulation if farming becomes a major progression pillar:

- different fertilizers restore different nutrients
- crop rotation preserves or depletes different nutrients
- legumes can restore nitrogen
- gypsum / lime can adjust calcium-related soil quality

### Fertility behavior

- Biome provides baseline fertility
- Soil type modifies retention and effectiveness
- Crops consume fertility slowly over time
- Compost / manure / mulch restores organic quality and some nutrients
- Rotation and fallow periods recover soil naturally

---

## Crops

Each crop should be data-driven like biomes.

### File structure

```text
data/
  crops/
    wheat.json
    potato.json
    cabbage.json
    rice.json
    flax.json
```

### Crop definition shape

```json
{
  "id": "wheat",
  "name": "Wheat",

  "climate": {
    "temperature": { "min": 0.35, "max": 0.70 },
    "humidity":    { "min": 0.25, "max": 0.70 }
  },

  "soil": {
    "moisture":      { "min": 0.35, "max": 0.65 },
    "drainage":      { "min": 0.40, "max": 0.80 },
    "organicMatter": { "min": 0.20, "max": 0.70 }
  },

  "fertility": {
    "greenGrowth": 0.50,
    "rootFruit":   0.40,
    "hardiness":   0.50
  },

  "growth": {
    "stages": 6,
    "baseDays": 8,
    "regrows": false
  },

  "yield": {
    "base": 2,
    "bonusAtIdeal": 2
  }
}
```

### Crop categories to support early

- grain crops
- root crops
- leafy crops
- wetland crops
- orchard / perennial crops later

This gives enough variety to make biome and moisture differences matter.

---

## Crop growth logic

Each planted crop should evaluate a small set of conditions:

- temperature
- season
- soil moisture
- fertility
- light
- flooding stress
- drought stress

### Growth outcome

Instead of simple alive/dead only, use a quality model:

- **Healthy** — normal speed, full yield
- **Stressed** — slower growth, reduced yield
- **Failing** — may wither or never mature
- **Dead** — destroyed by extreme mismatch, frost, flood, or neglect

### Scoring model

Use a range-aware score like the biome system:

```cpp
growthScore =
    temperatureFitness *
    moistureFitness *
    fertilityFitness *
    seasonFitness *
    lightFitness;
```

Then map that score to:

- growth speed
- disease risk
- final yield
- seed quality or byproducts later

This keeps the system consistent with your biome selection philosophy.

---

## Seasons and weather

Agriculture becomes much better if it reacts to time, not just static biome values.

### Seasonal effects

- temperature shifts by season
- rainfall frequency changes by season
- frost dates matter for sensitive crops
- dormant seasons pause some crop growth

### Weather effects

- rain passively waters fields
- storms can flood low-drainage land
- heat waves increase evaporation
- drought periods punish poor irrigation planning

### Implementation advice

Do not begin with a fully simulated climate calendar.

Start with:

- seasonal temperature modifier
- seasonal rainfall modifier
- frost threshold

That gives most of the value without a large simulation burden.

---

## Player actions

Agriculture should be improved through clear player tools.

### Early actions

- till soil
- plant seeds
- water crops
- harvest crops
- place compost / manure

### Mid-game actions

- irrigation channels
- wells
- crop rotation
- mulch
- drainage ditches
- greenhouses or cold frames

### Late-game actions

- selective fertilizers
- soil testing
- automated irrigation
- climate-controlled farming
- high-yield specialty crops

This progression matters because it turns farming from "plant on dirt" into land management.

---

## Failure modes

Bad farming land should fail for understandable reasons.

Use a short list of visible failure causes:

- too dry
- too wet
- too cold
- poor soil
- exhausted soil
- wrong season

Avoid hiding crop failure behind too many invisible stats early on.

---

## Data ownership

Keep the system split cleanly:

- `Biome` defines natural baseline climate and fertility
- `Soil` defines current block condition
- `Crop` defines requirements and outputs
- `Weather / Season` defines temporary modifiers

That separation will make the codebase easier to keep out of giant god classes.

---

## Recommended implementation path

### Milestone 1

- crop JSON loading
- simple tilled soil block state
- moisture value per farmland block
- 3-channel derived fertility model
- basic crop growth stages

Enables:

- planting, watering, harvesting
- visible difference between good land and bad land

### Milestone 2

- rain-driven moisture refill
- drainage and water table influence
- yield scaling from crop stress
- compost / manure item effects

Enables:

- farming that feels connected to biome and terrain

### Milestone 3

- seasonal modifiers
- crop rotation
- moisture spread / irrigation channels
- regional crop suitability differences

Enables:

- planning fields by climate and season

### Milestone 4

- per-block nutrient depletion
- disease / rot risk
- advanced fertilizers
- greenhouse or protected farming

Enables:

- deep agriculture as a long-term progression system

---

## Design guardrails

- Start with soft modifiers, not instant crop death for every mismatch
- Prefer a few meaningful variables over too many hidden stats
- Keep crop suitability legible enough that players can learn by observation
- Let biomes create natural farming identities
- Let player improvement overcome mediocre land, but not erase all regional differences

---

## Open questions

- Should wild plants use the same crop system or a lighter variant?
- Should farmland be a special block, or should any soil block be tillable?
- How visible should fertility be to the player at the start?
- Do you want crop quality grades, or only quantity?
- How punishing should bad seasons be?
- Is animal manure / compost part of the initial farming loop or later?
