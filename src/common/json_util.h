#pragma once

#include <string>
#include <vector>
#include <map>
#include <variant>
#include <stdexcept>
#include <sstream>
#include <cctype>

// Minimal JSON parser/writer — enough for our config file.
// Supports: null, bool, int, string, array, object.

struct JsonValue {
    enum Type { Null, Bool, Int, String, Array, Object };
    using ArrayType = std::vector<JsonValue>;
    using ObjectType = std::map<std::string, JsonValue>;

    Type type = Null;
    bool                vBool = false;
    int                 vInt = 0;
    std::string         vString;
    ArrayType           vArray;
    ObjectType          vObject;

    static JsonValue makeObject() { JsonValue v; v.type = Object; return v; }
    static JsonValue makeArray()  { JsonValue v; v.type = Array;  return v; }
    static JsonValue makeString(const std::string& s) { JsonValue v; v.type = String; v.vString = s; return v; }
    static JsonValue makeInt(int n) { JsonValue v; v.type = Int; v.vInt = n; return v; }
    static JsonValue makeBool(bool b) { JsonValue v; v.type = Bool; v.vBool = b; return v; }

    JsonValue& operator[](const std::string& key) {
        if (type != Object) { type = Object; vObject.clear(); }
        return vObject[key];
    }

    bool has(const std::string& key) const {
        return type == Object && vObject.count(key);
    }

    JsonValue& at(const std::string& key) {
        return vObject.at(key);
    }

    const JsonValue& at(const std::string& key) const {
        return vObject.at(key);
    }

    std::string asString(const std::string& defaultVal = "") const {
        return type == String ? vString : defaultVal;
    }

    int asInt(int defaultVal = 0) const {
        return type == Int ? vInt : defaultVal;
    }

    bool asBool(bool defaultVal = false) const {
        return type == Bool ? vBool : defaultVal;
    }

    const ArrayType& asArray() const { return vArray; }
    ArrayType& asArray() { return vArray; }

    // --- Serialize ---
    std::string serialize(int indent = 0) const {
        std::ostringstream oss;
        serializeTo(oss, indent, 0);
        return oss.str();
    }

private:
    void serializeTo(std::ostringstream& oss, int indent, int depth) const {
        std::string pad(depth * indent, ' ');
        std::string innerPad(indent > 0 ? (depth + 1) * indent : 0, ' ');

        switch (type) {
        case Null: oss << "null"; break;
        case Bool: oss << (vBool ? "true" : "false"); break;
        case Int:  oss << vInt; break;
        case String: oss << '"' << escape(vString) << '"'; break;
        case Array: {
            oss << '[';
            bool first = true;
            for (auto& e : vArray) {
                if (!first) oss << ',';
                if (indent > 0) { oss << '\n' << innerPad; }
                first = false;
                e.serializeTo(oss, indent, depth + 1);
            }
            if (indent > 0 && !vArray.empty()) oss << '\n' << pad;
            oss << ']';
            break;
        }
        case Object: {
            oss << '{';
            bool first = true;
            for (auto& [k, v] : vObject) {
                if (!first) oss << ',';
                if (indent > 0) oss << '\n' << innerPad;
                first = false;
                oss << '"' << escape(k) << "\":";
                if (indent > 0) oss << ' ';
                v.serializeTo(oss, indent, depth + 1);
            }
            if (indent > 0 && !vObject.empty()) oss << '\n' << pad;
            oss << '}';
            break;
        }
        }
    }

    static std::string escape(const std::string& s) {
        std::string o;
        for (char c : s) {
            switch (c) {
            case '"':  o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n"; break;
            case '\r': o += "\\r"; break;
            case '\t': o += "\\t"; break;
            default:   o += c;
            }
        }
        return o;
    }

public:
    // --- Parser ---
    static JsonValue parse(const std::string& json) {
        size_t i = 0;
        return parseValue(json, i);
    }

private:
    static void skipWs(const std::string& s, size_t& i) {
        while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
    }

    static JsonValue parseValue(const std::string& s, size_t& i) {
        skipWs(s, i);
        if (i >= s.size()) throw std::runtime_error("unexpected end of JSON");
        char c = s[i];
        if (c == '{') return parseObject(s, i);
        if (c == '[') return parseArray(s, i);
        if (c == '"') return parseString(s, i);
        if (c == 't' || c == 'f') return parseBool(s, i);
        if (c == 'n') return parseNull(s, i);
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) return parseInt(s, i);
        throw std::runtime_error(std::string("unexpected character: ") + c);
    }

    static JsonValue parseObject(const std::string& s, size_t& i) {
        JsonValue v = makeObject();
        ++i; // '{'
        skipWs(s, i);
        if (s[i] == '}') { ++i; return v; }
        while (true) {
            skipWs(s, i);
            auto key = parseString(s, i).vString;
            skipWs(s, i);
            if (s[i] != ':') throw std::runtime_error("expected ':'");
            ++i;
            v[key] = parseValue(s, i);
            skipWs(s, i);
            if (s[i] == '}') { ++i; break; }
            if (s[i] != ',') throw std::runtime_error("expected ',' or '}'");
            ++i;
        }
        return v;
    }

    static JsonValue parseArray(const std::string& s, size_t& i) {
        JsonValue v = makeArray();
        ++i; // '['
        skipWs(s, i);
        if (s[i] == ']') { ++i; return v; }
        while (true) {
            v.vArray.push_back(parseValue(s, i));
            skipWs(s, i);
            if (s[i] == ']') { ++i; break; }
            if (s[i] != ',') throw std::runtime_error("expected ',' or ']'");
            ++i;
        }
        return v;
    }

    static JsonValue parseString(const std::string& s, size_t& i) {
        ++i; // '"'
        std::string str;
        while (i < s.size() && s[i] != '"') {
            if (s[i] == '\\') {
                ++i;
                if (i >= s.size()) break;
                switch (s[i]) {
                case '"':  str += '"';  break;
                case '\\': str += '\\'; break;
                case '/':  str += '/';  break;
                case 'n':  str += '\n'; break;
                case 'r':  str += '\r'; break;
                case 't':  str += '\t'; break;
                default:   str += s[i]; break;
                }
            } else {
                str += s[i];
            }
            ++i;
        }
        ++i; // '"'
        JsonValue v; v.type = String; v.vString = str;
        return v;
    }

    static JsonValue parseBool(const std::string& s, size_t& i) {
        JsonValue v; v.type = Bool;
        if (s.substr(i, 4) == "true")  { v.vBool = true;  i += 4; }
        else if (s.substr(i, 5) == "false") { v.vBool = false; i += 5; }
        else throw std::runtime_error("expected true or false");
        return v;
    }

    static JsonValue parseNull(const std::string& s, size_t& i) {
        if (s.substr(i, 4) != "null") throw std::runtime_error("expected null");
        i += 4;
        return JsonValue{};
    }

    static JsonValue parseInt(const std::string& s, size_t& i) {
        size_t end = i;
        if (s[end] == '-') ++end;
        while (end < s.size() && std::isdigit(static_cast<unsigned char>(s[end]))) ++end;
        JsonValue v; v.type = Int; v.vInt = std::stoi(s.substr(i, end - i));
        i = end;
        return v;
    }
};
