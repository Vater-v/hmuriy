#pragma once
#include <cstdint>

namespace Config {
    // --- [ NETWORK ] ---
    // Настройки подключения к серверу оставляем
    constexpr const char* SERVER_IP = "127.0.0.1";
    constexpr int SERVER_PORT = 11111;

    // --- [ ADDRESSES / RVAs ] ---
    // Сюда ты впишешь новые адреса, когда найдешь их.
    // Например: constexpr uintptr_t RVA_UPDATE_FUNCTION = 0x......;
}