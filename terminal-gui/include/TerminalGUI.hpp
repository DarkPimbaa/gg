/**
 * @file TerminalGUI.hpp
 * @brief Biblioteca header-only para criar dashboards de monitoramento no terminal.
 * 
 * Inspirada no paradigma Immediate Mode GUI (IMGUI), permite criar interfaces
 * estáticas no terminal sem scrolling, com foco em performance para execução 24/7.
 * 
 * @example
 * ```cpp
 * #include "TerminalGUI.hpp"
 * 
 * int main() {
 *     tgui::init(tgui::Charset::Unicode);
 *     
 *     while (running) {
 *         tgui::begin_frame();
 *         tgui::text("Hello World");
 *         tgui::end_frame();
 *     }
 *     
 *     tgui::shutdown();
 * }
 * ```
 * 
 * @author TerminalGUI
 * @version 1.0.1
 */

#pragma once

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdarg>
#include <unistd.h>
#include <sys/ioctl.h>
#include <termios.h>

namespace tgui {

// ============================================================================
// ENUMS E CONSTANTES
// ============================================================================

/**
 * @brief Conjunto de caracteres para desenho de bordas.
 * 
 * - ASCII: Usa caracteres simples (+, -, |) compatíveis com qualquer terminal
 * - Unicode: Usa caracteres de box drawing (╔, ═, ║) para visual mais elegante
 */
enum class Charset {
    ASCII,   ///< Caracteres ASCII simples: +, -, |
    Unicode  ///< Caracteres Unicode box drawing: ╔, ═, ║
};

/**
 * @brief Cores ANSI padrão (16 cores).
 * 
 * As cores Bright* são versões mais claras das cores básicas.
 */
enum class Color : uint8_t {
    Black = 0,
    Red,
    Green,
    Yellow,
    Blue,
    Magenta,
    Cyan,
    White,
    BrightBlack,
    BrightRed,
    BrightGreen,
    BrightYellow,
    BrightBlue,
    BrightMagenta,
    BrightCyan,
    BrightWhite,
    Default = 255  ///< Usa a cor padrão do terminal
};

/**
 * @brief Estilos de texto.
 */
enum class Style : uint8_t {
    None      = 0,
    Bold      = 1 << 0,
    Dim       = 1 << 1,
    Italic    = 1 << 2,
    Underline = 1 << 3
};

// ============================================================================
// ESTRUTURAS INTERNAS
// ============================================================================

namespace internal {

/**
 * @brief Representa uma célula do buffer de tela.
 * 
 * Usa bit fields para minimizar uso de memória.
 */
struct Cell {
    char ch = ' ';
    uint8_t fg : 4;
    uint8_t bg : 4;
    uint8_t style : 4;
    uint8_t has_utf8 : 1;  // Flag: célula tem overlay UTF-8
    
    Cell() : ch(' '), fg(7), bg(0), style(0), has_utf8(0) {}
    
    bool operator==(const Cell& other) const {
        return ch == other.ch && fg == other.fg && bg == other.bg && 
               style == other.style && has_utf8 == other.has_utf8;
    }
    
    bool operator!=(const Cell& other) const {
        return !(*this == other);
    }
};

/**
 * @brief Overlay UTF-8 para caracteres/strings multibyte.
 */
struct Utf8Overlay {
    int x;
    int y;
    int display_width;  // Largura de exibição (colunas ocupadas)
    char str[256];      // String UTF-8 (até ~64 caracteres multibyte)
    uint8_t fg;
    uint8_t bg;
};

/**
 * @brief Retângulo para definir áreas de boxes.
 */
struct Rect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

/**
 * @brief Buffer de tela com double buffering para evitar flicker.
 * 
 * Pré-aloca toda a memória no init() e não faz alocações durante o loop.
 */
class ScreenBuffer {
public:
    Cell* buffer = nullptr;
    Cell* prev_buffer = nullptr;
    int width = 0;
    int height = 0;
    
    void init(int w, int h) {
        width = w;
        height = h;
        buffer = new Cell[w * h];
        prev_buffer = new Cell[w * h];
        clear();
        for (int i = 0; i < w * h; i++) {
            prev_buffer[i].ch = '\0';
        }
    }
    
    void clear() {
        for (int i = 0; i < width * height; i++) {
            buffer[i] = Cell();
        }
    }
    
    void set(int x, int y, const Cell& cell) {
        if (x >= 0 && x < width && y >= 0 && y < height) {
            buffer[y * width + x] = cell;
        }
    }
    
