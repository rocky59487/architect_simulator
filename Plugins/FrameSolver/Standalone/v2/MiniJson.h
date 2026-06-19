// MiniJson.h -- self-contained JSON parser/writer used by the v2 dispatcher.
//
// WHY HAND-WRITTEN
//   Rhino bridge spec (S6b § ⑪) commits to a "no third-party dependencies" rule for the engine
//   side: bringing in nlohmann::json / RapidJSON / picojson would violate it. The v2 header
//   uses JSON because it is auditable and self-describing, but the parser surface is tiny —
//   we only need: parse a UTF-8 buffer into a tree, look up object members by name, iterate
//   arrays, read int/double/string/bool, and write the same shapes back. ~250 LOC fits in a
//   header without losing readability. Performance is irrelevant for the header size we deal
//   with (typical < 1 KB; even worst-case model.set is < 1 MB and we parse once per call).
//
// FOOTPRINT
//   Header-only. Standard C++17 only (string, vector, variant, unordered_map). No exceptions
//   escape the parser; parse() returns false on malformed input and writes a one-line diag.
//
// SCOPE OF THE PARSER (sufficient for v2 wire schema)
//   * objects, arrays, strings, numbers, true/false/null
//   * \uXXXX escapes in strings (BMP only; surrogates pair NOT decoded — we accept them as
//     literals and pass through, since none of v2's strings need non-BMP content)
//   * trailing whitespace OK; trailing commas REJECTED (strict JSON, matches client)
//
// DELIBERATELY NOT SUPPORTED
//   Comments, numbers with leading +, NaN/Infinity literals (engine refuses those values for
//   safety; they are also not standard JSON). Add only when v2 schema requires them.

