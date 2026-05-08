/**
 * packs/base/scripts/main.js
 *
 * Test script for runtime API.
 */

Timers.setInterval(() => {
    const pos = Player.getPosition();
    console.log(`[Script] Server tick! Player at: ${pos.x.toFixed(2)}, ${pos.y.toFixed(2)}, ${pos.z.toFixed(2)}`);

    const bx = Math.floor(pos.x);
    const by = Math.floor(pos.y) - 1;
    const bz = Math.floor(pos.z);
    const block = World.getBlock(bx, by, bz);
    console.log(`[Script] Block below player: ${block}`);

    const grass = Data.getBlock('base:grass');
    console.log(`[Script] Base grass block name: ${grass ? grass.name : 'missing'}`);
}, 5000);

Commands.register('ping', ctx => {
    return `Pong from server (${ctx.args.join(' ') || 'no args'})`;
});

Logger.info('Registered commands:', Commands.list().join(', '))
Logger.info('Grass model file exists?', Models.exists('models/blocks/grass_block.json'));