    Cell& get(int x, int y) {
        static Cell dummy;
        if (x >= 0 && x < width && y >= 0 && y < height) {
            return buffer[y * width + x];
        }
        return dummy;
    }
    
    void shutdown() {
        delete[] buffer;
        delete[] prev_buffer;
        buffer = nullptr;
        prev_buffer = nullptr;
    }
};

/**
 * @brief Opções para configuração de boxes.
 */
struct BoxOptions {
    int width = 0;   ///< Largura fixa (0 = automático)
    int height = 0;  ///< Altura fixa (0 = automático)
};

/**
 * @brief Estado do layout para posicionamento de elementos.
 */
struct LayoutState {
    int cursor_x = 0;
    int cursor_y = 0;
    bool same_line_requested = false;
    int indent_level = 0;
    int last_element_width = 0;
    
    // Stack fixa para boxes aninhados (evita alocações)
    static constexpr int MAX_BOX_DEPTH = 8;
    Rect box_stack[MAX_BOX_DEPTH];
    BoxOptions box_options[MAX_BOX_DEPTH];
    int box_start_y[MAX_BOX_DEPTH];
    int box_depth = 0;
    
    void reset() {
        cursor_x = 0;
        cursor_y = 0;
        same_line_requested = false;
        indent_level = 0;
        last_element_width = 0;
        box_depth = 0;
    }
};

/**
 * @brief Contexto global da GUI (singleton).
 */
struct Context {
    ScreenBuffer screen;
    LayoutState layout;
    Charset charset = Charset::Unicode;
    bool initialized = false;
    
    int terminal_width = 80;
    int terminal_height = 24;
    
    // Buffer de saída pré-alocado para evitar alocações no render
    char* output_buffer = nullptr;
    size_t output_capacity = 0;
    size_t output_pos = 0;
    
    // Estado original do terminal para restaurar no shutdown
    struct termios original_termios;
    
    // Caracteres de borda (strings para suportar UTF-8)
    const char* border_h = "-";
    const char* border_v = "|";
    const char* border_tl = "+";
    const char* border_tr = "+";
    const char* border_bl = "+";
    const char* border_br = "+";
    
    // Overlays UTF-8 para bordas (pré-alocado)
    static constexpr int MAX_UTF8_OVERLAYS = 4096;
    Utf8Overlay utf8_overlays[MAX_UTF8_OVERLAYS];
    int utf8_overlay_count = 0;
    
    // Cores atuais
    Color current_fg = Color::White;
    Color current_bg = Color::Black;
};

inline Context& ctx() {
    static Context c;
    return c;
}

// ============================================================================
// CÓDIGOS ANSI
// ============================================================================

namespace ansi {
    constexpr const char* CLEAR_SCREEN = "\033[2J";
    constexpr const char* CURSOR_HOME = "\033[H";
    constexpr const char* CURSOR_HIDE = "\033[?25l";
    constexpr const char* CURSOR_SHOW = "\033[?25h";
    constexpr const char* RESET = "\033[0m";
    
    inline const char* fg_color(Color c) {
        static const char* codes[] = {
            "\033[30m", "\033[31m", "\033[32m", "\033[33m",
            "\033[34m", "\033[35m", "\033[36m", "\033[37m",
            "\033[90m", "\033[91m", "\033[92m", "\033[93m",
            "\033[94m", "\033[95m", "\033[96m", "\033[97m"
        };
        if (static_cast<uint8_t>(c) < 16) {
            return codes[static_cast<uint8_t>(c)];
        }
        return "\033[39m";
    }
    
