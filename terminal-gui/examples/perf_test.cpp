/**
 * @file perf_test.cpp
 * @brief Teste de performance - 60fps
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
    
    tgui::init(tgui::Charset::ASCII);
    
    const int TARGET_FPS = 60;
    const auto FRAME_TIME = std::chrono::microseconds(1000000 / TARGET_FPS);
    
    int frame = 0;
    double total_frame_time = 0;
    auto start_time = std::chrono::high_resolution_clock::now();
    
    while (running && frame < 3600) {  // 60 segundos
        auto frame_start = std::chrono::high_resolution_clock::now();
        
        tgui::begin_frame();
        
        tgui::text_colored("=== TESTE DE PERFORMANCE - 60 FPS ===", tgui::Color::Cyan);
        tgui::new_line();
        
        char buf[128];
        
        // Estatísticas
        snprintf(buf, sizeof(buf), "Frame: %d", frame);
        tgui::text(buf);
        
        double avg_frame_time = (frame > 0) ? (total_frame_time / frame) : 0;
        snprintf(buf, sizeof(buf), "Tempo medio por frame: %.2f ms", avg_frame_time);
        tgui::text(buf);
        
        double actual_fps = (avg_frame_time > 0) ? (1000.0 / avg_frame_time) : 0;
        snprintf(buf, sizeof(buf), "FPS atual: %.1f", actual_fps);
        tgui::Color fps_color = (actual_fps >= 55) ? tgui::Color::Green : 
                                 (actual_fps >= 30) ? tgui::Color::Yellow : 
                                 tgui::Color::Red;
        tgui::text_colored(buf, fps_color);
        
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::high_resolution_clock::now() - start_time
        ).count();
        snprintf(buf, sizeof(buf), "Tempo total: %ld s", elapsed);
        tgui::text(buf);
        
        tgui::new_line();
        tgui::separator();
        tgui::new_line();
        
        // Conteúdo para testar renderização
        tgui::box_begin("Box 1");
        tgui::text("Conteudo da box 1");
        tgui::progress_bar(frame % 100, 100, 30);
        tgui::box_end();
        
        tgui::box_begin("Box 2", {.width = 50, .height = 6});
        tgui::text("Conteudo da box 2 com tamanho fixo");
        tgui::text("Texto muito longo que sera truncado pela largura fixa da box...");
        tgui::box_end();
        
        for (int i = 0; i < 5; i++) {
            snprintf(buf, sizeof(buf), "Linha de teste %d - Frame %d", i, frame);
            tgui::text(buf);
        }
        
        tgui::new_line();
        tgui::text_colored("Pressione Ctrl+C para sair (ou aguarde 60s)", tgui::Color::BrightBlack);
        
        tgui::end_frame();
        
        auto frame_end = std::chrono::high_resolution_clock::now();
        auto frame_duration = std::chrono::duration_cast<std::chrono::microseconds>(frame_end - frame_start);
        total_frame_time += frame_duration.count() / 1000.0;
        
        // Espera para manter 60 FPS
        if (frame_duration < FRAME_TIME) {
            std::this_thread::sleep_for(FRAME_TIME - frame_duration);
        }
        
        frame++;
    }
    
    tgui::shutdown();
    
    printf("\n=== Resultado do Teste ===\n");
    printf("Total de frames: %d\n", frame);
    printf("Tempo medio por frame: %.2f ms\n", total_frame_time / frame);
    printf("FPS medio: %.1f\n", 1000.0 / (total_frame_time / frame));
    
    return 0;
}
