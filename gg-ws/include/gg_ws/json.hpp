#pragma once

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace gg {

/**
 * @brief Parser e manipulador JSON minimalista, thread-safe e sem exceções.
 * 
 * Design Goals:
 * - API simples e intuitiva
 * - Sem exceções (retorna std::optional ou valores default)
 * - Suporte a move semantics
 * - Bounds checking em todos os acessos
 * 
 * Exemplo:
 * @code
 *   auto json = gg::Json::parse(R"({"name": "test", "value": 42})");
 *   if (json) {
 *       std::cout << json->get("name").getString() << "\n";
 *       std::cout << json->get("value").getNumber() << "\n";
 *   }
 * @endcode
 */
class Json {
public:
    enum class Type : uint8_t {
        Null,
        Bool,
        Number,
        String,
        Array,
        Object
    };

    using Array = std::vector<Json>;
    using Object = std::unordered_map<std::string, Json>;

private:
    Type type_ = Type::Null;
    std::variant<
        std::monostate,     // Null
        bool,               // Bool
        double,             // Number
        std::string,        // String
        Array,              // Array
        Object              // Object
    > value_;

    // JSON nulo estático para retornos seguros
    static Json& nullJson() noexcept;

public:
    // ============================================
    // Construtores
    // ============================================
    Json() noexcept = default;
    Json(std::nullptr_t) noexcept : type_(Type::Null) {}
    Json(bool value) noexcept : type_(Type::Bool), value_(value) {}
    Json(int value) noexcept : type_(Type::Number), value_(static_cast<double>(value)) {}
    Json(int64_t value) noexcept : type_(Type::Number), value_(static_cast<double>(value)) {}
    Json(double value) noexcept : type_(Type::Number), value_(value) {}
    Json(const char* value) : type_(Type::String), value_(std::string(value)) {}
    Json(std::string value) noexcept : type_(Type::String), value_(std::move(value)) {}
    Json(std::string_view value) : type_(Type::String), value_(std::string(value)) {}
    Json(Array value) noexcept : type_(Type::Array), value_(std::move(value)) {}
    Json(Object value) noexcept : type_(Type::Object), value_(std::move(value)) {}
    
    // Initializer list para arrays
    Json(std::initializer_list<Json> init);

    // ============================================
    // Parsing - nunca lança exceção
    // ============================================
    
    /**
     * @brief Faz parsing de uma string JSON.
     * @param input String JSON a ser parseada
     * @return std::optional<Json> contendo o resultado ou std::nullopt em caso de erro
     */
    [[nodiscard]] static std::optional<Json> parse(std::string_view input) noexcept;
    
    /**
     * @brief Verifica se uma string é JSON válido sem criar objeto.
     * @param input String JSON a ser validada
     * @return true se válido, false caso contrário
     */
    [[nodiscard]] static bool isValid(std::string_view input) noexcept;

    // ============================================
    // Serialização
    // ============================================
    
    /**
     * @brief Converte o JSON para string.
     * @param pretty Se true, formata com indentação
     * @return String JSON
     */
    [[nodiscard]] std::string stringify(bool pretty = false) const;
    
    /**
     * @brief Converte o JSON para string (alias para stringify).
     */
    [[nodiscard]] std::string dump(bool pretty = false) const { return stringify(pretty); }

    // ============================================
    // Verificação de Tipo
    // ============================================
    [[nodiscard]] Type type() const noexcept { return type_; }
    [[nodiscard]] bool isNull() const noexcept { return type_ == Type::Null; }
    [[nodiscard]] bool isBool() const noexcept { return type_ == Type::Bool; }
    [[nodiscard]] bool isNumber() const noexcept { return type_ == Type::Number; }
    [[nodiscard]] bool isString() const noexcept { return type_ == Type::String; }
    [[nodiscard]] bool isArray() const noexcept { return type_ == Type::Array; }
    [[nodiscard]] bool isObject() const noexcept { return type_ == Type::Object; }

    // ============================================
    // Getters Seguros (retornam default se tipo errado)
    // ============================================
    [[nodiscard]] bool getBool(bool defaultValue = false) const noexcept;
    [[nodiscard]] double getNumber(double defaultValue = 0.0) const noexcept;
    [[nodiscard]] int64_t getInt(int64_t defaultValue = 0) const noexcept;
    [[nodiscard]] std::string_view getString(std::string_view defaultValue = "") const noexcept;
    
    /**
     * @brief Retorna cópia da string (para uso quando o Json pode ser destruído).
     */
    [[nodiscard]] std::string getStringCopy(const std::string& defaultValue = "") const;

    // ============================================
    // Acesso a Arrays
    // ============================================
    
    /**
     * @brief Acesso por índice (apenas para arrays).
     * @param index Índice do elemento
     * @return Referência ao elemento ou Json nulo se inválido
     */
    [[nodiscard]] const Json& operator[](size_t index) const noexcept;
    [[nodiscard]] Json& operator[](size_t index) noexcept;
    
    /**
     * @brief Retorna tamanho do array/object.
     */
    [[nodiscard]] size_t size() const noexcept;
    
    /**
     * @brief Verifica se array/object está vazio.
     */
    [[nodiscard]] bool empty() const noexcept;

    // ============================================
    // Acesso a Objects
    // ============================================
    
    /**
     * @brief Acesso por chave (apenas para objects).
     * @param key Chave do elemento
     * @return Referência ao elemento ou Json nulo se não encontrado
     */
    [[nodiscard]] const Json& operator[](std::string_view key) const noexcept;
    [[nodiscard]] Json& operator[](std::string_view key);
    
    /**
     * @brief Acesso por chave (alias para operator[]).
     */
    [[nodiscard]] const Json& get(std::string_view key) const noexcept;
    
    /**
     * @brief Verifica se uma chave existe no object.
     */
    [[nodiscard]] bool contains(std::string_view key) const noexcept;
    
    /**
     * @brief Retorna todas as chaves do object.
     */
    [[nodiscard]] std::vector<std::string> keys() const;

    // ============================================
    // Iteração
    // ============================================
    
    /**
     * @brief Itera sobre elementos de array.
     * @param fn Função a ser chamada para cada elemento
     */
    template<typename Fn>
    void forEach(Fn&& fn) const {
        if (isArray()) {
            const auto& arr = std::get<Array>(value_);
            for (const auto& item : arr) {
                fn(item);
            }
        }
    }
    
    /**
     * @brief Itera sobre pares chave-valor de object.
     * @param fn Função a ser chamada para cada par (key, value)
     */
    template<typename Fn>
    void forEachPair(Fn&& fn) const {
        if (isObject()) {
            const auto& obj = std::get<Object>(value_);
            for (const auto& [key, value] : obj) {
                fn(key, value);
            }
        }
    }

    // ============================================
    // Modificação
    // ============================================
    
    /**
     * @brief Adiciona elemento ao array.
     */
    void push(Json value);
    
    /**
     * @brief Remove último elemento do array.
     */
    void pop();
    
    /**
     * @brief Remove chave do object.
     */
    void erase(std::string_view key);
    
    /**
     * @brief Limpa array/object.
     */
    void clear();

    // ============================================
    // Comparação
    // ============================================
    [[nodiscard]] bool operator==(const Json& other) const noexcept;
    [[nodiscard]] bool operator!=(const Json& other) const noexcept { return !(*this == other); }

    // ============================================
    // Factory Methods
    // ============================================
    [[nodiscard]] static Json array() { return Json(Array{}); }
    [[nodiscard]] static Json object() { return Json(Object{}); }
};

} // namespace gg