    inline const char* bg_color(Color c) {
        static const char* codes[] = {
            "\033[40m", "\033[41m", "\033[42m", "\033[43m",
            "\033[44m", "\033[45m", "\033[46m", "\033[47m",
            "\033[100m", "\033[101m", "\033[102m", "\033[103m",
            "\033[104m", "\033[105m", "\033[106m", "\033[107m"
        };
        if (static_cast<uint8_t>(c) < 16) {
            return codes[static_cast<uint8_t>(c)];
        }
        return "\033[49m";
    }
}

// ============================================================================
// FUNÇÕES INTERNAS
// ============================================================================

inline void update_terminal_size() {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        ctx().terminal_width = w.ws_col;
        ctx().terminal_height = w.ws_row;
    }
}

inline void output_append(const char* str) {
    size_t len = strlen(str);
    if (ctx().output_pos + len < ctx().output_capacity) {
        memcpy(ctx().output_buffer + ctx().output_pos, str, len);
        ctx().output_pos += len;
    }
}

inline void output_append_n(const char* str, size_t len) {
    if (ctx().output_pos + len < ctx().output_capacity) {
        memcpy(ctx().output_buffer + ctx().output_pos, str, len);
        ctx().output_pos += len;
    }
}

inline void output_append_char(char c) {
    if (ctx().output_pos + 1 < ctx().output_capacity) {
        ctx().output_buffer[ctx().output_pos++] = c;
    }
}

inline void output_flush() {
    if (ctx().output_pos > 0) {
        write(STDOUT_FILENO, ctx().output_buffer, ctx().output_pos);
        ctx().output_pos = 0;
    }
}

inline void write_cell_at(int x, int y, const Cell& cell) {
    char pos_buf[32];
    snprintf(pos_buf, sizeof(pos_buf), "\033[%d;%dH", y + 1, x + 1);
    output_append(pos_buf);
    output_append(ansi::fg_color(static_cast<Color>(cell.fg)));
    output_append(ansi::bg_color(static_cast<Color>(cell.bg)));
    output_append_char(cell.ch);
}

inline void write_utf8_at(int x, int y, const char* str, Color fg, Color bg) {
    char pos_buf[32];
    snprintf(pos_buf, sizeof(pos_buf), "\033[%d;%dH", y + 1, x + 1);
    output_append(pos_buf);
    output_append(ansi::fg_color(fg));
    output_append(ansi::bg_color(bg));
    output_append(str);
}

inline void add_utf8_overlay(int x, int y, const char* str, Color fg, Color bg, int display_width = 1) {
    if (ctx().utf8_overlay_count >= Context::MAX_UTF8_OVERLAYS) return;
    if (x < 0 || x >= ctx().terminal_width || y < 0 || y >= ctx().terminal_height) return;
    
    auto& overlay = ctx().utf8_overlays[ctx().utf8_overlay_count++];
    overlay.x = x;
    overlay.y = y;
    overlay.display_width = display_width;
    strncpy(overlay.str, str, sizeof(overlay.str) - 1);
    overlay.str[sizeof(overlay.str) - 1] = '\0';
    overlay.fg = static_cast<uint8_t>(fg) & 0xF;
    overlay.bg = static_cast<uint8_t>(bg) & 0xF;
    
    // Marca a célula como tendo overlay
    ctx().screen.get(x, y).has_utf8 = 1;
}

inline void put_char(int x, int y, char ch, Color fg = Color::White, Color bg = Color::Black) {
    Cell cell;
    cell.ch = ch;
    cell.fg = static_cast<uint8_t>(fg) & 0xF;
    cell.bg = static_cast<uint8_t>(bg) & 0xF;
    ctx().screen.set(x, y, cell);
}

inline void put_border_char(int x, int y, const char* str, Color fg = Color::White, Color bg = Color::Black) {
    // Para ASCII, usa put_char normal
    if (ctx().charset == Charset::ASCII) {
        put_char(x, y, str[0], fg, bg);
    } else {
        // Para Unicode, usa overlay
        add_utf8_overlay(x, y, str, fg, bg);
        // Marca a célula com um espaço (será sobrescrita pelo overlay)
        Cell cell;
        cell.ch = ' ';
        cell.fg = static_cast<uint8_t>(fg) & 0xF;
        cell.bg = static_cast<uint8_t>(bg) & 0xF;
        cell.has_utf8 = 1;
        ctx().screen.set(x, y, cell);
    }
}

// Verifica se um byte é início de caractere UTF-8 multibyte
inline bool is_utf8_start(unsigned char c) {
    return (c & 0x80) != 0;  // Qualquer byte >= 128 indica UTF-8 multibyte
}

// Calcula quantos bytes um caractere UTF-8 ocupa
inline int utf8_char_bytes(unsigned char c) {
    if ((c & 0x80) == 0) return 1;       // ASCII
    if ((c & 0xE0) == 0xC0) return 2;    // 2-byte UTF-8
    if ((c & 0xF0) == 0xE0) return 3;    // 3-byte UTF-8
    if ((c & 0xF8) == 0xF0) return 4;    // 4-byte UTF-8
    return 1;  // Fallback
}

// Conta caracteres visíveis em uma string UTF-8 (não bytes)
inline int utf8_strlen(const char* str) {
    int count = 0;
    while (*str) {
        if ((*str & 0xC0) != 0x80) count++;  // Não é byte de continuação
        str++;
    }
    return count;
}

// Verifica se string contém caracteres UTF-8 multibyte
inline bool has_utf8(const char* str) {
    while (*str) {
        if (static_cast<unsigned char>(*str) >= 128) return true;
        str++;
    }
    return false;
}

inline void put_string(int x, int y, const char* str, int max_len = -1, Color fg = Color::White, Color bg = Color::Black) {
    // Se contém UTF-8, usa overlay para a string inteira
    if (has_utf8(str)) {
        int vis_len = utf8_strlen(str);
        
        // Trunca se necessário (copia para buffer temporário)
        if (max_len > 0 && vis_len > max_len) {
            // Cria string truncada com "..."
            char truncated[256];
            int byte_pos = 0;
            int char_count = 0;
            int target_chars = max_len - 3;
            
            while (str[byte_pos] && char_count < target_chars && byte_pos < 250) {
                int bytes = utf8_char_bytes(static_cast<unsigned char>(str[byte_pos]));
                for (int b = 0; b < bytes && str[byte_pos]; b++) {
                    truncated[byte_pos] = str[byte_pos];
                    byte_pos++;
                }
                char_count++;
                byte_pos -= bytes;  // Volta para ajustar
                byte_pos += bytes;
            }
            truncated[byte_pos++] = '.';
            truncated[byte_pos++] = '.';
            truncated[byte_pos++] = '.';
            truncated[byte_pos] = '\0';
            
            add_utf8_overlay(x, y, truncated, fg, bg, max_len);
            vis_len = max_len;
        } else {
            add_utf8_overlay(x, y, str, fg, bg, vis_len);
        }
        
        // Marca células como tendo overlay
        for (int i = 0; i < vis_len && x + i < ctx().terminal_width; i++) {
            Cell cell;
            cell.ch = ' ';
            cell.fg = static_cast<uint8_t>(fg) & 0xF;
            cell.bg = static_cast<uint8_t>(bg) & 0xF;
            cell.has_utf8 = 1;
            ctx().screen.set(x + i, y, cell);
        }
    } else {
        // ASCII puro - usa células normais
        int len = strlen(str);
        bool truncate = (max_len > 0 && len > max_len);
        int display_len = truncate ? max_len : len;
        
        for (int i = 0; i < display_len && x + i < ctx().terminal_width; i++) {
            if (truncate && i >= max_len - 3) {
                put_char(x + i, y, '.', fg, bg);
            } else {
                put_char(x + i, y, str[i], fg, bg);
            }
        }
    }
}

} // namespace internal

