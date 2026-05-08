/**
 * engine/scripts/00_logger.js
 *
 * Lightweight logging helpers exposed to pack scripts.
 */

const Logger = {
  info(...args) {
    __logInfo(...args)
  },

  warn(...args) {
    __logWarn(...args)
  },

  error(...args) {
    __logError(...args)
  },

  debug(...args) {
    __logInfo(...args)
  },
}
