#include "GameHooks.h"
#include "../Utils/Logger.h"
#include "../Utils/And64InlineHook.hpp"
#include "../Logic/CommandManager.h"
#include "../Network/Client.h"
#include "../Utils/StringUtils.h"
#include "../Game/GameConfig.h"

#include <string>
#include <sstream>
#include <iomanip> // Для std::put_time, std::setw
#include <chrono>  // Для времени
#include <ctime>   // Для gmtime
#include <dlfcn.h> // Для dlsym

// =============================================================
// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
// =============================================================

void* g_CupControllerInstance = nullptr;
void* g_SocketBusInstance = nullptr;    

// =============================================================
// ОПРЕДЕЛЕНИЕ ТИПОВ ФУНКЦИЙ
// =============================================================

// --- Unity ---
void (*orig_Update)(void* instance);
void* (*orig_SerializeObject)(void* value);
void* (*orig_DeserializeObject)(void* str, void* type, void* settings);

// --- Backgammon ---
typedef void (*CupController_Ctor_t)(void* instance, void* board, void* cmd);
CupController_Ctor_t orig_CupController_Ctor = nullptr;

typedef void (*SocketBus_Ctor_t)(void* instance, void* webSocket, void* queue, void* signal, bool log);
SocketBus_Ctor_t orig_SocketBus_Ctor = nullptr;

typedef void* (*WebSocket_SendText_t)(void* instance, void* message);
WebSocket_SendText_t func_SendText = nullptr;

// il2cpp string creation
typedef void* (*il2cpp_string_new_t)(const char* str);
il2cpp_string_new_t func_il2cpp_string_new = nullptr;

// =============================================================
// ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
// =============================================================

struct Il2CppString {
    void* klass;
    void* monitor;
    int32_t length;       
    char16_t chars[0];    
};

std::string ReadIl2CppString(void* ptr) {
    if (!ptr) return "";
    Il2CppString* il2cppStr = (Il2CppString*)ptr;
    if (il2cppStr->length <= 0) return "";
    
    std::string s;
    for(int i=0; i<il2cppStr->length; i++) {
        s += (char)il2cppStr->chars[i];
    }
    return s;
}

void* CreateIl2CppString(const char* str) {
    if (func_il2cpp_string_new) {
        return func_il2cpp_string_new(str);
    }
    return nullptr;
}

// Генерация времени в формате: 2026-01-10T13:43:35.630385Z
std::string GetCurrentTimeISO8601() {
    using namespace std::chrono;
    
    auto now = system_clock::now();
    auto now_c = system_clock::to_time_t(now);
    auto duration = now.time_since_epoch();
    auto micros = duration_cast<microseconds>(duration) % 1000000;

    std::tm tm_buf;
    gmtime_r(&now_c, &tm_buf);

    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", &tm_buf);

    std::stringstream ss;
    ss << buffer << "." << std::setfill('0') << std::setw(6) << micros.count() << "Z";
    
    return ss.str();
}

// =============================================================
// ЛОГИКА ОТПРАВКИ (ОБЕРТКА)
// =============================================================

void SendDirectJson(const char* innerPayload) {
    // 1. Проверки
    if (g_SocketBusInstance == nullptr) {
        LOGE("SendDirectJson: No SocketBus instance! (Wait for game connection)");
        return;
    }
    if (func_SendText == nullptr) {
        LOGE("SendDirectJson: SendText function pointer is null!");
        return;
    }

    // 2. Получаем указатель на WebSocket
    void* webSocketInstance = *(void**)((uintptr_t)g_SocketBusInstance + Config::OFFSET_SOCKETBUS_WEBSOCKET);
    if (webSocketInstance == nullptr) {
        LOGE("SendDirectJson: WebSocket field is null!");
        return;
    }

    // 3. Работаем с ID сообщения (инкремент)
    int* pIdCounter = (int*)((uintptr_t)g_SocketBusInstance + Config::OFFSET_SOCKETBUS_ID);
    *pIdCounter = *pIdCounter + 1;
    int currentId = *pIdCounter;

    // 4. Генерируем время
    std::string timestamp = GetCurrentTimeISO8601();

    // 5. Собираем ПОЛНЫЙ пакет
    // Оборачиваем пришедший innerPayload в структуру StageAction
    std::stringstream ss;
    ss << "{\"id\":" << currentId 
       << ",\"time\":\"" << timestamp << "\""
       << ",\"type\":\"StageAction\"" 
       << ",\"payload\":" << innerPayload << "}"; // Вставляем то, что пришло с сервера

    std::string fullPacket = ss.str();

    // 6. Конвертируем в C# строку
    void* il2cppStr = CreateIl2CppString(fullPacket.c_str());
    if (!il2cppStr) {
        LOGE("SendDirectJson: Failed to create string");
        return;
    }

    // 7. Отправляем
    LOGW("GameHooks: >>> INJECTING PACKET [%d]: %s", currentId, fullPacket.c_str());
    func_SendText(webSocketInstance, il2cppStr);
    
    // Опционально: тост, что пакет ушел
    // NetworkClient::Instance().SendToast("Inject ID: " + std::to_string(currentId));
}

