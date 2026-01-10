#include "GameHooks.h"
#include "../Utils/Logger.h"
#include "../Utils/And64InlineHook.hpp"
#include "../Logic/CommandManager.h"
#include "../Network/Client.h"

// UniRx.MainThreadDispatcher.Update
// [Token(Token = "0x600051C")]
// [Address(RVA = "0x5A80A84", Offset = "0x5A7CA84", VA = "0x5A80A84")]
#define RVA_TARGET_FUNC 0x5A80A84

// Указатель на оригинальную функцию Update
void (*orig_TargetFunc)(void* instance);

// --- Наша функция-перехватчик (Detour) ---
void H_TargetFunc(void* instance) {
    
    // 1. Сначала вызываем оригинал, чтобы логика UniRx работала как положено
    if (orig_TargetFunc) {
        orig_TargetFunc(instance);
    }

    // 2. Мы находимся в главном потоке Unity (Main Thread).
    // Здесь безопасно работать с Unity API и обрабатывать игровые команды.
    
    // Запрашиваем у менеджера, есть ли что-то на исполнение
    std::string cmd = CommandManager::Instance().ProcessQueue();

    if (!cmd.empty()) {
        LOGI("[MainThread] Executing command: %s", cmd.c_str());

        // --- Блок выполнения команд ---
        // Здесь можно реализовать switch/if для конкретных действий
        
        // Пример реакции: показываем тост в игре
        NetworkClient::Instance().SendToast("CMD Executed: " + cmd);

        /* Пример реализации логики:
           if (cmd == "give_money") { 
               // ... pointer manipulation ... 
           }
        */

        // Если нужно, отправляем ответ серверу о выполнении (если протокол требует)
        // NetworkClient::Instance().SendRaw("DONE: " + cmd);
    }
}

// --- Установка хука ---
void GameHooks::Install(uintptr_t baseAddress) {
    LOGI("GameHooks: Initialization started...");

    // Вычисляем реальный адрес: Base + RVA
    void* targetAddr = (void*)(baseAddress + RVA_TARGET_FUNC);
    
    LOGD("GameHooks: Hooking MainThreadDispatcher.Update at %p (RVA: 0x%X)", targetAddr, RVA_TARGET_FUNC);

    // Ставим хук
    A64HookFunction(targetAddr, (void*)H_TargetFunc, (void**)&orig_TargetFunc);
    
    LOGI("GameHooks: Hook installed. Waiting for commands in Main Thread...");
}