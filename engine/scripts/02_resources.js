/**
 * engine/scripts/02_resources.js
 *
 * Pack-aware resource access helpers.
 */

const Resources = {
  exists(path) {
    return __resourceExists(path)
  },

  readText(path) {
    return __resourceReadText(path)
  },

  readJson(path) {
    const text = __resourceReadText(path)
    return text === null ? null : JSON.parse(text)
  },

  list(path) {
    return __resourceList(path)
  },
}
