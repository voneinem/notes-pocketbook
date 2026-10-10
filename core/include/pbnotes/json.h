#pragma once
// Minimal, defensive JSON reader and string quoting (no external dependency, no Qt).
// Parsing fails as a whole on any syntax error: callers never see partial results.

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace pbnotes {
namespace json {

class Value {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Type type = Type::Null;
    bool boolean = false;
    double number = 0;
    bool is_integer = false;  // the number was written without fraction/exponent
    std::string string;
    std::vector<Value> array;
    std::vector<std::pair<std::string, Value>> object;

    bool is_null() const { return type == Type::Null; }
    bool is_bool() const { return type == Type::Bool; }
    bool is_number() const { return type == Type::Number; }
    bool is_string() const { return type == Type::String; }
    bool is_array() const { return type == Type::Array; }
    bool is_object() const { return type == Type::Object; }

    // Object member lookup; nullptr if this is not an object or the key is absent.
    const Value* find(const std::string& key) const;
    // Typed accessors: nullopt if absent or of the wrong type.
    std::optional<std::string> get_string(const std::string& key) const;
    std::optional<int64_t> get_int(const std::string& key) const;
    std::optional<bool> get_bool(const std::string& key) const;
};

// Parses a complete JSON document (trailing whitespace allowed). On failure returns nullopt and
// sets *error (if given).
std::optional<Value> parse(const std::string& text, std::string* error = nullptr);

// JSON string literal with quotes and escapes (UTF-8 passes through unchanged).
std::string quote(const std::string& s);

}  // namespace json
}  // namespace pbnotes