// ============================================================================
// API PÚBLICA
// ============================================================================

/**
 * @brief Inicializa a biblioteca TerminalGUI.
 * 
 * Deve ser chamada antes de qualquer outra função. Pré-aloca todos os buffers
 * necessários para evitar alocações durante o loop principal.
 * 
 * @param cs Conjunto de caracteres para bordas (ASCII ou Unicode)
 * 
 * @example
 * ```cpp
 * tgui::init(tgui::Charset::Unicode);
 * // ... loop principal ...
 * tgui::shutdown();
 * ```
 */
inline void init(Charset cs = Charset::Unicode) {
    if (internal::ctx().initialized) return;
    
    internal::ctx().charset = cs;
    
    // Configura caracteres de borda
    if (cs == Charset::Unicode) {
        internal::ctx().border_h = "═";
        internal::ctx().border_v = "║";
        internal::ctx().border_tl = "╔";
        internal::ctx().border_tr = "╗";
        internal::ctx().border_bl = "╚";
        internal::ctx().border_br = "╝";
    } else {
        internal::ctx().border_h = "-";
        internal::ctx().border_v = "|";
        internal::ctx().border_tl = "+";
        internal::ctx().border_tr = "+";
        internal::ctx().border_bl = "+";
        internal::ctx().border_br = "+";
    }
    
    // Detecta tamanho do terminal
    internal::update_terminal_size();
    
    // Aloca buffers
    internal::ctx().screen.init(internal::ctx().terminal_width, internal::ctx().terminal_height);
    internal::ctx().output_capacity = internal::ctx().terminal_width * internal::ctx().terminal_height * 40;
    internal::ctx().output_buffer = new char[internal::ctx().output_capacity];
    
    // Salva estado do terminal
    tcgetattr(STDIN_FILENO, &internal::ctx().original_termios);
    
    // Esconde cursor e limpa tela
    write(STDOUT_FILENO, internal::ansi::CURSOR_HIDE, strlen(internal::ansi::CURSOR_HIDE));
    write(STDOUT_FILENO, internal::ansi::CLEAR_SCREEN, strlen(internal::ansi::CLEAR_SCREEN));
    write(STDOUT_FILENO, internal::ansi::CURSOR_HOME, strlen(internal::ansi::CURSOR_HOME));
    
    internal::ctx().initialized = true;
}

