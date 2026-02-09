#pragma once

#include <string>
#include <map>
#include <vector>
#include <sstream>
#include <algorithm>
#include <stdexcept>

namespace ggnet {

// A very simple JSON value wrapper
class Json {
public:
    enum Type { NULL_VAL, OBJECT, ARRAY, STRING, NUMBER, BOOLEAN };
    Type type = NULL_VAL;
    std::string str_val;
    std::map<std::string, Json> obj_val;
    std::vector<Json> arr_val;

    Json() {}
    Json(const std::string& s) : type(STRING), str_val(s) {}
    Json(double d) : type(NUMBER), str_val(std::to_string(d)) {}
    Json(bool b) : type(BOOLEAN), str_val(b ? "true" : "false") {}

    // Parse static method
    static Json parse(const std::string& raw) {
        size_t pos = 0;
        return parseInternal(raw, pos);
    }

    // Accessors
    Json& operator[](const std::string& key) {
        if (type != OBJECT) throw std::runtime_error("Not an object");
        return obj_val[key];
    }
    
    Json& operator[](size_t index) {
        if (type != ARRAY) throw std::runtime_error("Not an array");
        if (index >= arr_val.size()) throw std::runtime_error("Index out of bounds");
        return arr_val[index];
    }

    std::string as_string() const { return str_val; }
    
    double as_double() const {
        if (type == NUMBER || type == STRING) {
            try { return std::stod(str_val); } catch(...) { return 0.0; }
        }
        return 0.0;
    }

    int as_int() const {
        if (type == NUMBER || type == STRING) {
            try { return std::stoi(str_val); } catch(...) { return 0; }
        }
        return 0;
    }

    bool as_bool() const {
        return str_val == "true";
    }

private:
    static void skipWhitespace(const std::string& json, size_t& pos) {
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n' || json[pos] == '\r')) {
            pos++;
        }
    }

    static Json parseInternal(const std::string& json, size_t& pos) {
        skipWhitespace(json, pos);
        if (pos >= json.size()) return Json();

        char c = json[pos];
        if (c == '{') {
            Json obj;
            obj.type = OBJECT;
            pos++; // skip {
            while (pos < json.size()) {
                skipWhitespace(json, pos);
                if (json[pos] == '}') { pos++; break; }

                // Key
                Json key = parseInternal(json, pos);
                if (key.type != STRING) throw std::runtime_error("Invalid key");
                
                skipWhitespace(json, pos);
                if (json[pos] != ':') throw std::runtime_error("Expected :");
                pos++;

                // Value
                Json val = parseInternal(json, pos);
                obj.obj_val[key.str_val] = val;

                skipWhitespace(json, pos);
                if (json[pos] == ',') pos++;
                else if (json[pos] == '}') { pos++; break; }
                else throw std::runtime_error("Expected , or }");
            }
            return obj;
        } else if (c == '[') {
            Json arr;
            arr.type = ARRAY;
            pos++; // skip [
            while (pos < json.size()) {
                skipWhitespace(json, pos);
                if (json[pos] == ']') { pos++; break; }

                Json val = parseInternal(json, pos);
                arr.arr_val.push_back(val);

                skipWhitespace(json, pos);
                if (json[pos] == ',') pos++;
                else if (json[pos] == ']') { pos++; break; }
                else throw std::runtime_error("Expected , or ]");
            }
            return arr;
        } else if (c == '"') {
            Json str;
            str.type = STRING;
            pos++; // skip "
            std::string s;
            while (pos < json.size()) {
                if (json[pos] == '"') { pos++; break; }
                if (json[pos] == '\\') pos++; // Skip escape for now (simple)
                s += json[pos];
                pos++;
            }
            str.str_val = s;
            return str;
        } else if (c == 't' || c == 'f') {
            // boolean
             std::string b;
             while(pos < json.length() && (json[pos] >= 'a' && json[pos] <= 'z')) b += json[pos++];
             Json val;
             val.type = BOOLEAN;
             val.str_val = b;
             return val;
        } else {
            // Number
            std::string num;
            while (pos < json.size() && (isdigit(json[pos]) || json[pos] == '.' || json[pos] == '-' || json[pos] == 'e' || json[pos] == 'E')) {
                num += json[pos];
                pos++;
            }
            Json val;
            val.type = NUMBER;
            val.str_val = num;
            return val;
        }
    }
};

} // namespace ggnet
