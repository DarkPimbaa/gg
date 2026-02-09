#include "gg_ws/json.hpp"
#include <cmath>
#include <cstring>
#include <sstream>
#include <iomanip>

namespace gg {

// ============================================
// JSON Nulo Estático
// ============================================
Json& Json::nullJson() noexcept {
    static Json null;
    return null;
}

// ============================================
// Construtor com initializer_list
// ============================================
Json::Json(std::initializer_list<Json> init) {
    // Detecta se é object ou array
    // Object: todos elementos são arrays de 2 elementos onde primeiro é string
    bool isObject = true;
    for (const auto& item : init) {
        if (!item.isArray() || item.size() != 2 || !item[0].isString()) {
            isObject = false;
            break;
        }
    }
    
    if (isObject && init.size() > 0) {
        type_ = Type::Object;
        Object obj;
        for (const auto& item : init) {
            obj[std::string(item[0].getString())] = item[1];
        }
        value_ = std::move(obj);
    } else {
        type_ = Type::Array;
        value_ = Array(init);
    }
}

// ============================================
// Parser Interno
// ============================================
namespace {

class JsonParser {
public:
    explicit JsonParser(std::string_view input) : input_(input), pos_(0) {}
    
    std::optional<Json> parse() noexcept {
        try {
            skipWhitespace();
            if (pos_ >= input_.size()) {
                return std::nullopt;
            }
            
            auto result = parseValue();
            if (!result) return std::nullopt;
            
            skipWhitespace();
            // Deve ter consumido toda a entrada
            if (pos_ != input_.size()) {
                return std::nullopt;
            }
            
            return result;
        } catch (...) {
            return std::nullopt;
        }
    }

private:
    std::string_view input_;
    size_t pos_;
    
    char peek() const noexcept {
        if (pos_ >= input_.size()) return '\0';
        return input_[pos_];
    }
    
    char consume() noexcept {
        if (pos_ >= input_.size()) return '\0';
        return input_[pos_++];
    }
    
    bool consume(char expected) noexcept {
        if (peek() != expected) return false;
        pos_++;
        return true;
    }
    
    void skipWhitespace() noexcept {
        while (pos_ < input_.size()) {
            char c = input_[pos_];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                pos_++;
            } else {
                break;
            }
        }
    }
    
    std::optional<Json> parseValue() noexcept {
        skipWhitespace();
        
        char c = peek();
        
        if (c == 'n') return parseNull();
        if (c == 't' || c == 'f') return parseBool();
        if (c == '"') return parseString();
        if (c == '[') return parseArray();
        if (c == '{') return parseObject();
        if (c == '-' || (c >= '0' && c <= '9')) return parseNumber();
        
        return std::nullopt;
    }
    
    std::optional<Json> parseNull() noexcept {
        if (input_.substr(pos_, 4) == "null") {
            pos_ += 4;
            return Json(nullptr);
        }
        return std::nullopt;
    }
    
    std::optional<Json> parseBool() noexcept {
        if (input_.substr(pos_, 4) == "true") {
            pos_ += 4;
            return Json(true);
        }
        if (input_.substr(pos_, 5) == "false") {
            pos_ += 5;
            return Json(false);
        }
        return std::nullopt;
    }
    
    std::optional<Json> parseNumber() noexcept {
        size_t start = pos_;
        
        // Opcional: sinal negativo
        if (peek() == '-') consume();
        
        // Parte inteira
        if (peek() == '0') {
            consume();
        } else if (peek() >= '1' && peek() <= '9') {
            while (peek() >= '0' && peek() <= '9') consume();
        } else {
            return std::nullopt;
        }
        
        // Parte decimal (opcional)
        if (peek() == '.') {
            consume();
            if (!(peek() >= '0' && peek() <= '9')) return std::nullopt;
            while (peek() >= '0' && peek() <= '9') consume();
        }
        
        // Expoente (opcional)
        if (peek() == 'e' || peek() == 'E') {
            consume();
            if (peek() == '+' || peek() == '-') consume();
            if (!(peek() >= '0' && peek() <= '9')) return std::nullopt;
            while (peek() >= '0' && peek() <= '9') consume();
        }
        
        std::string numStr(input_.substr(start, pos_ - start));
        
        // Conversão segura
        char* end;
        double value = std::strtod(numStr.c_str(), &end);
        if (end != numStr.c_str() + numStr.size()) {
            return std::nullopt;
        }
        
        return Json(value);
    }
    
