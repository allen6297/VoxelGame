/**
 * engine/scripts/05_runtime.js
 *
 * High-level runtime APIs for interacting with the world and players.
 */

const World = {
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

const Player = {
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
