#pragma once
#include <cstdint>

namespace GameHooks {
    // Инициализация хуков. Вызывается из главного потока после загрузки libil2cpp.so
    void Install(uintptr_t baseAddress);
}