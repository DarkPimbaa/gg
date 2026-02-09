#include "gg_ws/json.hpp"
#include <iostream>
#include <cassert>

using namespace gg;

// ============================================
// Macros de Teste
// ============================================
#define TEST(name) void test_##name()
#define RUN_TEST(name) \
    std::cout << "  " << #name << "... "; \
    test_##name(); \
    std::cout << "OK\n"

#define ASSERT(expr) \
    if (!(expr)) { \
        std::cerr << "\nFALHA: " << #expr << " em " << __FILE__ << ":" << __LINE__ << "\n"; \
        std::exit(1); \
    }

#define ASSERT_EQ(a, b) ASSERT((a) == (b))
#define ASSERT_NE(a, b) ASSERT((a) != (b))

// ============================================
// Testes de Parsing
// ============================================
TEST(parse_null) {
    auto json = Json::parse("null");
    ASSERT(json.has_value());
    ASSERT(json->isNull());
}

TEST(parse_bool_true) {
    auto json = Json::parse("true");
    ASSERT(json.has_value());
    ASSERT(json->isBool());
    ASSERT_EQ(json->getBool(), true);
}

TEST(parse_bool_false) {
    auto json = Json::parse("false");
    ASSERT(json.has_value());
    ASSERT(json->isBool());
    ASSERT_EQ(json->getBool(), false);
}

TEST(parse_number_int) {
    auto json = Json::parse("42");
    ASSERT(json.has_value());
    ASSERT(json->isNumber());
    ASSERT_EQ(json->getNumber(), 42.0);
    ASSERT_EQ(json->getInt(), 42);
}

TEST(parse_number_negative) {
    auto json = Json::parse("-123");
    ASSERT(json.has_value());
    ASSERT_EQ(json->getNumber(), -123.0);
}

TEST(parse_number_float) {
    auto json = Json::parse("3.14159");
    ASSERT(json.has_value());
    ASSERT(json->getNumber() > 3.14 && json->getNumber() < 3.15);
}

TEST(parse_number_exponent) {
    auto json = Json::parse("1.5e10");
    ASSERT(json.has_value());
    ASSERT_EQ(json->getNumber(), 1.5e10);
}

TEST(parse_string_simple) {
    auto json = Json::parse("\"hello\"");
    ASSERT(json.has_value());
    ASSERT(json->isString());
    ASSERT_EQ(json->getString(), "hello");
}

TEST(parse_string_escape) {
    auto json = Json::parse(R"("hello\nworld")");
    ASSERT(json.has_value());
    ASSERT_EQ(json->getString(), "hello\nworld");
}

TEST(parse_string_unicode) {
    auto json = Json::parse(R"("\u0048\u0065\u006c\u006c\u006f")");
    ASSERT(json.has_value());
    ASSERT_EQ(json->getString(), "Hello");
}

TEST(parse_array_empty) {
    auto json = Json::parse("[]");
    ASSERT(json.has_value());
    ASSERT(json->isArray());
    ASSERT_EQ(json->size(), 0);
}

TEST(parse_array_simple) {
    auto json = Json::parse("[1, 2, 3]");
    ASSERT(json.has_value());
    ASSERT(json->isArray());
    ASSERT_EQ(json->size(), 3);
    ASSERT_EQ((*json)[0].getNumber(), 1.0);
    ASSERT_EQ((*json)[1].getNumber(), 2.0);
    ASSERT_EQ((*json)[2].getNumber(), 3.0);
}

TEST(parse_array_mixed) {
    auto json = Json::parse(R"([1, "two", true, null])");
    ASSERT(json.has_value());
    ASSERT_EQ(json->size(), 4);
    ASSERT((*json)[0].isNumber());
    ASSERT((*json)[1].isString());
    ASSERT((*json)[2].isBool());
    ASSERT((*json)[3].isNull());
}

TEST(parse_object_empty) {
    auto json = Json::parse("{}");
    ASSERT(json.has_value());
    ASSERT(json->isObject());
    ASSERT_EQ(json->size(), 0);
}

