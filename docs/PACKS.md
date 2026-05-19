I am building a single-player Minecraft-like voxel game in C++.

I want to add a JavaScript modding system inspired by KubeJS.

Please help me design and implement a small working prototype with:

1. A C++ registry system for blocks and items
2. JSON data loading for mods
3. Embedded JavaScript scripting using QuickJS
4. A safe JS API exposed from C++:
    - StartupEvents.registry("block", event => {})
    - event.create(id)
    - Registry.modifyBlock(id, callback)
5. A mod folder structure like:

packs/
'Namespace'/
pack.json
blocks/
items/
recipes/
scripts/
startup/
server/
client/

6. Example files:
    - pack.json
    - copper_ore.json
    - startup.js

7. C++ code that:
    - loads pack JSON files
    - runs startup.js
    - prints the final block registry

Important design rules:
- C++ owns all real engine data
- JavaScript can only modify data through safe APIs
- registries should be frozen after startup
- this is single-player only for now
- keep the prototype small and understandable

Please provide the code in stages and explain how each piece connects.

~Registry registry;

loadJsonBlocks(registry, "mods/example_mod/data/blocks");

JsEngine js;

js.exposeFunction("registerBlock", [&](std::string id) {
registry.createBlock(id);
});

js.exposeFunction("modifyBlock", [&](std::string id, JsFunction callback) {
BlockDef& block = registry.getBlock(id);
callback(block);
});

js.runFile("mods/example_mod/scripts/startup.js");~