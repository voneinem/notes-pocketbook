#include "pbnotes/json.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace pbnotes {
namespace json {

const Value* Value::find(const std::string& key) const {
    if (type != Type::Object) return nullptr;
    for (const auto& kv : object)
        if (kv.first == key) return &kv.second;
    return nullptr;
}

std::optional<std::string> Value::get_string(const std::string& key) const {
    const Value* v = find(key);
    if (!v || !v->is_string()) return std::nullopt;
    return v->string;
}

std::optional<int64_t> Value::get_int(const std::string& key) const {
    const Value* v = find(key);
    if (!v || !v->is_number() || !v->is_integer) return std::nullopt;
    return static_cast<int64_t>(v->number);
}

std::optional<bool> Value::get_bool(const std::string& key) const {
    const Value* v = find(key);
    if (!v || !v->is_bool()) return std::nullopt;
    return v->boolean;
}

namespace {

class Parser {
public:
    explicit Parser(const std::string& text) : s_(text) {}

    bool parse_document(Value* out, std::string* error) {
        skip_ws();
        if (!parse_value(out, 0)) {
            if (error) *error = error_.empty() ? "syntax error" : error_;
            return false;
        }
        skip_ws();
        if (pos_ != s_.size()) {
            if (error) *error = "trailing characters";
            return false;
        }
        return true;
    }

private:
    static constexpr int kMaxDepth = 64;

    void skip_ws() {
        while (pos_ < s_.size() && (s_[pos_] == ' ' || s_[pos_] == '\t' || s_[pos_] == '\n' || s_[pos_] == '\r')) ++pos_;
    }
    bool fail(const char* msg) {
        if (error_.empty()) error_ = msg;
        return false;
    }
    bool literal(const char* word) {
        const size_t n = std::strlen(word);
        if (s_.compare(pos_, n, word) != 0) return fail("bad literal");
        pos_ += n;
        return true;
    }

    bool parse_value(Value* out, int depth) {
        if (depth > kMaxDepth) return fail("nesting too deep");
        if (pos_ >= s_.size()) return fail("unexpected end");
        const char c = s_[pos_];
        if (c == '{') return parse_object(out, depth);
        if (c == '[') return parse_array(out, depth);
        if (c == '"') {
            out->type = Value::Type::String;
            return parse_string(&out->string);
        }
        if (c == 't') { out->type = Value::Type::Bool; out->boolean = true; return literal("true"); }
        if (c == 'f') { out->type = Value::Type::Bool; out->boolean = false; return literal("false"); }
        if (c == 'n') { out->type = Value::Type::Null; return literal("null"); }
        return parse_number(out);
    }