#pragma once

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace frame_v2 {

// v2.4 release-hardening: cap the parser's nesting depth so an adversarial peer cannot
// stack-overflow the dispatcher process (typical v2 headers nest 4-5 levels; 64 is roomy).
inline constexpr int kMaxJsonDepth = 64;

class Json;
using JsonObject = std::unordered_map<std::string, Json>;
using JsonArray  = std::vector<Json>;

class Json {
public:
    enum class Type { Null, Bool, Int, Double, String, Array, Object };

    Json() = default;
    Json(bool b)                       : v_(b) {}
    Json(int i)                        : v_(static_cast<int64_t>(i)) {}
    Json(int64_t i)                    : v_(i) {}
    Json(double d)                     : v_(d) {}
    Json(const char* s)                : v_(std::string(s)) {}
    Json(std::string s)                : v_(std::move(s)) {}
    Json(JsonArray a)                  : v_(std::move(a)) {}
    Json(JsonObject o)                 : v_(std::move(o)) {}

    Type type() const {
        switch (v_.index()) {
            case 0: return Type::Null;   case 1: return Type::Bool;
            case 2: return Type::Int;    case 3: return Type::Double;
            case 4: return Type::String; case 5: return Type::Array;
            case 6: return Type::Object;
        }
        return Type::Null;
    }

    bool isNull()   const { return type() == Type::Null;   }
    bool isBool()   const { return type() == Type::Bool;   }
    bool isInt()    const { return type() == Type::Int;    }
    bool isDouble() const { return type() == Type::Double; }
    bool isNumber() const { return isInt() || isDouble();  }
    bool isString() const { return type() == Type::String; }
    bool isArray()  const { return type() == Type::Array;  }
    bool isObject() const { return type() == Type::Object; }

    bool                 asBool()   const { return std::get<bool>(v_); }
    int64_t              asInt()    const { return isInt() ? std::get<int64_t>(v_) : static_cast<int64_t>(std::get<double>(v_)); }
    double               asDouble() const { return isDouble() ? std::get<double>(v_) : static_cast<double>(std::get<int64_t>(v_)); }
    const std::string&   asString() const { return std::get<std::string>(v_); }
    const JsonArray&     asArray()  const { return std::get<JsonArray>(v_); }
    JsonArray&           asArray()        { return std::get<JsonArray>(v_); }
    const JsonObject&    asObject() const { return std::get<JsonObject>(v_); }
    JsonObject&          asObject()       { return std::get<JsonObject>(v_); }

    bool has(const std::string& k) const {
        if (!isObject()) return false;
        return std::get<JsonObject>(v_).count(k) > 0;
    }
    const Json* get(const std::string& k) const {
        if (!isObject()) return nullptr;
        const auto& o = std::get<JsonObject>(v_);
        auto it = o.find(k);
        return it == o.end() ? nullptr : &it->second;
    }
    Json& operator[](const std::string& k) {
        if (!isObject()) v_ = JsonObject{};
        return std::get<JsonObject>(v_)[k];
    }

    // Defaulted getters; useful in v2 method handlers ("optional bool, default false").
    bool   getBool  (const std::string& k, bool   d = false) const { auto p = get(k); return p && p->isBool()   ? p->asBool()   : d; }
    int64_t getInt  (const std::string& k, int64_t d = 0)    const { auto p = get(k); return p && p->isNumber() ? p->asInt()    : d; }
    double getDouble(const std::string& k, double d = 0)     const { auto p = get(k); return p && p->isNumber() ? p->asDouble() : d; }
    std::string getString(const std::string& k, std::string d = "") const {
        auto p = get(k);
        return p && p->isString() ? p->asString() : std::move(d);
    }

    std::string dump() const {
        std::string out;
        dumpInto(out);
        return out;
    }

    static bool parse(const std::string& src, Json& out, std::string& err) {
        Parser p(src);
        return p.parseValue(out, err) && p.eatTrailing(err);
    }

private:
    std::variant<std::monostate, bool, int64_t, double, std::string, JsonArray, JsonObject> v_;

    void dumpInto(std::string& out) const {
        switch (type()) {
            case Type::Null:   out += "null"; return;
            case Type::Bool:   out += asBool() ? "true" : "false"; return;
            case Type::Int:    out += std::to_string(asInt());     return;
            case Type::Double: {
                char buf[32];
                std::snprintf(buf, sizeof(buf), "%.17g", asDouble());
                out += buf;
                return;
            }
            case Type::String: dumpString(asString(), out); return;
            case Type::Array: {
                out += '[';
                bool first = true;
                for (const auto& e : asArray()) {
                    if (!first) out += ',';
                    first = false;
                    e.dumpInto(out);
                }
                out += ']';
                return;
            }
            case Type::Object: {
                out += '{';
                bool first = true;
                for (const auto& kv : asObject()) {
                    if (!first) out += ',';
                    first = false;
                    dumpString(kv.first, out);
                    out += ':';
                    kv.second.dumpInto(out);
                }
                out += '}';
                return;
            }
        }
    }

    static void dumpString(const std::string& s, std::string& out) {
        out += '"';
        for (unsigned char c : s) {
            switch (c) {
                case '"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n";  break;
                case '\r': out += "\\r";  break;
                case '\t': out += "\\t";  break;
                case '\b': out += "\\b";  break;
                case '\f': out += "\\f";  break;
                default:
                    if (c < 0x20) {
                        char buf[8];
                        std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                        out += buf;
                    } else {
                        out += static_cast<char>(c);
                    }
            }
        }
        out += '"';
    }

    struct Parser {
        const std::string& s;
        size_t i = 0;
        int    depth = 0;   // current object/array nesting; bounded by kMaxJsonDepth
        explicit Parser(const std::string& src) : s(src) {}

        void skip() { while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) ++i; }
        bool eatTrailing(std::string& err) {
            skip();
            if (i != s.size()) { err = "trailing data after JSON value"; return false; }
            return true;
        }

        bool parseValue(Json& out, std::string& err) {
            skip();
            if (i >= s.size()) { err = "unexpected EOF"; return false; }
            char c = s[i];
            if (c == '"') return parseString(out, err);
            if (c == '{') return parseObject(out, err);
            if (c == '[') return parseArray(out, err);
            if (c == 't' || c == 'f') return parseBool(out, err);
            if (c == 'n') return parseNull(out, err);
            if (c == '-' || (c >= '0' && c <= '9')) return parseNumber(out, err);
            err = "unexpected character"; return false;
        }

        bool parseObject(Json& out, std::string& err) {
            if (++depth > kMaxJsonDepth) { err = "JSON nesting too deep"; return false; }
            ++i; skip();
            JsonObject o;
            if (i < s.size() && s[i] == '}') { ++i; out = std::move(o); --depth; return true; }
            for (;;) {
                skip();
                Json key;
                if (i >= s.size() || s[i] != '"') { err = "expected string key"; return false; }
                if (!parseString(key, err)) return false;
                skip();
                if (i >= s.size() || s[i] != ':') { err = "expected ':' after key"; return false; }
                ++i;
                Json val;
                if (!parseValue(val, err)) return false;
                o.emplace(key.asString(), std::move(val));
                skip();
                if (i >= s.size()) { err = "unterminated object"; return false; }
                if (s[i] == ',') { ++i; continue; }
                if (s[i] == '}') { ++i; out = std::move(o); --depth; return true; }
                err = "expected ',' or '}'"; return false;
            }
        }

        bool parseArray(Json& out, std::string& err) {
            if (++depth > kMaxJsonDepth) { err = "JSON nesting too deep"; return false; }
            ++i; skip();
            JsonArray a;
            if (i < s.size() && s[i] == ']') { ++i; out = std::move(a); --depth; return true; }
            for (;;) {
                Json v;
                if (!parseValue(v, err)) return false;
                a.push_back(std::move(v));
                skip();
                if (i >= s.size()) { err = "unterminated array"; return false; }
                if (s[i] == ',') { ++i; continue; }
                if (s[i] == ']') { ++i; out = std::move(a); --depth; return true; }
                err = "expected ',' or ']'"; return false;
            }
        }

        bool parseString(Json& out, std::string& err) {
            ++i;   // open quote
            std::string r;
            while (i < s.size()) {
                char c = s[i++];
                if (c == '"') { out = std::move(r); return true; }
                if (c == '\\') {
                    if (i >= s.size()) { err = "bad escape"; return false; }
                    char e = s[i++];
                    switch (e) {
                        case '"':  r += '"';  break;
                        case '\\': r += '\\'; break;
                        case '/':  r += '/';  break;
                        case 'n':  r += '\n'; break;
                        case 'r':  r += '\r'; break;
                        case 't':  r += '\t'; break;
                        case 'b':  r += '\b'; break;
                        case 'f':  r += '\f'; break;
                        case 'u': {
                            if (i + 4 > s.size()) { err = "bad unicode escape"; return false; }
                            uint32_t cp = 0;
                            for (int k = 0; k < 4; ++k) {
                                char h = s[i++]; int d;
                                if (h >= '0' && h <= '9') d = h - '0';
                                else if (h >= 'a' && h <= 'f') d = 10 + h - 'a';
                                else if (h >= 'A' && h <= 'F') d = 10 + h - 'A';
                                else { err = "bad hex digit"; return false; }
                                cp = (cp << 4) | static_cast<uint32_t>(d);
                            }
                            if (cp < 0x80) r += static_cast<char>(cp);
                            else if (cp < 0x800) { r += static_cast<char>(0xC0 | (cp >> 6)); r += static_cast<char>(0x80 | (cp & 0x3F)); }
                            else { r += static_cast<char>(0xE0 | (cp >> 12)); r += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); r += static_cast<char>(0x80 | (cp & 0x3F)); }
                            break;
                        }
                        default: err = "bad escape char"; return false;
                    }
                } else {
                    r += c;
                }
            }
            err = "unterminated string"; return false;
        }

        bool parseBool(Json& out, std::string& err) {
            if (i + 4 <= s.size() && std::memcmp(&s[i], "true", 4) == 0) { i += 4; out = true; return true; }
            if (i + 5 <= s.size() && std::memcmp(&s[i], "false", 5) == 0) { i += 5; out = false; return true; }
            err = "expected 'true' or 'false'"; return false;
        }
        bool parseNull(Json& out, std::string& err) {
            if (i + 4 <= s.size() && std::memcmp(&s[i], "null", 4) == 0) { i += 4; out = Json(); return true; }
            err = "expected 'null'"; return false;
        }
        bool parseNumber(Json& out, std::string& err) {
            size_t start = i;
            auto isDigit = [&](size_t p) { return p < s.size() && s[p] >= '0' && s[p] <= '9'; };
            if (s[i] == '-') {
                ++i;
                if (!isDigit(i)) { err = "expected digit after '-'"; return false; }
            }
            if (!isDigit(i)) { err = "expected digit"; return false; }
            if (s[i] == '0') {
                ++i;
                if (isDigit(i)) { err = "leading zero in number"; return false; }
            } else {
                while (isDigit(i)) ++i;
            }
            bool isDouble = false;
            if (i < s.size() && s[i] == '.') {
                isDouble = true; ++i;
                if (!isDigit(i)) { err = "expected digit after decimal point"; return false; }
                while (isDigit(i)) ++i;
            }
            if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
                isDouble = true; ++i;
                if (i < s.size() && (s[i] == '+' || s[i] == '-')) ++i;
                if (!isDigit(i)) { err = "expected digit in exponent"; return false; }
                while (isDigit(i)) ++i;
            }
            if (i == start) { err = "expected number"; return false; }
            std::string num(s, start, i - start);
            if (isDouble) {
                // v2.4 release-hardening: catch overflow-to-Inf (e.g. "1e400" is legal JSON
                // syntax, parses with strtod to HUGE_VAL = +Inf, then silently poisons every
                // downstream double once B3 wires the engine). Reject as malformed input.
                double d = std::strtod(num.c_str(), nullptr);
                if (!std::isfinite(d)) { err = "non-finite number (overflow or denormal)"; return false; }
                out = d;
            } else {
                out = static_cast<int64_t>(std::strtoll(num.c_str(), nullptr, 10));
            }
            return true;
        }
    };
};

}  // namespace frame_v2
