#pragma once
#include <cstdint>

namespace Config {
    // --- [ NETWORK ] ---
    constexpr const char* SERVER_IP = "127.0.0.1";
    constexpr int SERVER_PORT = 11111;

    // --- [ ADDRESSES / RVAs ] ---
    // JSON парсер (для перехвата логов)
    constexpr uintptr_t RVA_DESERIALIZE_OBJ = 0x5344770; 
    
    // Cup::FixedUpdate - Тикер (~50 раз в сек)
    constexpr uintptr_t RVA_CUP_FIXED_UPDATE = 0x2E723A0; 

    // Cup::ThrowDice - Метод броска
    constexpr uintptr_t RVA_CUP_THROW_DICE  = 0x2E72164;

    // --- [ OFFSETS ] ---
    constexpr uintptr_t OFFSET_CUP_INTERACTIVE  = 0x98; 
}