/**
 * @brief Finaliza a biblioteca e restaura o terminal ao estado original.
 */
inline void shutdown() {
    if (!internal::ctx().initialized) return;
    
    write(STDOUT_FILENO, internal::ansi::CURSOR_SHOW, strlen(internal::ansi::CURSOR_SHOW));
    write(STDOUT_FILENO, internal::ansi::RESET, strlen(internal::ansi::RESET));
    write(STDOUT_FILENO, internal::ansi::CLEAR_SCREEN, strlen(internal::ansi::CLEAR_SCREEN));
    write(STDOUT_FILENO, internal::ansi::CURSOR_HOME, strlen(internal::ansi::CURSOR_HOME));
    
    tcsetattr(STDIN_FILENO, TCSANOW, &internal::ctx().original_termios);
    
    internal::ctx().screen.shutdown();
    delete[] internal::ctx().output_buffer;
    internal::ctx().output_buffer = nullptr;
    
    internal::ctx().initialized = false;
}

/**
 * @brief Inicia um novo frame de renderização.
 */
inline void begin_frame() {
    int old_w = internal::ctx().terminal_width;
    int old_h = internal::ctx().terminal_height;
    internal::update_terminal_size();
    
    if (old_w != internal::ctx().terminal_width || old_h != internal::ctx().terminal_height) {
        internal::ctx().screen.shutdown();
        internal::ctx().screen.init(internal::ctx().terminal_width, internal::ctx().terminal_height);
    }
    
    internal::ctx().screen.clear();
    internal::ctx().layout.reset();
    internal::ctx().utf8_overlay_count = 0;  // Limpa overlays
}

/**
 * @brief Finaliza o frame e renderiza no terminal.
 */
inline void end_frame() {
    auto& screen = internal::ctx().screen;
    
    // Renderiza células que mudaram (sem UTF-8)
    for (int y = 0; y < screen.height; y++) {
        for (int x = 0; x < screen.width; x++) {
            auto& cur = screen.buffer[y * screen.width + x];
            auto& prev = screen.prev_buffer[y * screen.width + x];
            
            if (cur != prev) {
                if (!cur.has_utf8) {
                    internal::write_cell_at(x, y, cur);
                }
                prev = cur;
            }
        }
    }
    
    // Renderiza overlays UTF-8
    for (int i = 0; i < internal::ctx().utf8_overlay_count; i++) {
        auto& overlay = internal::ctx().utf8_overlays[i];
        internal::write_utf8_at(overlay.x, overlay.y, overlay.str, 
                                static_cast<Color>(overlay.fg), 
                                static_cast<Color>(overlay.bg));
    }
    
    internal::output_append(internal::ansi::RESET);
    internal::output_flush();
}

/**
 * @brief Adiciona texto na posição atual do cursor de layout.
 * 
 * @param str Texto a ser exibido
 */
inline void text(const char* str) {
    auto& layout = internal::ctx().layout;
    
    int x, y;
    int max_width = -1;
    
    if (layout.box_depth > 0) {
        auto& box = layout.box_stack[layout.box_depth - 1];
        auto& opts = layout.box_options[layout.box_depth - 1];
        
        // Dentro da box: base X é box.x + 1, mas cursor_x pode ter offset do same_line
        x = box.x + 1 + layout.cursor_x + (layout.indent_level * 2);
        y = layout.cursor_y;
        
        max_width = (opts.width > 0) ? opts.width - 2 - layout.cursor_x : -1;
    } else {
        // Fora da box: usa cursor_x diretamente
        x = layout.cursor_x + (layout.indent_level * 2);
        y = layout.cursor_y;
    }
    
    internal::put_string(x, y, str, max_width, internal::ctx().current_fg, internal::ctx().current_bg);
    
    // Calcula largura visual (para UTF-8)
    int str_width = internal::has_utf8(str) ? internal::utf8_strlen(str) : static_cast<int>(strlen(str));
    layout.last_element_width = str_width;
    
    if (!layout.same_line_requested) {
        layout.cursor_y++;
        layout.cursor_x = 0;
    } else {
        layout.cursor_x += str_width + 1;
        layout.same_line_requested = false;
    }
}

/**
 * @brief Adiciona texto colorido na posição atual.
 * 
 * @param str Texto a ser exibido
 * @param fg Cor do texto (foreground)
 * @param bg Cor de fundo (background), opcional
 */