    std::optional<Json> parseString() noexcept {
        if (!consume('"')) return std::nullopt;
        
        std::string result;
        result.reserve(32);
        
        while (pos_ < input_.size()) {
            char c = consume();
            
            if (c == '"') {
                return Json(std::move(result));
            }
            
            if (c == '\\') {
                if (pos_ >= input_.size()) return std::nullopt;
                
                char escaped = consume();
                switch (escaped) {
                    case '"':  result += '"'; break;
                    case '\\': result += '\\'; break;
                    case '/':  result += '/'; break;
                    case 'b':  result += '\b'; break;
                    case 'f':  result += '\f'; break;
                    case 'n':  result += '\n'; break;
                    case 'r':  result += '\r'; break;
                    case 't':  result += '\t'; break;
                    case 'u': {
                        // Unicode escape \uXXXX
                        if (pos_ + 4 > input_.size()) return std::nullopt;
                        
                        uint32_t codepoint = 0;
                        for (int i = 0; i < 4; i++) {
                            char hex = consume();
                            codepoint *= 16;
                            if (hex >= '0' && hex <= '9') codepoint += hex - '0';
                            else if (hex >= 'a' && hex <= 'f') codepoint += 10 + hex - 'a';
                            else if (hex >= 'A' && hex <= 'F') codepoint += 10 + hex - 'A';
                            else return std::nullopt;
                        }
                        
                        // Converte para UTF-8
                        if (codepoint < 0x80) {
                            result += static_cast<char>(codepoint);
                        } else if (codepoint < 0x800) {
                            result += static_cast<char>(0xC0 | (codepoint >> 6));
                            result += static_cast<char>(0x80 | (codepoint & 0x3F));
                        } else {
                            result += static_cast<char>(0xE0 | (codepoint >> 12));
                            result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                            result += static_cast<char>(0x80 | (codepoint & 0x3F));
                        }
                        break;
                    }
                    default:
                        return std::nullopt;
                }
            } else if (static_cast<unsigned char>(c) < 0x20) {
                // Caracteres de controle não são permitidos
                return std::nullopt;
            } else {
                result += c;
            }
        }
        
        return std::nullopt; // String não terminada
    }
    
    std::optional<Json> parseArray() noexcept {
        if (!consume('[')) return std::nullopt;
        
        Json::Array arr;
        skipWhitespace();
        
        if (peek() == ']') {
            consume();
            return Json(std::move(arr));
        }
        
        while (true) {
            auto value = parseValue();
            if (!value) return std::nullopt;
            
            arr.push_back(std::move(*value));
            
            skipWhitespace();
            
            if (peek() == ']') {
                consume();
                return Json(std::move(arr));
            }
            
            if (!consume(',')) return std::nullopt;
        }
    }
    
    std::optional<Json> parseObject() noexcept {
        if (!consume('{')) return std::nullopt;
        
        Json::Object obj;
        skipWhitespace();
        
        if (peek() == '}') {
            consume();
            return Json(std::move(obj));
        }
        
        while (true) {
            skipWhitespace();
            
            // Chave deve ser string
            auto key = parseString();
            if (!key || !key->isString()) return std::nullopt;
            
            skipWhitespace();
            if (!consume(':')) return std::nullopt;
            
            auto value = parseValue();
            if (!value) return std::nullopt;
            
            obj[std::string(key->getString())] = std::move(*value);
            
            skipWhitespace();
            
            if (peek() == '}') {
                consume();
                return Json(std::move(obj));
            }
            
            if (!consume(',')) return std::nullopt;
        }
    }
};

} // anonymous namespace

// ============================================
// Parsing
// ============================================
std::optional<Json> Json::parse(std::string_view input) noexcept {
    JsonParser parser(input);
    return parser.parse();
}

bool Json::isValid(std::string_view input) noexcept {
    return parse(input).has_value();
}

// ============================================
// Serialização
// ============================================
namespace {

void escapeString(std::ostringstream& out, std::string_view str) {
    out << '"';
    for (char c : str) {
        switch (c) {
            case '"':  out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\b': out << "\\b"; break;
            case '\f': out << "\\f"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    out << "\\u" << std::hex << std::setfill('0') << std::setw(4) 
                        << static_cast<int>(static_cast<unsigned char>(c));
                } else {
                    out << c;
                }
        }
    }
    out << '"';
}

void stringifyImpl(std::ostringstream& out, const Json& json, bool pretty, int indent) {
    std::string indentStr(indent * 2, ' ');
    std::string nextIndent((indent + 1) * 2, ' ');
    
    switch (json.type()) {
        case Json::Type::Null:
            out << "null";
            break;
            
        case Json::Type::Bool:
            out << (json.getBool() ? "true" : "false");
            break;
            
        case Json::Type::Number: {
            double num = json.getNumber();
            if (std::isnan(num) || std::isinf(num)) {
                out << "null";
            } else if (num == std::floor(num) && std::abs(num) < 1e15) {
                out << static_cast<int64_t>(num);
            } else {
                out << std::setprecision(17) << num;
            }
            break;
        }
            
        case Json::Type::String:
            escapeString(out, json.getString());
            break;
            
        case Json::Type::Array: {
            out << '[';
            bool first = true;
            json.forEach([&](const Json& item) {
                if (!first) out << ',';
                if (pretty) out << '\n' << nextIndent;
                first = false;
                stringifyImpl(out, item, pretty, indent + 1);
            });
            if (pretty && json.size() > 0) out << '\n' << indentStr;
            out << ']';
            break;
        }
            
        case Json::Type::Object: {
            out << '{';
            bool first = true;
            json.forEachPair([&](const std::string& key, const Json& value) {
                if (!first) out << ',';
                if (pretty) out << '\n' << nextIndent;
                first = false;
                escapeString(out, key);
                out << ':';
                if (pretty) out << ' ';
                stringifyImpl(out, value, pretty, indent + 1);
            });
            if (pretty && json.size() > 0) out << '\n' << indentStr;
            out << '}';
            break;
        }
    }
}

} // anonymous namespace