    bool parse_number(Value* out) {
        const size_t start = pos_;
        if (pos_ < s_.size() && s_[pos_] == '-') ++pos_;
        if (pos_ >= s_.size() || !std::isdigit(static_cast<unsigned char>(s_[pos_]))) return fail("bad number");
        if (s_[pos_] == '0') ++pos_;
        else while (pos_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[pos_]))) ++pos_;
        bool integer = true;
        if (pos_ < s_.size() && s_[pos_] == '.') {
            integer = false;
            ++pos_;
            if (pos_ >= s_.size() || !std::isdigit(static_cast<unsigned char>(s_[pos_]))) return fail("bad number");
            while (pos_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[pos_]))) ++pos_;
        }
        if (pos_ < s_.size() && (s_[pos_] == 'e' || s_[pos_] == 'E')) {
            integer = false;
            ++pos_;
            if (pos_ < s_.size() && (s_[pos_] == '+' || s_[pos_] == '-')) ++pos_;
            if (pos_ >= s_.size() || !std::isdigit(static_cast<unsigned char>(s_[pos_]))) return fail("bad number");
            while (pos_ < s_.size() && std::isdigit(static_cast<unsigned char>(s_[pos_]))) ++pos_;
        }
        out->type = Value::Type::Number;
        out->number = std::strtod(s_.substr(start, pos_ - start).c_str(), nullptr);
        out->is_integer = integer && std::fabs(out->number) < 9.0e15;
        return true;
    }

    static void append_utf8(std::string* out, unsigned cp) {
        if (cp < 0x80) out->push_back(static_cast<char>(cp));
        else if (cp < 0x800) {
            out->push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out->push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            out->push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out->push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out->push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out->push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out->push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out->push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out->push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }

    bool hex4(unsigned* v) {
        if (pos_ + 4 > s_.size()) return fail("bad \\u escape");
        unsigned r = 0;
        for (int i = 0; i < 4; ++i) {
            const char h = s_[pos_ + i];
            r <<= 4;
            if (h >= '0' && h <= '9') r |= h - '0';
            else if (h >= 'a' && h <= 'f') r |= 10 + h - 'a';
            else if (h >= 'A' && h <= 'F') r |= 10 + h - 'A';
            else return fail("bad \\u escape");
        }
        pos_ += 4;
        *v = r;
        return true;
    }

    bool parse_string(std::string* out) {
        ++pos_;  // opening quote
        out->clear();
        while (pos_ < s_.size()) {
            const char c = s_[pos_++];
            if (c == '"') return true;
            if (static_cast<unsigned char>(c) < 0x20) return fail("control character in string");
            if (c != '\\') { out->push_back(c); continue; }
            if (pos_ >= s_.size()) break;
            const char e = s_[pos_++];
            switch (e) {
                case '"': out->push_back('"'); break;
                case '\\': out->push_back('\\'); break;
                case '/': out->push_back('/'); break;
                case 'b': out->push_back('\b'); break;
                case 'f': out->push_back('\f'); break;
                case 'n': out->push_back('\n'); break;
                case 'r': out->push_back('\r'); break;
                case 't': out->push_back('\t'); break;
                case 'u': {
                    unsigned cp;
                    if (!hex4(&cp)) return false;
                    if (cp >= 0xD800 && cp < 0xDC00) {  // high surrogate: needs a low one
                        if (pos_ + 1 < s_.size() && s_[pos_] == '\\' && s_[pos_ + 1] == 'u') {
                            pos_ += 2;
                            unsigned lo;
                            if (!hex4(&lo)) return false;
                            if (lo < 0xDC00 || lo > 0xDFFF) return fail("bad surrogate pair");
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                        } else {
                            return fail("lone surrogate");
                        }
                    } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                        return fail("lone surrogate");
                    }
                    append_utf8(out, cp);
                    break;
                }
                default: return fail("bad escape");
            }
        }
        return fail("unterminated string");
    }

    bool parse_array(Value* out, int depth) {
        out->type = Value::Type::Array;
        ++pos_;
        skip_ws();
        if (pos_ < s_.size() && s_[pos_] == ']') { ++pos_; return true; }
        while (true) {
            Value item;
            skip_ws();
            if (!parse_value(&item, depth + 1)) return false;
            out->array.push_back(std::move(item));
            skip_ws();
            if (pos_ >= s_.size()) return fail("unterminated array");
            if (s_[pos_] == ',') { ++pos_; continue; }
            if (s_[pos_] == ']') { ++pos_; return true; }
            return fail("bad array");
        }
    }

    bool parse_object(Value* out, int depth) {
        out->type = Value::Type::Object;
        ++pos_;
        skip_ws();
        if (pos_ < s_.size() && s_[pos_] == '}') { ++pos_; return true; }
        while (true) {
            skip_ws();
            if (pos_ >= s_.size() || s_[pos_] != '"') return fail("object key expected");
            std::string key;
            if (!parse_string(&key)) return false;
            skip_ws();
            if (pos_ >= s_.size() || s_[pos_] != ':') return fail("':' expected");
            ++pos_;
            skip_ws();
            Value v;
            if (!parse_value(&v, depth + 1)) return false;
            out->object.emplace_back(std::move(key), std::move(v));
            skip_ws();
            if (pos_ >= s_.size()) return fail("unterminated object");
            if (s_[pos_] == ',') { ++pos_; continue; }
            if (s_[pos_] == '}') { ++pos_; return true; }
            return fail("bad object");
        }
    }

    const std::string& s_;
    size_t pos_ = 0;
    std::string error_;
};

}  // namespace

std::optional<Value> parse(const std::string& text, std::string* error) {
    Value v;
    Parser p(text);
    if (!p.parse_document(&v, error)) return std::nullopt;
    return v;
}

std::string quote(const std::string& s) {
    std::string out = "\"";
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof buf, "\\u%04x", c);
                    out += buf;
                } else {
                    out.push_back(static_cast<char>(c));
                }
        }
    }
    out += "\"";
    return out;
}

}  // namespace json
}  // namespace pbnotes
