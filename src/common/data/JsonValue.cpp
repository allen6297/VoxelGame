#include "data/JsonValue.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <vector>

namespace voxel {
bool JsonValue::isObject() const {
    return std::holds_alternative<Object>(value);
}

bool JsonValue::isArray() const {
    return std::holds_alternative<Array>(value);
}

bool JsonValue::isString() const {
    return std::holds_alternative<std::string>(value);
}

bool JsonValue::isNumber() const {
    return std::holds_alternative<double>(value);
}

bool JsonValue::isBool() const {
    return std::holds_alternative<bool>(value);
}

const JsonValue::Object& JsonValue::asObject() const {
    return std::get<Object>(value);
}

const JsonValue::Array& JsonValue::asArray() const {
    return std::get<Array>(value);
}

const std::string& JsonValue::asString() const {
    return std::get<std::string>(value);
}

double JsonValue::asNumber() const {
    return std::get<double>(value);
}

bool JsonValue::asBool() const {
    return std::get<bool>(value);
}

namespace {
class JsonParser {
public:
    explicit JsonParser(const std::string& text) : text_(text) {}

    JsonValue parse() {
        skipWhitespace();
        JsonValue value = parseValue();
        skipWhitespace();
        if (position_ != text_.size()) {
            throw std::runtime_error("Unexpected trailing JSON content");
        }
        return value;
    }

private:
    JsonValue parseValue() {
        skipWhitespace();
        if (position_ >= text_.size()) {
            throw std::runtime_error("Unexpected end of JSON input");
        }

        const char c = text_[position_];
        if (c == '{') {
            return parseObject();
        }
        if (c == '[') {
            return parseArray();
        }
        if (c == '"') {
            return JsonValue {parseString()};
        }
        if (c == 't') {
            consumeKeyword("true");
            return JsonValue {true};
        }
        if (c == 'f') {
            consumeKeyword("false");
            return JsonValue {false};
        }
        if (c == 'n') {
            consumeKeyword("null");
            return JsonValue {nullptr};
        }
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
            return JsonValue {parseNumber()};
        }

        throw std::runtime_error("Unsupported JSON token");
    }

    JsonValue parseObject() {
        JsonValue::Object object;
        consume('{');
        skipWhitespace();

        if (peek('}')) {
            consume('}');
            return JsonValue {object};
        }

        while (true) {
            skipWhitespace();
            const std::string key = parseString();
            skipWhitespace();
            consume(':');
            object.emplace(key, parseValue());
            skipWhitespace();

            if (peek('}')) {
                consume('}');
                break;
            }

            consume(',');
        }

        return JsonValue {object};
    }

    JsonValue parseArray() {
        JsonValue::Array array;
        consume('[');
        skipWhitespace();

        if (peek(']')) {
            consume(']');
            return JsonValue {array};
        }

        while (true) {
            array.push_back(parseValue());
            skipWhitespace();

            if (peek(']')) {
                consume(']');
                break;
            }

            consume(',');
        }

        return JsonValue {array};
    }

    std::string parseString() {
        consume('"');
        std::string result;

        while (position_ < text_.size()) {
            const char c = text_[position_++];
            if (c == '"') {
                return result;
            }
            if (c == '\\') {
                if (position_ >= text_.size()) {
                    throw std::runtime_error("Invalid JSON escape");
                }
                const char escaped = text_[position_++];
                switch (escaped) {
                    case '"': result.push_back('"'); break;
                    case '\\': result.push_back('\\'); break;
                    case '/': result.push_back('/'); break;
                    case 'b': result.push_back('\b'); break;
                    case 'f': result.push_back('\f'); break;
                    case 'n': result.push_back('\n'); break;
                    case 'r': result.push_back('\r'); break;
                    case 't': result.push_back('\t'); break;
                    default: throw std::runtime_error("Unsupported JSON escape sequence");
                }
                continue;
            }
            result.push_back(c);
        }

        throw std::runtime_error("Unterminated JSON string");
    }

    double parseNumber() {
        const std::size_t start = position_;
        if (text_[position_] == '-') {
            ++position_;
        }
        while (position_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[position_]))) {
            ++position_;
        }
        if (position_ < text_.size() && text_[position_] == '.') {
            ++position_;
            while (position_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[position_]))) {
                ++position_;
            }
        }

        return std::stod(text_.substr(start, position_ - start));
    }

    void consume(const char expected) {
        skipWhitespace();
        if (position_ >= text_.size() || text_[position_] != expected) {
            throw std::runtime_error("Unexpected JSON character");
        }
        ++position_;
    }

    void consumeKeyword(const std::string& keyword) {
        if (text_.compare(position_, keyword.size(), keyword) != 0) {
            throw std::runtime_error("Unexpected JSON keyword");
        }
        position_ += keyword.size();
    }

    bool peek(const char expected) const {
        return position_ < text_.size() && text_[position_] == expected;
    }

    void skipWhitespace() {
        while (position_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[position_]))) {
            ++position_;
        }
    }

    const std::string& text_;
    std::size_t position_ = 0;
};
}  // namespace

JsonValue parseJson(const std::string& text) {
    return JsonParser(text).parse();
}

namespace {
std::string escapeJsonString(const std::string& input) {
    std::string result;
    result.reserve(input.size() + 8);
    for (const char c : input) {
        switch (c) {
            case '\\': result += "\\\\"; break;
            case '"':  result += "\\\""; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:   result.push_back(c); break;
        }
    }
    return result;
}

std::string indentString(const int indent) {
    return std::string(static_cast<std::size_t>(indent), ' ');
}
}  // namespace

std::string serializeJson(const JsonValue& value, const int indent) {
    if (std::holds_alternative<std::nullptr_t>(value.value)) {
        return "null";
    }
    if (std::holds_alternative<bool>(value.value)) {
        return std::get<bool>(value.value) ? "true" : "false";
    }
    if (std::holds_alternative<double>(value.value)) {
        std::string text = std::to_string(std::get<double>(value.value));
        text.erase(text.find_last_not_of('0') + 1);
        if (!text.empty() && text.back() == '.') {
            text.push_back('0');
        }
        return text;
    }
    if (std::holds_alternative<std::string>(value.value)) {
        return "\"" + escapeJsonString(std::get<std::string>(value.value)) + "\"";
    }
    if (std::holds_alternative<JsonValue::Array>(value.value)) {
        const auto& array = std::get<JsonValue::Array>(value.value);
        if (array.empty()) {
            return "[]";
        }
        std::string result = "[\n";
        for (std::size_t i = 0; i < array.size(); ++i) {
            result += indentString(indent + 2) + serializeJson(array[i], indent + 2);
            if (i + 1 != array.size()) {
                result += ",";
            }
            result += "\n";
        }
        result += indentString(indent) + "]";
        return result;
    }

    const auto& object = std::get<JsonValue::Object>(value.value);
    if (object.empty()) {
        return "{}";
    }

    std::vector<std::string> keys;
    keys.reserve(object.size());
    for (const auto& [key, _] : object) {
        keys.push_back(key);
    }
    std::sort(keys.begin(), keys.end());

    std::string result = "{\n";
    for (std::size_t i = 0; i < keys.size(); ++i) {
        const auto& key = keys[i];
        result += indentString(indent + 2) + "\"" + escapeJsonString(key) + "\": " +
                  serializeJson(object.at(key), indent + 2);
        if (i + 1 != keys.size()) {
            result += ",";
        }
        result += "\n";
    }
    result += indentString(indent) + "}";
    return result;
}
}  // namespace voxel