std::string Json::stringify(bool pretty) const {
    std::ostringstream out;
    stringifyImpl(out, *this, pretty, 0);
    return out.str();
}

// ============================================
// Getters
// ============================================
bool Json::getBool(bool defaultValue) const noexcept {
    if (type_ != Type::Bool) return defaultValue;
    return std::get<bool>(value_);
}

double Json::getNumber(double defaultValue) const noexcept {
    if (type_ != Type::Number) return defaultValue;
    return std::get<double>(value_);
}

int64_t Json::getInt(int64_t defaultValue) const noexcept {
    if (type_ != Type::Number) return defaultValue;
    return static_cast<int64_t>(std::get<double>(value_));
}

std::string_view Json::getString(std::string_view defaultValue) const noexcept {
    if (type_ != Type::String) return defaultValue;
    return std::get<std::string>(value_);
}

std::string Json::getStringCopy(const std::string& defaultValue) const {
    if (type_ != Type::String) return defaultValue;
    return std::get<std::string>(value_);
}

// ============================================
// Acesso a Arrays
// ============================================
const Json& Json::operator[](size_t index) const noexcept {
    if (type_ != Type::Array) return nullJson();
    const auto& arr = std::get<Array>(value_);
    if (index >= arr.size()) return nullJson();
    return arr[index];
}

Json& Json::operator[](size_t index) noexcept {
    if (type_ != Type::Array) return nullJson();
    auto& arr = std::get<Array>(value_);
    if (index >= arr.size()) return nullJson();
    return arr[index];
}

size_t Json::size() const noexcept {
    if (type_ == Type::Array) {
        return std::get<Array>(value_).size();
    }
    if (type_ == Type::Object) {
        return std::get<Object>(value_).size();
    }
    return 0;
}

bool Json::empty() const noexcept {
    return size() == 0;
}

// ============================================
// Acesso a Objects
// ============================================
const Json& Json::operator[](std::string_view key) const noexcept {
    if (type_ != Type::Object) return nullJson();
    const auto& obj = std::get<Object>(value_);
    auto it = obj.find(std::string(key));
    if (it == obj.end()) return nullJson();
    return it->second;
}

Json& Json::operator[](std::string_view key) {
    if (type_ == Type::Null) {
        // Auto-convert to object
        type_ = Type::Object;
        value_ = Object{};
    }
    if (type_ != Type::Object) return nullJson();
    auto& obj = std::get<Object>(value_);
    return obj[std::string(key)];
}

const Json& Json::get(std::string_view key) const noexcept {
    return (*this)[key];
}

bool Json::contains(std::string_view key) const noexcept {
    if (type_ != Type::Object) return false;
    const auto& obj = std::get<Object>(value_);
    return obj.find(std::string(key)) != obj.end();
}

std::vector<std::string> Json::keys() const {
    std::vector<std::string> result;
    if (type_ == Type::Object) {
        const auto& obj = std::get<Object>(value_);
        result.reserve(obj.size());
        for (const auto& [key, _] : obj) {
            result.push_back(key);
        }
    }
    return result;
}

// ============================================
// Modificação
// ============================================
void Json::push(Json value) {
    if (type_ == Type::Null) {
        type_ = Type::Array;
        value_ = Array{};
    }
    if (type_ != Type::Array) return;
    std::get<Array>(value_).push_back(std::move(value));
}

void Json::pop() {
    if (type_ != Type::Array) return;
    auto& arr = std::get<Array>(value_);
    if (!arr.empty()) arr.pop_back();
}

void Json::erase(std::string_view key) {
    if (type_ != Type::Object) return;
    std::get<Object>(value_).erase(std::string(key));
}

void Json::clear() {
    if (type_ == Type::Array) {
        std::get<Array>(value_).clear();
    } else if (type_ == Type::Object) {
        std::get<Object>(value_).clear();
    }
}

// ============================================
// Comparação
// ============================================
bool Json::operator==(const Json& other) const noexcept {
    if (type_ != other.type_) return false;
    return value_ == other.value_;
}

} // namespace gg
