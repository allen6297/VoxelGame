/**
 * packs/base/scripts/main.js
 *
 * Test script for runtime API.
 */

let timer = 0;

function tick(dt) {
    timer += dt;
    if (timer >= 5.0) {
        timer = 0;
        const pos = Player.getPosition();
        console.log(`[Script] Server tick! Player at: ${pos.x.toFixed(2)}, ${pos.y.toFixed(2)}, ${pos.z.toFixed(2)}`);
        
        // Example: check block below player
        const bx = Math.floor(pos.x);
        const by = Math.floor(pos.y) - 1;
        const bz = Math.floor(pos.z);
        const block = World.getBlock(bx, by, bz);
        console.log(`[Script] Block below player: ${block}`);
    }
}
