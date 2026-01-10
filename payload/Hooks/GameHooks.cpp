#include "GameHooks.h"
#include "../Utils/Logger.h"
#include "../Network/Client.h"
#include "../Logic/CommandManager.h"
#include "../Utils/And64InlineHook.hpp" 

// RVA для UnityEngine.EventSystems.EventSystem.Update
#define RVA_EVENTSYSTEM_UPDATE 0x611F6E0

// --- Заглушка исполнителя ---
void ExecuteGameAction(const std::string& action) {
    LOGD("GameHooks: Executing action -> '%s'", action.c_str());
    
    if (action == "test_ping") {
        NetworkClient::Instance().SendHint("Pong! 🏓 (EventSystem)");
    }
}

// --- Хук на EventSystem.Update ---
// Это метод класса, поэтому первый аргумент - this (instance)
void (*orig_EventSystem_Update)(void* instance);

void H_EventSystem_Update(void* instance) {
    // 1. Обязательно вызываем оригинал, чтобы клики в игре работали!
    if (orig_EventSystem_Update) {
        orig_EventSystem_Update(instance);
    }

    // 2. Наша логика
    // EventSystem.Update вызывается 1 раз за кадр движком Unity.
    // Это идеальное место для нашего тикера.
    
    static int tickCounter = 0;
    tickCounter++;
    
    // Для отладки: выводим лог раз в 600 кадров (примерно раз в 10 сек), 
    // просто чтобы убедиться, что хук жив, не засоряя лог.
    if (tickCounter % 600 == 0) {
        LOGD("GameHooks: EventSystem Heartbeat is beating... ❤️");
    }

    // Обрабатываем очередь команд
    std::string actionToRun = CommandManager::Instance().ProcessQueue();
    if (!actionToRun.empty()) {
        ExecuteGameAction(actionToRun);
    }
}

// --- Установка ---
void GameHooks::Install(uintptr_t baseAddress) {
    LOGI("GameHooks: Initialization started...");

    void* targetAddr = (void*)(baseAddress + RVA_EVENTSYSTEM_UPDATE);
    LOGD("GameHooks: Hooking EventSystem.Update at %p (RVA: 0x%X)", targetAddr, RVA_EVENTSYSTEM_UPDATE);

    // Ставим хук
    A64HookFunction(targetAddr, (void*)H_EventSystem_Update, (void**)&orig_EventSystem_Update);
    
    LOGI("GameHooks: EventSystem Hook installed. Waiting for UI loop...");
}