// =============================================================
// ХУКИ
// =============================================================

// Хук конструктора SocketBus: ловим instance
void H_SocketBus_Ctor(void* instance, void* webSocket, void* queue, void* signal, bool log) {
    g_SocketBusInstance = instance;
    LOGI("GameHooks: Captured SocketBus instance: %p", instance);
    if (orig_SocketBus_Ctor) {
        orig_SocketBus_Ctor(instance, webSocket, queue, signal, log);
    }
}

// Главный цикл (Update)
void H_Update(void* instance) {
    if (orig_Update) orig_Update(instance);

    // Забираем "команду" из очереди. 
    // ТЕПЕРЬ ЭТО НЕ КОМАНДА, А ЧИСТЫЙ JSON PAYLOAD (строка)
    std::string jsonPayload = CommandManager::Instance().ProcessQueue();
    
    if (!jsonPayload.empty()) {
        LOGI("[MainThread] Received API Payload from Server: %s", jsonPayload.c_str());
        
        // БЕЗ ЛОГИКИ IF/ELSE.
        // Просто берем то, что пришло, и отправляем в игру, обернув в конверт.
        SendDirectJson(jsonPayload.c_str());
    }
}

// Логгирование исходящего
void* H_SerializeObject(void* value) {
    void* res = orig_SerializeObject(value);
    if (res) {
        std::string s = ReadIl2CppString(res);
        if (!Utils::IsSpamOrIgnored(s)) {
            if (NetworkClient::Instance().IsConnected()) {
                NetworkClient::Instance().SendRaw("OUT: " + Utils::SmartMinify(s));
            }
        }
    }
    return res;
}

// Логгирование входящего
void* H_DeserializeObject(void* str, void* type, void* settings) {
    if (str) {
         std::string s = ReadIl2CppString(str);
         if (!Utils::IsSpamOrIgnored(s)) {
             if (NetworkClient::Instance().IsConnected()) {
                NetworkClient::Instance().SendRaw("IN: " + Utils::SmartMinify(s));
            }
         }
    }
    return orig_DeserializeObject(str, type, settings);
}

// Хук стаканчика (нужен для совместимости или если понадобятся старые методы)
void H_CupController_Ctor(void* instance, void* board, void* cmd) {
    g_CupControllerInstance = instance;
    if (orig_CupController_Ctor) orig_CupController_Ctor(instance, board, cmd);
}

// =============================================================
// УСТАНОВКА
// =============================================================

void GameHooks::Install(uintptr_t baseAddress) {
    LOGI("GameHooks: Initialization started...");

    void* libHandle = dlopen("libil2cpp.so", RTLD_NOW);
    if (libHandle) {
        func_il2cpp_string_new = (il2cpp_string_new_t)dlsym(libHandle, "il2cpp_string_new");
    }

    // Хуки
    A64HookFunction((void*)(baseAddress + Config::RVA_UPDATE_FUNC), (void*)H_Update, (void**)&orig_Update);
    A64HookFunction((void*)(baseAddress + Config::RVA_SERIALIZE), (void*)H_SerializeObject, (void**)&orig_SerializeObject);
    A64HookFunction((void*)(baseAddress + Config::RVA_DESERIALIZE), (void*)H_DeserializeObject, (void**)&orig_DeserializeObject);
    A64HookFunction((void*)(baseAddress + Config::RVA_CUP_CTOR), (void*)H_CupController_Ctor, (void**)&orig_CupController_Ctor);
    A64HookFunction((void*)(baseAddress + Config::RVA_SOCKETBUS_CTOR), (void*)H_SocketBus_Ctor, (void**)&orig_SocketBus_Ctor);

    // Адреса
    func_SendText = (WebSocket_SendText_t)(baseAddress + Config::RVA_WEBSOCKET_SENDTEXT);

    LOGI("GameHooks: Hooks installed. Passive listening mode.");
}