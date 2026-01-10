#pragma once
#include <cstdint>

namespace Config {
    // --- [ NETWORK ] ---
    constexpr const char* SERVER_IP = "127.0.0.1";
    constexpr int SERVER_PORT = 11111;

    // --- [ ADDRESSES / RVAs ] ---
    // 1. Системные
    constexpr uintptr_t RVA_UPDATE_FUNC = 0x5A80A84;
    constexpr uintptr_t RVA_SERIALIZE   = 0x5375EA0;
    constexpr uintptr_t RVA_DESERIALIZE = 0x5376614;

    // 2. Игровые (Backgammon)
    // RollDicesOnCupThrowController
    constexpr uintptr_t RVA_CUP_CTOR    = 0x30568D4; // Конструктор (чтобы украсть this)
    constexpr uintptr_t RVA_ROLL_METHOD = 0x3056A34; // Метод RollDices (приватный)
}