inline void text_colored(const char* str, Color fg, Color bg = Color::Black) {
    Color old_fg = internal::ctx().current_fg;
    Color old_bg = internal::ctx().current_bg;
    internal::ctx().current_fg = fg;
    internal::ctx().current_bg = bg;
    text(str);
    internal::ctx().current_fg = old_fg;
    internal::ctx().current_bg = old_bg;
}

/**
 * @brief Adiciona texto formatado (estilo printf) na posição atual.
 * 
 * Usa formatação printf internamente, eliminando a necessidade de snprintf manual.
 * Buffer interno de 512 caracteres.
 * 
 * @param fmt String de formato (igual printf)
 * @param ... Argumentos variádicos
 * 
 * @example
 * ```cpp
 * tgui::textf("Frame: %d | FPS: %.1f", frame, fps);
 * tgui::textf("CPU: %d%%", cpu_usage);
 * ```
 */
inline void textf(const char* fmt, ...) {
    char buffer[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    text(buffer);
}

/**
 * @brief Adiciona texto formatado colorido (estilo printf).
 * 
 * @param fg Cor do texto
 * @param fmt String de formato
 * @param ... Argumentos variádicos
 * 
 * @example
 * ```cpp
 * tgui::textf_colored(tgui::Color::Green, "Status: %s", "OK");
 * tgui::textf_colored(tgui::Color::Red, "Erro: %d", error_code);
 * ```
 */
inline void textf_colored(Color fg, const char* fmt, ...) {
    char buffer[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    text_colored(buffer, fg);
}

/**
 * @brief Adiciona texto formatado colorido com cor de fundo.
 * 
 * @param fg Cor do texto
 * @param bg Cor de fundo
 * @param fmt String de formato
 * @param ... Argumentos variádicos
 */
inline void textf_colored(Color fg, Color bg, const char* fmt, ...) {
    char buffer[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    text_colored(buffer, fg, bg);
}

/**
 * @brief Adiciona um número inteiro como texto.
 * 
 * @param value Valor inteiro
 * 
 * @example
 * ```cpp
 * tgui::text("Contador: ");
 * tgui::same_line();
 * tgui::text(42);
 * ```
 */
inline void text(int value) {
    textf("%d", value);
}

/**
 * @brief Adiciona um número float como texto.
 * 
 * @param value Valor float
 * @param decimals Casas decimais (padrão: 2)
 */
inline void text(float value, int decimals = 2) {
    char fmt[16];
    snprintf(fmt, sizeof(fmt), "%%.%df", decimals);
    char buffer[64];
    snprintf(buffer, sizeof(buffer), fmt, value);
    text(buffer);
}

/**
 * @brief Adiciona um número double como texto.
 */
inline void text(double value, int decimals = 2) {
    text(static_cast<float>(value), decimals);
}

/**
 * @brief Define a cor padrão para os próximos elementos.
 */
inline void set_color(Color fg, Color bg = Color::Black) {
    internal::ctx().current_fg = fg;
    internal::ctx().current_bg = bg;
}

/**
 * @brief Faz o próximo elemento aparecer na mesma linha.
 */
inline void same_line() {
    internal::ctx().layout.same_line_requested = true;
}

/**
 * @brief Força uma nova linha no layout.
 */
inline void new_line() {
    internal::ctx().layout.cursor_y++;
    internal::ctx().layout.cursor_x = 0;
    internal::ctx().layout.same_line_requested = false;
}

/**
 * @brief Adiciona uma linha horizontal separadora.
 * 
 * @param width Largura da linha (0 = largura do terminal)
 */
inline void separator(int width = 0) {
    auto& layout = internal::ctx().layout;
    int y = layout.cursor_y;
    int x;
    int w;
    
    if (layout.box_depth > 0) {
        auto& box = layout.box_stack[layout.box_depth - 1];
        auto& opts = layout.box_options[layout.box_depth - 1];
        x = box.x + 1 + layout.cursor_x + (layout.indent_level * 2);
        int max_w = (opts.width > 0) ? opts.width - 2 - layout.cursor_x : (box.width - 2 - layout.cursor_x);
        w = (width > 0) ? width : max_w;
    } else {
        x = layout.cursor_x + (layout.indent_level * 2);
        w = (width > 0) ? width : internal::ctx().terminal_width - x;
    }
    
    for (int i = 0; i < w && x + i < internal::ctx().terminal_width; i++) {
        internal::put_char(x + i, y, '-', internal::ctx().current_fg, internal::ctx().current_bg);
    }
    
    layout.cursor_y++;
    layout.cursor_x = 0;
}

/**
 * @brief Aumenta o nível de indentação (2 espaços por nível).
 */
inline void indent() {
    internal::ctx().layout.indent_level++;
}

/**
 * @brief Diminui o nível de indentação.
 */
inline void unindent() {
    if (internal::ctx().layout.indent_level > 0) {
        internal::ctx().layout.indent_level--;
    }
}

/**
 * @brief Inicia uma box com borda e título.
 * 
 * @param title Título da box (aparece na borda superior)
 */
inline void box_begin(const char* title) {
    auto& layout = internal::ctx().layout;
    auto& ctx = internal::ctx();
    
    if (layout.box_depth >= internal::LayoutState::MAX_BOX_DEPTH) return;
    
    int x = layout.cursor_x + (layout.indent_level * 2);
    int y = layout.cursor_y;
    
    layout.box_stack[layout.box_depth] = {x, y, 0, 0};
    layout.box_options[layout.box_depth] = {0, 0};
    layout.box_start_y[layout.box_depth] = y;
    layout.box_depth++;
    
    int title_len = strlen(title);
    int box_width = ctx.terminal_width - x;
    
    // Canto superior esquerdo
    internal::put_border_char(x, y, ctx.border_tl, ctx.current_fg, ctx.current_bg);
    
    // Espaço + Título + Espaço
    internal::put_char(x + 1, y, ' ', ctx.current_fg, ctx.current_bg);
    internal::put_string(x + 2, y, title, -1, ctx.current_fg, ctx.current_bg);
    internal::put_char(x + 2 + title_len, y, ' ', ctx.current_fg, ctx.current_bg);
    
    // Linha horizontal após título
    for (int i = x + 3 + title_len; i < x + box_width - 1; i++) {
        internal::put_border_char(i, y, ctx.border_h, ctx.current_fg, ctx.current_bg);
    }
    
    // Canto superior direito
    internal::put_border_char(x + box_width - 1, y, ctx.border_tr, ctx.current_fg, ctx.current_bg);
    
    layout.box_stack[layout.box_depth - 1].width = box_width;
    layout.cursor_y++;
    layout.cursor_x = x + 1;
}

/**
 * @brief Inicia uma box com tamanho fixo.
 * 
 * Se o conteúdo exceder o tamanho, o texto será truncado com "...".
 * 
 * @param title Título da box
 * @param opts Opções de tamanho (width e height)
 */
inline void box_begin(const char* title, internal::BoxOptions opts) {
    auto& layout = internal::ctx().layout;
    auto& ctx = internal::ctx();
    
    if (layout.box_depth >= internal::LayoutState::MAX_BOX_DEPTH) return;
    
    int x = layout.cursor_x + (layout.indent_level * 2);
    int y = layout.cursor_y;
    int box_width = (opts.width > 0) ? opts.width : (ctx.terminal_width - x);
    
    layout.box_stack[layout.box_depth] = {x, y, box_width, opts.height};
    layout.box_options[layout.box_depth] = opts;
    layout.box_start_y[layout.box_depth] = y;
    layout.box_depth++;
    
    int title_len = strlen(title);
    
    // Canto superior esquerdo
    internal::put_border_char(x, y, ctx.border_tl, ctx.current_fg, ctx.current_bg);
    
    internal::put_char(x + 1, y, ' ', ctx.current_fg, ctx.current_bg);
    
    // Trunca título se necessário
    int max_title = box_width - 4;
    if (title_len > max_title) {
        for (int i = 0; i < max_title - 3; i++) {
            internal::put_char(x + 2 + i, y, title[i], ctx.current_fg, ctx.current_bg);
        }
        internal::put_string(x + 2 + max_title - 3, y, "...", -1, ctx.current_fg, ctx.current_bg);
        title_len = max_title;
    } else {
        internal::put_string(x + 2, y, title, -1, ctx.current_fg, ctx.current_bg);
    }
    
    internal::put_char(x + 2 + title_len, y, ' ', ctx.current_fg, ctx.current_bg);
    
    for (int i = x + 3 + title_len; i < x + box_width - 1; i++) {
        internal::put_border_char(i, y, ctx.border_h, ctx.current_fg, ctx.current_bg);
    }
    
    // Canto superior direito
    internal::put_border_char(x + box_width - 1, y, ctx.border_tr, ctx.current_fg, ctx.current_bg);
    
    // Se altura fixa, desenha bordas laterais
    if (opts.height > 0) {
        for (int row = 1; row < opts.height - 1; row++) {
            internal::put_border_char(x, y + row, ctx.border_v, ctx.current_fg, ctx.current_bg);
            internal::put_border_char(x + box_width - 1, y + row, ctx.border_v, ctx.current_fg, ctx.current_bg);
        }
    }
    
    layout.cursor_y++;
    layout.cursor_x = x + 1;
}

/**
 * @brief Finaliza a box atual.
 */
inline void box_end() {
    auto& layout = internal::ctx().layout;
    auto& ctx = internal::ctx();
    
    if (layout.box_depth <= 0) return;
    
    layout.box_depth--;
    auto& box = layout.box_stack[layout.box_depth];
    auto& opts = layout.box_options[layout.box_depth];
    
    int y = layout.cursor_y;
    
    // Se altura fixa, usa a altura definida
    if (opts.height > 0) {
        y = layout.box_start_y[layout.box_depth] + opts.height - 1;
    }
    
    // Desenha bordas laterais para linhas intermediárias (se não foi feito antes)
    if (opts.height == 0) {
        for (int row = layout.box_start_y[layout.box_depth] + 1; row < y; row++) {
            internal::put_border_char(box.x, row, ctx.border_v, ctx.current_fg, ctx.current_bg);
            internal::put_border_char(box.x + box.width - 1, row, ctx.border_v, ctx.current_fg, ctx.current_bg);
        }
    }
    
    // Desenha borda inferior
    internal::put_border_char(box.x, y, ctx.border_bl, ctx.current_fg, ctx.current_bg);
    for (int i = 1; i < box.width - 1; i++) {
        internal::put_border_char(box.x + i, y, ctx.border_h, ctx.current_fg, ctx.current_bg);
    }
    internal::put_border_char(box.x + box.width - 1, y, ctx.border_br, ctx.current_fg, ctx.current_bg);
    
    layout.cursor_y = y + 1;
    layout.cursor_x = 0;
}

/**
 * @brief Desenha uma barra de progresso.
 * 
 * @param value Valor atual
 * @param max Valor máximo
 * @param width Largura da barra em caracteres (padrão: 20)
 */
inline void progress_bar(int value, int max, int width = 20) {
    auto& layout = internal::ctx().layout;
    
    int x, y;
    y = layout.cursor_y;
    
    // Calcula posição X considerando same_line e boxes
    if (layout.box_depth > 0) {
        auto& box = layout.box_stack[layout.box_depth - 1];
        x = box.x + 1 + layout.cursor_x + (layout.indent_level * 2);
    } else {
        x = layout.cursor_x + (layout.indent_level * 2);
    }
    
    // Consome o same_line_requested se estava setado
    bool was_same_line = layout.same_line_requested;
    if (was_same_line) {
        layout.same_line_requested = false;
    }
    
    float percent = (max > 0) ? (static_cast<float>(value) / max) : 0;
    int filled = static_cast<int>(percent * width);
    
    internal::put_char(x, y, '[', internal::ctx().current_fg, internal::ctx().current_bg);
    
    for (int i = 0; i < width; i++) {
        char ch = (i < filled) ? '#' : ' ';
        Color fg = (i < filled) ? Color::Green : Color::White;
        internal::put_char(x + 1 + i, y, ch, fg, internal::ctx().current_bg);
    }
    
    internal::put_char(x + 1 + width, y, ']', internal::ctx().current_fg, internal::ctx().current_bg);
    
    // Porcentagem
    char pct_buf[8];
    snprintf(pct_buf, sizeof(pct_buf), " %d%%", static_cast<int>(percent * 100));
    internal::put_string(x + 2 + width, y, pct_buf, -1, internal::ctx().current_fg, internal::ctx().current_bg);
    
    layout.last_element_width = width + 7;
    layout.cursor_y++;
    layout.cursor_x = 0;
}

/**
 * @brief Move o cursor de layout para uma posição absoluta.
 * 
 * @param x Coluna (0 = esquerda)
 * @param y Linha (0 = topo)
 */
inline void set_cursor(int x, int y) {
    internal::ctx().layout.cursor_x = x;
    internal::ctx().layout.cursor_y = y;
}

/**
 * @brief Obtém a largura atual do terminal.
 */
inline int get_terminal_width() {
    return internal::ctx().terminal_width;
}

/**
 * @brief Obtém a altura atual do terminal.
 */
inline int get_terminal_height() {
    return internal::ctx().terminal_height;
}

} // namespace tgui
