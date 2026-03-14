#include "common/simple_json.h"

#include <cctype>
#include <cstdlib>
#include <sstream>
#include <utility>

namespace ship_sim::json {

Value::Value(std::nullptr_t value) : storage_(value) {}
Value::Value(bool value) : storage_(value) {}
Value::Value(double value) : storage_(value) {}
Value::Value(std::string value) : storage_(std::move(value)) {}
Value::Value(Object value) : storage_(std::move(value)) {}
Value::Value(Array value) : storage_(std::move(value)) {}

bool Value::isNull() const { return std::holds_alternative<std::nullptr_t>(storage_); }
bool Value::isBool() const { return std::holds_alternative<bool>(storage_); }
bool Value::isNumber() const { return std::holds_alternative<double>(storage_); }
bool Value::isString() const { return std::holds_alternative<std::string>(storage_); }
bool Value::isObject() const { return std::holds_alternative<Object>(storage_); }
bool Value::isArray() const { return std::holds_alternative<Array>(storage_); }

bool Value::asBool() const {
    if (!isBool()) {
        throw std::runtime_error("JSON value is not a bool");
    }
    return std::get<bool>(storage_);
}

double Value::asNumber() const {
    if (!isNumber()) {
        throw std::runtime_error("JSON value is not a number");
    }
    return std::get<double>(storage_);
}

const std::string& Value::asString() const {
    if (!isString()) {
        throw std::runtime_error("JSON value is not a string");
    }
    return std::get<std::string>(storage_);
}

const Value::Object& Value::asObject() const {
    if (!isObject()) {
        throw std::runtime_error("JSON value is not an object");
    }
    return std::get<Object>(storage_);
}

const Value::Array& Value::asArray() const {
    if (!isArray()) {
        throw std::runtime_error("JSON value is not an array");
    }
    return std::get<Array>(storage_);
}

const Value& Value::requireObjectMember(const std::string& key) const {
    const auto& object = asObject();
    const auto it = object.find(key);
    if (it == object.end()) {
        throw std::runtime_error("Missing JSON member: " + key);
    }
    return it->second;
}

double Value::requireNumberMember(const std::string& key) const {
    return requireObjectMember(key).asNumber();
}

const std::string& Value::requireStringMember(const std::string& key) const {
    return requireObjectMember(key).asString();
}

namespace {

class Parser {
public:
    explicit Parser(const std::string& text) : text_(text) {}

    Value parseDocument() {
        skipWhitespace();
        Value value = parseValue();
        skipWhitespace();
        if (!isAtEnd()) {
            throw error("Unexpected trailing content");
        }
        return value;
    }

private:
    const std::string& text_;
    std::size_t pos_ {0};

    Value parseValue() {
        if (isAtEnd()) {
            throw error("Unexpected end of input");
        }

        const char ch = text_[pos_];
        if (ch == '{') {
            return parseObject();
        }
        if (ch == '[') {
            return parseArray();
        }
        if (ch == '"') {
            return Value(parseString());
        }
        if (ch == 't') {
            consumeKeyword("true");
            return Value(true);
        }
        if (ch == 'f') {
            consumeKeyword("false");
            return Value(false);
        }
        if (ch == 'n') {
            consumeKeyword("null");
            return Value(nullptr);
        }
        if (ch == '-' || std::isdigit(static_cast<unsigned char>(ch))) {
            return Value(parseNumber());
        }

        throw error("Unexpected character while parsing JSON value");
    }

    Value parseObject() {
        expect('{');
        skipWhitespace();

        Value::Object object;
        if (peek('}')) {
            expect('}');
            return Value(std::move(object));
        }

        while (true) {
            skipWhitespace();
            if (!peek('"')) {
                throw error("Expected string key in object");
            }

            std::string key = parseString();
            skipWhitespace();
            expect(':');
            skipWhitespace();
            object.emplace(std::move(key), parseValue());
            skipWhitespace();

            if (peek('}')) {
                expect('}');
                break;
            }
            expect(',');
            skipWhitespace();
        }

        return Value(std::move(object));
    }

    Value parseArray() {
        expect('[');
        skipWhitespace();

        Value::Array array;
        if (peek(']')) {
            expect(']');
            return Value(std::move(array));
        }

        while (true) {
            skipWhitespace();
            array.push_back(parseValue());
            skipWhitespace();
            if (peek(']')) {
                expect(']');
                break;
            }
            expect(',');
            skipWhitespace();
        }

        return Value(std::move(array));
    }

    std::string parseString() {
        expect('"');
        std::string result;

        while (!isAtEnd()) {
            const char ch = text_[pos_++];
            if (ch == '"') {
                return result;
            }
            if (ch == '\\') {
                if (isAtEnd()) {
                    throw error("Unexpected end of input inside string escape");
                }
                const char escaped = text_[pos_++];
                switch (escaped) {
                    case '"':
                    case '\\':
                    case '/':
                        result.push_back(escaped);
                        break;
                    case 'b':
                        result.push_back('\b');
                        break;
                    case 'f':
                        result.push_back('\f');
                        break;
                    case 'n':
                        result.push_back('\n');
                        break;
                    case 'r':
                        result.push_back('\r');
                        break;
                    case 't':
                        result.push_back('\t');
                        break;
                    default:
                        throw error("Unsupported string escape sequence");
                }
                continue;
            }
            result.push_back(ch);
        }

        throw error("Unterminated string literal");
    }

    double parseNumber() {
        const std::size_t start = pos_;

        if (peek('-')) {
            ++pos_;
        }
        parseDigits();
        if (peek('.')) {
            ++pos_;
            parseDigits();
        }
        if (peek('e') || peek('E')) {
            ++pos_;
            if (peek('+') || peek('-')) {
                ++pos_;
            }
            parseDigits();
        }

        const std::string number_text = text_.substr(start, pos_ - start);
        char* end_ptr = nullptr;
        const double value = std::strtod(number_text.c_str(), &end_ptr);
        if (end_ptr == nullptr || *end_ptr != '\0') {
            throw error("Invalid number literal");
        }
        return value;
    }

    void parseDigits() {
        if (isAtEnd() || !std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
            throw error("Expected digit");
        }
        while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
            ++pos_;
        }
    }

    void consumeKeyword(const char* keyword) {
        while (*keyword != '\0') {
            if (isAtEnd() || text_[pos_] != *keyword) {
                throw error("Invalid JSON keyword");
            }
            ++pos_;
            ++keyword;
        }
    }

    void expect(char ch) {
        if (isAtEnd() || text_[pos_] != ch) {
            std::ostringstream oss;
            oss << "Expected '" << ch << "'";
            throw error(oss.str());
        }
        ++pos_;
    }

    bool peek(char ch) const {
        return !isAtEnd() && text_[pos_] == ch;
    }

    void skipWhitespace() {
        while (!isAtEnd() && std::isspace(static_cast<unsigned char>(text_[pos_]))) {
            ++pos_;
        }
    }

    bool isAtEnd() const {
        return pos_ >= text_.size();
    }

    std::runtime_error error(const std::string& message) const {
        std::ostringstream oss;
        oss << message << " at position " << pos_;
        return std::runtime_error(oss.str());
    }
};

}  // namespace

Value parse(const std::string& text) {
    Parser parser(text);
    return parser.parseDocument();
}

}  // namespace ship_sim::json
