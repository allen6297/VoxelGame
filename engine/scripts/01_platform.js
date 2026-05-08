/**
 * engine/scripts/01_platform.js
 *
 * Platform and runtime environment helpers.
 */

const Platform = {
  isClient() {
    return __platform_isClient()
  },

  isServer() {
    return __platform_isServer()
  },

  isDevelopment() {
    return __platform_isDevelopment()
  },

  getGameVersion() {
    return __platform_getGameVersion()
  },

  isPackLoaded(id) {
    return __platform_isPackLoaded(id)
  },
}
