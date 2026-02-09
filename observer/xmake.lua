add_rules("mode.debug", "mode.release")

set_languages("c++17")

target("GG_Observer")
    set_kind("headeronly")  -- Header-only library
    
    -- Headers que serão instalados
    add_headerfiles("src/*.hpp", {prefixdir = "GG_Observer"})
    add_headerfiles("src/GG_Observer.h", {prefixdir = "GG_Observer"})
    
    -- Include dirs públicos
    add_includedirs("src", {public = true})

-- Exemplo de uso (opcional, executável de teste)
target("GG_Observer_Example")
    set_kind("binary")
    set_default(false)  -- Não compila por padrão
    add_files("examples/*.cpp")
    add_includedirs("src")