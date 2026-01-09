#include <android/log.h>
#include <pthread.h>
#include <unistd.h>
#include "Utils/Logger.h" 
#include "Network/Client.h"
#include "Utils/MemoryUtils.h"
#include "Hooks/GameHooks.h" // Подключаем наши хуки

// --- Поток инициализации ---
void* init_thread(void*) {
    LOGD(">>> ValeraHmuriy: Init Thread Started <<<");
    
    // 1. Ждем библиотеку
    uintptr_t il2cpp_base = 0;
    while (il2cpp_base == 0) {
        il2cpp_base = get_lib_addr("libil2cpp.so");
        if (il2cpp_base == 0) usleep(100000);
    }

    LOGD(">>> libil2cpp.so found: %p <<<", (void*)il2cpp_base);
    
    // 2. Ставим хуки (Вся магия теперь там)
    GameHooks::Install(il2cpp_base);

    while (true) {
    std::string action = CommandManager::Instance().ProcessQueue();
    if (!action.empty()) {
        ExecuteGameAction(action); // Временно вызываем напрямую, так как GameHooks::Install пуст
    }
    usleep(50000); // 20 раз в секунду
}

    // 3. Сообщаем серверу
    NetworkClient::Instance().SendToast("Hmuriy injected successfully 💉");
    
    // Поток выполнил задачу и завершается. 
    // Дальше работает Unity и наши хуки внутри него.
    return NULL;
}

// --- Точка входа ---
extern "C" void __attribute__((constructor)) hmuriy_entry() {
    // 1. Запускаем сеть
    NetworkClient::Instance().Start();

    // 2. Запускаем инициализацию
    pthread_t pt_init;
    pthread_create(&pt_init, NULL, init_thread, NULL);
}