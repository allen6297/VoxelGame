#pragma once

#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace voxel {
struct JsonValue {
    using Object = std::unordered_map<std::string, JsonValue>;
    using Array = std::vector<JsonValue>;

    std::variant<std::nullptr_t, bool, double, std::string, Object, Array> value;

    bool isObject() const;
    bool isArray() const;
    bool isString() const;
    bool isNumber() const;
    bool isBool() const;

    const Object& asObject() const;
    const Array& asArray() const;
    const std::string& asString() const;
    double asNumber() const;
    bool asBool() const;
};

JsonValue parseJson(const std::string& text);
std::string serializeJson(const JsonValue& value, int indent = 0);
}  // namespace voxel
