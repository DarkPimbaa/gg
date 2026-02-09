# TerminalGUI

Biblioteca C++ **header-only** para criar dashboards de monitoramento no terminal, inspirada no paradigma **Immediate Mode GUI**.



## Características

- 📦 **Header-only** - Basta incluir `TerminalGUI.hpp`
- ⚡ **Performance** - Otimizado para 60fps, zero alocações no loop principal
- 🔒 **Estável** - Projetado para execução 24/7 sem vazamentos de memória
- 🎨 **UTF-8** - Suporte completo a Unicode (bordas e acentos)
- 🧩 **Simples** - API intuitiva estilo IMGUI.
- 🌐 **Plataforma** - Atualmente somente Linux.

## Instalação

Copie `include/TerminalGUI.hpp` para seu projeto e inclua:

```cpp
#include "TerminalGUI.hpp"
```

## Exemplo Rápido

```cpp
#include "TerminalGUI.hpp"
#include <thread>
#include <chrono>

int main() {
    tgui::init(tgui::Charset::Unicode);  // ou ASCII
    
    int frame = 0;
    while (true) {
        tgui::begin_frame();
        
        tgui::text("Dashboard de Monitoramento");
        tgui::new_line();
        
        // Formatação integrada - sem snprintf!
        tgui::textf("Frame: %d", frame++);
        tgui::textf_colored(tgui::Color::Green, "Status: %s", "OK");
        
        // Box com borda
        tgui::box_begin("CPU");
        tgui::textf("Uso: %d%%", 45);
        tgui::progress_bar(45, 100, 20);
        tgui::box_end();
        
        tgui::end_frame();
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    
    tgui::shutdown();
}
```

## API Completa

### Inicialização

| Função | Descrição |
|--------|-----------|
| `init(Charset)` | Inicializa com `Charset::ASCII` ou `Charset::Unicode` |
| `shutdown()` | Finaliza e libera memória |
| `begin_frame()` | Inicia frame (limpa buffer) |
| `end_frame()` | Renderiza no terminal |

### Texto

| Função | Descrição |
|--------|-----------|
| `text(str)` | Texto simples |
| `text(int)` | Número inteiro |
| `text(float, decimals)` | Float com N casas decimais |
| `textf(fmt, ...)` | **Texto formatado (printf-style)** |
| `text_colored(str, color)` | Texto com cor |
| `textf_colored(color, fmt, ...)` | **Formatado + colorido** |

### Layout

| Função | Descrição |
|--------|-----------|
| `same_line()` | Próximo elemento na mesma linha |
| `new_line()` | Força nova linha |
| `separator(width)` | Linha horizontal |
| `indent()` | Aumenta indentação (+2 espaços) |
| `unindent()` | Diminui indentação |
| `set_cursor(x, y)` | Posição absoluta |

### Widgets

| Função | Descrição |
|--------|-----------|
| `box_begin(title)` | Inicia box (tamanho automático) |
| `box_begin(title, {w, h})` | Box com tamanho fixo (trunca com "...") |
| `box_end()` | Fecha box |
| `progress_bar(val, max, width)` | Barra de progresso |

### Cores

| Função | Descrição |
|--------|-----------|
| `set_color(fg, bg)` | Define cor padrão |

```cpp
// Cores disponíveis
tgui::Color::Black, Red, Green, Yellow, Blue, Magenta, Cyan, White
tgui::Color::BrightBlack, BrightRed, BrightGreen, BrightYellow, 
             BrightBlue, BrightMagenta, BrightCyan, BrightWhite
```

### Utilitários

| Função | Descrição |
|--------|-----------|
| `get_terminal_width()` | Largura do terminal |
| `get_terminal_height()` | Altura do terminal |

## Compilar Exemplos

```bash
make all        # Compila tudo
make basic      # Exemplo básico
make dashboard  # Dashboard completo
make perf_test  # Teste de 60fps

./bin/dashboard
```

## Requisitos

- Linux
- C++17
- Nenhuma dependência externa

## Performance

- **Dirty checking**: Só renderiza células que mudaram
- **Buffers pré-alocados**: Zero alocações no loop principal
- **Overlays UTF-8**: Caracteres multibyte sem overhead

## Licença

MIT
