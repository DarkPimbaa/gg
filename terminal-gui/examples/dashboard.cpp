/**
 * @file dashboard.cpp
 * @brief Dashboard de monitoramento com boxes e layout complexo
 */

#include "../include/TerminalGUI.hpp"
#include <thread>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <ctime>

volatile bool running = true;

void signal_handler(int) {
    running = false;
}

// Simula valores de monitoramento
struct SystemStats {
    int cpu_usage = 0;
    int ram_usage = 0;
    int disk_usage = 0;
    int network_in = 0;
    int network_out = 0;
    int temperature = 0;
    
    void update() {
        cpu_usage = 20 + (rand() % 60);
        ram_usage = 40 + (rand() % 30);
        disk_usage = 55 + (rand() % 10);
        network_in = rand() % 1000;
        network_out = rand() % 500;
        temperature = 45 + (rand() % 20);
    }
};

int main() {
    signal(SIGINT, signal_handler);
    srand(time(nullptr));
    
    tgui::init(tgui::Charset::Unicode);
    
    SystemStats stats;
    int frame = 0;
    
    while (running) {
        if (frame % 20 == 0) {
            stats.update();
        }
        
        tgui::begin_frame();
        
        // Header
        tgui::set_color(tgui::Color::Cyan);
        tgui::text("╔══════════════════════════════════════════════════════════════╗");
        tgui::text("║           DASHBOARD DE MONITORAMENTO - TerminalGUI           ║");
        tgui::text("╚══════════════════════════════════════════════════════════════╝");
        tgui::set_color(tgui::Color::White);
        tgui::new_line();
        
        // Box de CPU - usando textf!
        tgui::box_begin("CPU");
        tgui::Color cpu_color = (stats.cpu_usage > 80) ? tgui::Color::Red : 
                                 (stats.cpu_usage > 50) ? tgui::Color::Yellow : 
                                 tgui::Color::Green;
        tgui::textf_colored(cpu_color, "Uso: %d%%", stats.cpu_usage);
        tgui::text("Progresso:");
        tgui::same_line();
        tgui::progress_bar(stats.cpu_usage, 100, 20);
        tgui::box_end();
        
        // Box de Memória - usando textf!
        tgui::box_begin("Memoria RAM");
        tgui::Color ram_color = (stats.ram_usage > 80) ? tgui::Color::Red : 
                                 (stats.ram_usage > 60) ? tgui::Color::Yellow : 
                                 tgui::Color::Green;
        tgui::textf_colored(ram_color, "Uso: %d%% (%.1f GB / 16 GB)", stats.ram_usage, stats.ram_usage * 0.16);
        tgui::text("Progresso:");
        tgui::same_line();
        tgui::progress_bar(stats.ram_usage, 100, 20);
        tgui::box_end();
        
        // Box de Disco - usando textf!
        tgui::box_begin("Disco");
        tgui::textf("Uso: %d%% (%.0f GB / 500 GB)", stats.disk_usage, stats.disk_usage * 5.0);
        tgui::progress_bar(stats.disk_usage, 100, 25);
        tgui::box_end();
        
        // Box de Rede com tamanho fixo - usando textf_colored!
        tgui::box_begin("Rede", {.width = 40, .height = 5});
        tgui::textf_colored(tgui::Color::Green, "IN:  %4d KB/s", stats.network_in);
        tgui::textf_colored(tgui::Color::Magenta, "OUT: %4d KB/s", stats.network_out);
        tgui::box_end();
        
        // Box de Temperatura - usando textf_colored!
        tgui::box_begin("Temperatura", {.width = 40, .height = 4});
        tgui::Color temp_color = (stats.temperature > 70) ? tgui::Color::Red : 
                                  (stats.temperature > 55) ? tgui::Color::Yellow : 
                                  tgui::Color::Cyan;
        tgui::textf_colored(temp_color, "CPU: %d C", stats.temperature);
        tgui::box_end();
        
        // Footer - usando textf!
        tgui::new_line();
        tgui::separator(50);
        
        tgui::textf_colored(tgui::Color::BrightBlack, "Frame: %d | FPS: ~60", frame);
        tgui::text_colored("Pressione Ctrl+C para sair", tgui::Color::BrightBlack);
        
        tgui::end_frame();
        
        frame++;
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    
    tgui::shutdown();
    return 0;
}