TEST(parse_object_simple) {
    auto json = Json::parse(R"({"name": "test", "value": 42})");
    ASSERT(json.has_value());
    ASSERT(json->isObject());
    ASSERT_EQ(json->get("name").getString(), "test");
    ASSERT_EQ(json->get("value").getNumber(), 42.0);
}

TEST(parse_object_nested) {
    auto json = Json::parse(R"({"outer": {"inner": "value"}})");
    ASSERT(json.has_value());
    ASSERT(json->get("outer").isObject());
    ASSERT_EQ(json->get("outer").get("inner").getString(), "value");
}

TEST(parse_complex) {
    auto json = Json::parse(R"({
        "users": [
            {"id": 1, "name": "Alice"},
            {"id": 2, "name": "Bob"}
        ],
        "count": 2,
        "active": true
    })");
    
    ASSERT(json.has_value());
    ASSERT(json->get("users").isArray());
    ASSERT_EQ(json->get("users").size(), 2);
    ASSERT_EQ(json->get("users")[0].get("name").getString(), "Alice");
    ASSERT_EQ(json->get("count").getInt(), 2);
    ASSERT_EQ(json->get("active").getBool(), true);
}

// ============================================
// Testes de Parsing Inválido
// ============================================
TEST(parse_invalid_empty) {
    ASSERT(!Json::parse("").has_value());
}

TEST(parse_invalid_trailing) {
    ASSERT(!Json::parse("123 456").has_value());
}

TEST(parse_invalid_unclosed_string) {
    ASSERT(!Json::parse("\"hello").has_value());
}

TEST(parse_invalid_unclosed_array) {
    ASSERT(!Json::parse("[1, 2").has_value());
}

TEST(parse_invalid_unclosed_object) {
    ASSERT(!Json::parse("{\"key\": 1").has_value());
}

// ============================================
// Testes de Serialização
// ============================================
TEST(stringify_null) {
    Json json(nullptr);
    ASSERT_EQ(json.stringify(), "null");
}

TEST(stringify_bool) {
    ASSERT_EQ(Json(true).stringify(), "true");
    ASSERT_EQ(Json(false).stringify(), "false");
}

TEST(stringify_number) {
    ASSERT_EQ(Json(42).stringify(), "42");
    ASSERT_EQ(Json(-123).stringify(), "-123");
}

TEST(stringify_string) {
    Json json("hello");
    ASSERT_EQ(json.stringify(), "\"hello\"");
}

TEST(stringify_string_escape) {
    Json json("hello\nworld");
    ASSERT_EQ(json.stringify(), "\"hello\\nworld\"");
}

TEST(stringify_array) {
    Json::Array arr = {Json(1), Json(2), Json(3)};
    Json json(arr);
    ASSERT_EQ(json.stringify(), "[1,2,3]");
}

TEST(stringify_object) {
    Json json = Json::object();
    json["name"] = "test";
    json["value"] = 42;
    
    std::string str = json.stringify();
    // Ordem pode variar, então verificamos se contém os pares
    ASSERT(str.find("\"name\"") != std::string::npos);
    ASSERT(str.find("\"test\"") != std::string::npos);
    ASSERT(str.find("\"value\"") != std::string::npos);
    ASSERT(str.find("42") != std::string::npos);
}

// ============================================
// Testes de Acesso Seguro
// ============================================
TEST(safe_access_missing_key) {
    auto json = Json::parse(R"({"name": "test"})");
    ASSERT(json.has_value());
    ASSERT(json->get("missing").isNull());
    ASSERT_EQ(json->get("missing").getString("default"), "default");
}

TEST(safe_access_wrong_type) {
    auto json = Json::parse("42");
    ASSERT(json.has_value());
    ASSERT_EQ(json->getString("fallback"), "fallback");
    ASSERT_EQ(json->getBool(true), true);
}

TEST(safe_access_array_bounds) {
    auto json = Json::parse("[1, 2, 3]");
    ASSERT(json.has_value());
    ASSERT((*json)[100].isNull());
    ASSERT_EQ((*json)[100].getNumber(999), 999.0);
}

// ============================================
// Testes de Modificação
// ============================================
TEST(modify_object) {
    Json json = Json::object();
    json["key1"] = "value1";
    json["key2"] = 42;
    json["key3"] = true;
    
    ASSERT_EQ(json.get("key1").getString(), "value1");
    ASSERT_EQ(json.get("key2").getNumber(), 42.0);
    ASSERT_EQ(json.get("key3").getBool(), true);
}

