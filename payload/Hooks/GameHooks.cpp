#include "GameHooks.h"
#include "../Utils/Logger.h"
#include "../Utils/And64InlineHook.hpp" 

// RVA из твоего дампа (бывший RVA_ON_ENABLE в JS)
#define RVA_TARGET_FUNC 0x5F2BA10 

// Указатель на оригинальную функцию
// void OnEnable(void* this) - предполагаемая сигнатура
void (*orig_TargetFunc)(void* instance);

// --- Наша функция-перехватчик (Detour) ---
void H_TargetFunc(void* instance) {
    
    // 1. Сначала вызываем оригинал, чтобы игра не сломалась 
    // (в JS это обычно делается автоматически или в конце, в C++ лучше явно вызвать)
    if (orig_TargetFunc) {
        orig_TargetFunc(instance);
    }

    // 2. Реализация логики из JS: "Переменная чтобы не спамить в лог"
    static bool hasNotified = false;

    if (!hasNotified) {
        // Вывод в Logcat (аналог console.warn)
        LOGW("\n========================================");
        LOGW("[SUCCESS] We are inside Unity Main Thread!");
        LOGW("[Info] Hook triggered by UI Element: %p", instance);
        LOGW("========================================\n");

        // Блокируем дальнейший спам
        hasNotified = true;
    }
}

// --- Установка хука ---
void GameHooks::Install(uintptr_t baseAddress) {
    LOGI("GameHooks: Initialization started...");

    // Вычисляем реальный адрес: Base + RVA
    void* targetAddr = (void*)(baseAddress + RVA_TARGET_FUNC);
    
    LOGD("GameHooks: Hooking Target Function at %p (RVA: 0x%X)", targetAddr, RVA_TARGET_FUNC);

    // Ставим хук с помощью Android-Inline-Hook
    // targetAddr - где хукаем
    // H_TargetFunc - наша функция
    // orig_TargetFunc - куда сохранить адрес оригинальной инструкции (трамплин)
    A64HookFunction(targetAddr, (void*)H_TargetFunc, (void**)&orig_TargetFunc);
    
    LOGI("GameHooks: Hook installed. Waiting for execution in Main Thread...");
}