/**
 * engine/scripts/05_runtime.js
 *
 * High-level runtime APIs for interacting with the world, players, and
 * finalized game data.
 */

globalThis.Data = {
    /**
     * Looks up finalized content by id.
     * @param {string} id
     */
    getBlock(id) {
        return __dataGetBlock(id);
    },

    getItem(id) {
        return __dataGetItem(id);
    },

    getBiome(id) {
        return __dataGetBiome(id);
    },

    getTag(id) {
        return __dataGetTag(id);
    },

    getRecipe(id) {
        return __dataGetRecipe(id);
    },

    getLocalization(locale, key) {
        return __dataGetLocalization(locale, key);
    },
};

globalThis.Timers = {
    setTimeout(callback, delayMs) {
        return __timerSetTimeout(callback, delayMs);
    },

    setInterval(callback, intervalMs) {
        return __timerSetInterval(callback, intervalMs);
    },

    clear(id) {
        return __timerClear(id);
    },
};

globalThis.Commands = {
    register(name, handler) {
        return __commandRegister(name, handler);
    },

    list() {
        return __commandList();
    },
};

globalThis.World = {
    /**
     * Gets the block state ID or namespaced ID at the given coordinates.
     * @param {number} x
     * @param {number} y
     * @param {number} z
     * @returns {string|number}
     */
    getBlock(x, y, z) {
        return __world_getBlock(Math.floor(x), Math.floor(y), Math.floor(z));
    },

    /**
     * Sets the block at the given coordinates.
     * @param {number} x
     * @param {number} y
     * @param {number} z
     * @param {string|number} block Namespaced ID or state ID.
     */
    setBlock(x, y, z, block) {
        __world_setBlock(Math.floor(x), Math.floor(y), Math.floor(z), block);
    }
};

globalThis.Player = {
    /**
     * Gets the current player's position.
     * @returns {{x: number, y: number, z: number}}
     */
    getPosition() {
        return __player_getPosition();
    },

    /**
     * Sets the current player's position.
     * @param {number} x
     * @param {number} y
     * @param {number} z
     */
    setPosition(x, y, z) {
        __player_setPosition(x, y, z);
    },

    /**
     * Gets the current player's inventory.
     * @returns {Array<{itemId: string, count: number}>}
     */
    getInventory() {
        return __player_getInventory();
    }
};
