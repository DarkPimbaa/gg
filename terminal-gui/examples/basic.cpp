/**
 * @file basic.cpp
 * @brief Exemplo básico de uso da TerminalGUI
 */

#include "../include/TerminalGUI.hpp"
#include <thread>
#include <chrono>
#include <csignal>

volatile bool running = true;

void signal_handler(int) {
    running = false;
}

int main() {
    signal(SIGINT, signal_handler);
    
    tgui::init(tgui::Charset::Unicode);
    
    int counter = 0;
    
    while (running) {
        tgui::begin_frame();
        
        // Título
        tgui::text_colored("=== TerminalGUI - Exemplo Básico ===", tgui::Color::Cyan);
        tgui::new_line();
        
        // Texto simples
        tgui::text("Texto normal");
        
        // Texto colorido
        tgui::text_colored("Texto verde", tgui::Color::Green);
        tgui::text_colored("Texto vermelho", tgui::Color::Red);
        tgui::text_colored("Texto amarelo", tgui::Color::Yellow);
        
        tgui::new_line();
        
        // same_line demo
        tgui::text("Status:");
        tgui::same_line();
        tgui::text_colored("OK", tgui::Color::Green);
        
        tgui::text("Contador:");
        tgui::same_line();
        
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", counter++);
        tgui::text(buf);
        
        tgui::new_line();
        tgui::separator();
        
        // Indentação
        tgui::text("Lista com indentação:");
        tgui::indent();
        tgui::text("- Item 1");
        tgui::text("- Item 2");
        tgui::indent();
        tgui::text("- Sub-item 2.1");
        tgui::text("- Sub-item 2.2");
        tgui::unindent();
        tgui::text("- Item 3");
        tgui::unindent();
        
        tgui::new_line();
        
        // Barra de progresso
        int progress = counter % 101;
        tgui::text("Progresso:");
        tgui::same_line();
        tgui::progress_bar(progress, 100, 30);
        
        tgui::new_line();
        tgui::text_colored("Pressione Ctrl+C para sair", tgui::Color::BrightBlack);
        
        tgui::end_frame();
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    tgui::shutdown();
    return 0;
}
