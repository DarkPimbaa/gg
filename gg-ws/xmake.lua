set_project("gg_ws")
set_version("1.0.0")
set_languages("c++17")

-- Modos de compilação
add_rules("mode.debug", "mode.release")

-- Flags de performance (release)
if is_mode("release") then
    set_optimize("fastest")
    add_cxxflags("-fno-rtti", {force = true})
    add_defines("NDEBUG")
end

-- Flags de segurança de memória (debug)
if is_mode("debug") then
    set_optimize("none")
    add_cxxflags("-g", "-O0", {force = true})
    -- Sanitizers para detectar bugs
    if not is_plat("windows") then
        add_cxxflags("-fsanitize=address,undefined", {force = true})
        add_ldflags("-fsanitize=address,undefined", {force = true})
    end
end

-- Dependência: OpenSSL para TLS
add_requires("openssl")

-- ============================================
-- Biblioteca Principal
-- ============================================
target("gg_ws")
    set_kind("static")
    add_files("src/*.cpp")
    add_includedirs("include", {public = true})
    add_includedirs("src", {private = true})  -- Para headers internos
    add_packages("openssl")
    add_headerfiles("include/(gg_ws/*.hpp)")
    
    -- Flags específicas por plataforma
    if is_plat("linux") then
        add_syslinks("pthread")
    elseif is_plat("windows") then
        add_syslinks("ws2_32", "crypt32")
    end

-- ============================================
-- Testes
-- ============================================
target("test_json")
    set_kind("binary")
    set_default(false)
    add_files("tests/test_json.cpp")
    add_deps("gg_ws")
    add_packages("openssl")

target("test_websocket")
    set_kind("binary")
    set_default(false)
    add_files("tests/test_websocket.cpp")
    add_deps("gg_ws")
    add_packages("openssl")

target("test_thread_safety")
    set_kind("binary")
    set_default(false)
    add_files("tests/test_thread_safety.cpp")
    add_deps("gg_ws")
    add_packages("openssl")

-- ============================================
-- Exemplo
-- ============================================
target("example_basic")
    set_kind("binary")
    set_default(false)
    add_files("examples/basic_client.cpp")
    add_deps("gg_ws")
    add_packages("openssl")
