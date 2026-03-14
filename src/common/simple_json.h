#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace ship_sim::json {

class Value {
public:
    using Object = std::unordered_map<std::string, Value>;
    using Array = std::vector<Value>;

    Value() = default;
    explicit Value(std::nullptr_t);
    explicit Value(bool value);
    explicit Value(double value);
    explicit Value(std::string value);
    explicit Value(Object value);
    explicit Value(Array value);

    bool isNull() const;
    bool isBool() const;
    bool isNumber() const;
    bool isString() const;
    bool isObject() const;
    bool isArray() const;

    bool asBool() const;
    double asNumber() const;
    const std::string& asString() const;
    const Object& asObject() const;
    const Array& asArray() const;

    const Value& requireObjectMember(const std::string& key) const;
    double requireNumberMember(const std::string& key) const;
    const std::string& requireStringMember(const std::string& key) const;

private:
    using Storage = std::variant<std::nullptr_t, bool, double, std::string, Object, Array>;
    Storage storage_ {nullptr};
};

Value parse(const std::string& text);

}  // namespace ship_sim::json
