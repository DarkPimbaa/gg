#pragma once

#include <thread>

#ifdef __linux__
#include <pthread.h>
#include <sched.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

namespace gg::internal {

/**
 * @brief Fixa a thread atual em um núcleo específico da CPU.
 * @param core Número do núcleo (0-based)
 * @return true se sucesso, false se falhou
 */
inline bool pinCurrentThread(int core) noexcept {
#ifdef __linux__
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core, &cpuset);
    return pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset) == 0;
#elif defined(_WIN32)
    DWORD_PTR mask = 1ULL << core;
    return SetThreadAffinityMask(GetCurrentThread(), mask) != 0;
#else
    (void)core;
    return false;  // Não suportado
#endif
}

/**
 * @brief Retorna o número de núcleos disponíveis na CPU.
 */
inline int getCoreCount() noexcept {
    unsigned int count = std::thread::hardware_concurrency();
    return count > 0 ? static_cast<int>(count) : 1;
}

/**
 * @brief Verifica se um número de núcleo é válido.
 */
inline bool isValidCore(int core) noexcept {
    return core >= 0 && core < getCoreCount();
}

} // namespace gg::internal
