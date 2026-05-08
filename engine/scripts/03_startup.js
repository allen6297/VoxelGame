/**
 * engine/scripts/03_startup.js
 *
 * Tag and localization helpers plus the legacy startup API placeholder.
 */

const Localization = {
  add(locale, entries) {
    __locAdd(locale, entries)
  },

  get(locale, key) {
    return __locGet(locale, key)
  },
}

const Models = {
  exists(path) {
    return __modelExists(path)
  },

  readText(path) {
    return __modelReadText(path)
  },

  readJson(path) {
    return __modelReadJson(path)
  },

  list(path) {
    return __modelList(path)
  },
}

const Tags = {
  add(tagId, memberId) {
    __tagAdd(tagId, memberId)
  },

  remove(tagId, memberId) {
    __tagRemove(tagId, memberId)
  },

  has(tagId, memberId) {
    return __tagHas(tagId, memberId)
  },
}
