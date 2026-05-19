# Models & Textures

## Directory layout

```
assets/
  models/
    blocks/       Block model JSON files
  textures/
    blocks/       Block textures (.ppm)
    ui/           UI textures (.ppm)
```

---

## Textures

The following formats are supported:

| Format | Extension |
|--------|-----------|
| PNG | `.png` |
| JPEG | `.jpg` / `.jpeg` |
| PPM (ASCII P3) | `.ppm` |

Any resolution works, though powers of 2 (16×16, 32×32, etc.) are recommended for clean UV sampling.

To reference a texture in a model, use a path relative to the `assets/` root:

```
textures/blocks/stone.png
```

---

## Block models

Models are JSON files referenced from a block's data file via the `"model"` field:

```json
"render": {
    "model": "models/blocks/stone.json"
}
```

### Minimal model

```json
{
    "textures": {
        "all": "textures/blocks/stone.ppm"
    },
    "elements": [
        {
            "from": [0, 0, 0],
            "to":   [16, 16, 16],
            "faces": {
                "up":    { "uv": [0, 0, 16, 16], "texture": "#all" },
                "down":  { "uv": [0, 0, 16, 16], "texture": "#all" },
                "north": { "uv": [0, 0, 16, 16], "texture": "#all" },
                "south": { "uv": [0, 0, 16, 16], "texture": "#all" },
                "east":  { "uv": [0, 0, 16, 16], "texture": "#all" },
                "west":  { "uv": [0, 0, 16, 16], "texture": "#all" }
            }
        }
    ]
}
```

### Coordinates

- The block occupies a **16×16×16** unit cube (`from [0,0,0]` to `to [16,16,16]`).
- Coordinates outside this range are valid — elements can overhang into adjacent block space and collision will still work correctly.
- UV coordinates are also in 0–16 space, mapping to the texture's full width/height.

### Texture variables

The `"textures"` object declares named variables. Face references use `#name` to refer to them:

```json
"textures": {
    "top":  "textures/blocks/grass.ppm",
    "side": "textures/blocks/stone.ppm"
},
"faces": {
    "up":    { "texture": "#top" },
    "north": { "texture": "#side" }
}
```

Variables can also chain — `"all": "#side"` resolves through to the final path.

### Face keys

| Key | Face |
|-----|------|
| `up` | Top |
| `down` | Bottom |
| `north` | -Z |
| `south` | +Z |
| `east` | +X |
| `west` | -X |

### Element rotation

```json
"rotation": {
    "origin": [8, 8, 8],
    "axis": "y",
    "angle": 45
}
```

`axis` is `"x"`, `"y"`, or `"z"`. `angle` is in degrees.

### Multiple elements

A model can have any number of elements. Each element produces its own **collision box** automatically — no extra configuration needed:

```json
"elements": [
    { "from": [0, 0, 0], "to": [16, 16, 16], "faces": { ... } },
    { "from": [6, 16, 6], "to": [10, 32, 10], "faces": { ... } }
]
```

---

## Parent models

Models support inheritance via `"parent"`. The child inherits the parent's elements and texture variables, then overrides what it needs:

```json
{
    "parent": "models/blocks/cube_all.json",
    "textures": {
        "all": "textures/blocks/stone.ppm"
    }
}
```

Parent chains resolve depth-first (max depth: 8). A child's `"textures"` always override the parent's. A child's `"elements"` fully replace the parent's.

### Built-in parent models

| Model | Texture keys | Description |
|-------|-------------|-------------|
| `models/blocks/cube_all.json` | `all` | Full cube, same texture on all 6 faces |
| `models/blocks/cube_column.json` | `end`, `side` | Full cube with different top/bottom vs sides |

### Example — inheriting cube_column

```json
{
    "parent": "models/blocks/cube_column.json",
    "textures": {
        "end":  "textures/blocks/dirt.ppm",
        "side": "textures/blocks/grass.ppm"
    }
}
```

---

## Texture key mapping

When `populateFaceTextures` reads a resolved model, texture variable names map to faces as follows:

| Key(s) | Faces set |
|--------|-----------|
| `all` | all 6 faces |
| `top`, `up`, `end` | `up` + `down` |
| `bottom`, `down` | `down` only |
| `side` | `north`, `south`, `east`, `west` |
| `north` / `south` / `east` / `west` / `up` / `down` | that face only |

---

## Block tinting

Set `"tintKey": true` in the block's render section to apply the biome grass color as a vertex tint. The texture should be greyscale or near-white so the tint color comes through cleanly.

```json
"render": {
    "tintKey": true,
    "model": "models/blocks/grass_block.json"
}
```

---

## Blockbench compatibility

Models exported from Blockbench work directly. Fields like `"format_version"`, `"credit"`, and `"color"` are ignored by the parser. Keep element coordinates in 0–32 range for predictable results. Textures can be PNG, JPEG, or PPM — export from Blockbench as PNG and reference the `.png` path in the model.