TEST(modify_array) {
    Json json = Json::array();
    json.push(1);
    json.push("two");
    json.push(true);
    
    ASSERT_EQ(json.size(), 3);
    ASSERT_EQ(json[0].getNumber(), 1.0);
    ASSERT_EQ(json[1].getString(), "two");
    ASSERT_EQ(json[2].getBool(), true);
}

TEST(modify_erase) {
    Json json = Json::object();
    json["keep"] = 1;
    json["remove"] = 2;
    
    json.erase("remove");
    
    ASSERT(json.contains("keep"));
    ASSERT(!json.contains("remove"));
}

// ============================================
// Testes de Comparação
// ============================================
TEST(compare_equal) {
    auto a = Json::parse(R"({"x": 1})");
    auto b = Json::parse(R"({"x": 1})");
    ASSERT(a.has_value() && b.has_value());
    ASSERT(*a == *b);
}

TEST(compare_not_equal) {
    auto a = Json::parse("1");
    auto b = Json::parse("2");
    ASSERT(a.has_value() && b.has_value());
    ASSERT(*a != *b);
}

// ============================================
// Testes de Roundtrip
// ============================================
TEST(roundtrip) {
    std::string original = R"({"array":[1,2,3],"bool":true,"null":null,"number":42,"string":"hello"})";
    auto parsed = Json::parse(original);
    ASSERT(parsed.has_value());
    
    std::string serialized = parsed->stringify();
    auto reparsed = Json::parse(serialized);
    ASSERT(reparsed.has_value());
    
    ASSERT(*parsed == *reparsed);
}

// ============================================
// Main
// ============================================
int main() {
    std::cout << "=== Testes do Parser JSON ===\n\n";
    
    std::cout << "Parsing básico:\n";
    RUN_TEST(parse_null);
    RUN_TEST(parse_bool_true);
    RUN_TEST(parse_bool_false);
    RUN_TEST(parse_number_int);
    RUN_TEST(parse_number_negative);
    RUN_TEST(parse_number_float);
    RUN_TEST(parse_number_exponent);
    RUN_TEST(parse_string_simple);
    RUN_TEST(parse_string_escape);
    RUN_TEST(parse_string_unicode);
    
    std::cout << "\nArrays e Objects:\n";
    RUN_TEST(parse_array_empty);
    RUN_TEST(parse_array_simple);
    RUN_TEST(parse_array_mixed);
    RUN_TEST(parse_object_empty);
    RUN_TEST(parse_object_simple);
    RUN_TEST(parse_object_nested);
    RUN_TEST(parse_complex);
    
    std::cout << "\nParsing inválido:\n";
    RUN_TEST(parse_invalid_empty);
    RUN_TEST(parse_invalid_trailing);
    RUN_TEST(parse_invalid_unclosed_string);
    RUN_TEST(parse_invalid_unclosed_array);
    RUN_TEST(parse_invalid_unclosed_object);
    
    std::cout << "\nSerialização:\n";
    RUN_TEST(stringify_null);
    RUN_TEST(stringify_bool);
    RUN_TEST(stringify_number);
    RUN_TEST(stringify_string);
    RUN_TEST(stringify_string_escape);
    RUN_TEST(stringify_array);
    RUN_TEST(stringify_object);
    
    std::cout << "\nAcesso seguro:\n";
    RUN_TEST(safe_access_missing_key);
    RUN_TEST(safe_access_wrong_type);
    RUN_TEST(safe_access_array_bounds);
    
    std::cout << "\nModificação:\n";
    RUN_TEST(modify_object);
    RUN_TEST(modify_array);
    RUN_TEST(modify_erase);
    
    std::cout << "\nComparação:\n";
    RUN_TEST(compare_equal);
    RUN_TEST(compare_not_equal);
    
    std::cout << "\nRoundtrip:\n";
    RUN_TEST(roundtrip);
    
    std::cout << "\n=== TODOS OS TESTES PASSARAM ===\n";
    return 0;
}
