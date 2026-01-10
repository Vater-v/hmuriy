#include "GameHooks.h"
#include "../Utils/Logger.h"
#include "../Utils/And64InlineHook.hpp"
#include "../Logic/CommandManager.h"
#include "../Network/Client.h"
#include "../Utils/StringUtils.h"
#include "../Game/GameConfig.h"

#include <string>
#include <vector>
#include <codecvt>
#include <locale>

// =============================================================
// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ (Состояние игры)
// =============================================================

// Здесь храним "украденный" указатель на контроллер стаканчика
void* g_CupControllerInstance = nullptr;

// =============================================================
// ОПРЕДЕЛЕНИЕ ТИПОВ ФУНКЦИЙ
// =============================================================

// --- Стандартные Unity ---
void (*orig_Update)(void* instance);
void* (*orig_SerializeObject)(void* value);
void* (*orig_DeserializeObject)(void* str, void* type, void* settings);

// --- Игровые (Backgammon) ---

// Конструктор: public RollDicesOnCupThrowController(Board board, RollDiceCommand cmd)
// void* instance - это "this"
typedef void (*CupController_Ctor_t)(void* instance, void* board, void* cmd);
CupController_Ctor_t orig_CupController_Ctor = nullptr;

// Метод броска: private void RollDices()
typedef void (*CupController_Roll_t)(void* instance);
CupController_Roll_t func_CupController_Roll = nullptr; // Не хукаем, просто сохраняем адрес

// =============================================================
// ВСПОМОГАТЕЛЬНЫЕ СТРУКТУРЫ И ФУНКЦИИ
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
    int32_t len = il2cppStr->length;
    if (len <= 0) return "";
    if (len > 500000) return ""; 

    try {
        std::u16string u16str(il2cppStr->chars, len);
        std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convert;
        return convert.to_bytes(u16str);
    } catch (...) {
        return "";
    }
}

void LogTraffic(const char* prefix, const std::string& msg) {
    const size_t CHUNK_SIZE = 3500;
    size_t length = msg.length();
    if (length == 0) return;

    LOGI("%s [LEN: %zu] >>>", prefix, length);
    for (size_t i = 0; i < length; i += CHUNK_SIZE) {
        std::string chunk = msg.substr(i, CHUNK_SIZE);
        LOGD("%s: %s", prefix, chunk.c_str());
    }
    LOGI("%s <<< END", prefix);
}

// Функция для принудительного броска
void ForceGameRoll() {
    // 1. Проверяем, перехватили ли мы уже контроллер
    if (g_CupControllerInstance == nullptr) {
        LOGE("GameHooks: Cannot roll! CupController instance not found yet. (Wait for match start)");
        NetworkClient::Instance().SendToast("Error: Wait for game start!");
        return;
    }

    // 2. Проверяем, нашли ли мы функцию (должна быть найдена при init)
    if (func_CupController_Roll != nullptr) {
        LOGW("GameHooks: >>> INVOKING RollDices() on instance %p <<<", g_CupControllerInstance);
        
        // Вызываем приватный метод игры!
        // Это эквивалентно тому, что игрок нажал на стаканчик.
        func_CupController_Roll(g_CupControllerInstance);
        
        NetworkClient::Instance().SendToast("🎲 Dice Rolled!");
    } else {
        LOGE("GameHooks: Roll function address is null!");
    }
}

// =============================================================
// ФУНКЦИИ-ПЕРЕХВАТЧИКИ
// =============================================================

// --- Обработка JSON ---
void ProcessJson(const std::string& rawJson, const char* tagPrefix) {
    if (Utils::IsSpamOrIgnored(rawJson)) return;

    std::string minifiedJson = Utils::SmartMinify(rawJson);
    if (Utils::IsSpamOrIgnored(minifiedJson)) return;

    LogTraffic(tagPrefix, minifiedJson);

    // Если это ВХОДЯЩИЙ трафик, отдаем менеджеру (сейчас там отключена валидация, но пусть будет)
    if (strcmp(tagPrefix, "IN") == 0) {
        CommandManager::Instance().AnalyzeGameResponse(minifiedJson);
    }

    if (NetworkClient::Instance().IsConnected()) {
        std::string netMsg = std::string(tagPrefix) + ": " + minifiedJson;
        NetworkClient::Instance().SendRaw(netMsg);
    }
}

// --- 1. Хук Update ---
void H_Update(void* instance) {
    if (orig_Update) orig_Update(instance);

    // Спрашиваем у менеджера, есть ли команда на выполнение
    std::string cmd = CommandManager::Instance().ProcessQueue();
    if (!cmd.empty()) {
        LOGI("[MainThread] Executing command: %s", cmd.c_str());
        
        // === ОБРАБОТКА КОМАНД ===
        if (cmd == "roll_dice") {
            ForceGameRoll();
        }
        else {
            // Для других команд просто шлем тост, пока не реализованы
            NetworkClient::Instance().SendToast("CMD: " + cmd);
        }
        // ========================
    }
}

// --- 2. Хук на конструктор контроллера стаканчика ---
// Этот хук нужен только для того, чтобы украсть "this" (g_CupControllerInstance)
void H_CupController_Ctor(void* instance, void* board, void* cmd) {
    // Сохраняем указатель на созданный объект
    g_CupControllerInstance = instance;
    LOGI("GameHooks: Captured CupController instance: %p", instance);

    // Обязательно вызываем оригинал!
    if (orig_CupController_Ctor) {
        orig_CupController_Ctor(instance, board, cmd);
    }
}

// --- 3. Хук SerializeObject (OUT) ---
void* H_SerializeObject(void* value) {
    void* resultString = orig_SerializeObject(value);
    if (resultString) {
        std::string jsonStr = ReadIl2CppString(resultString);
        ProcessJson(jsonStr, "OUT");
    }
    return resultString;
}

// --- 4. Хук DeserializeObject (IN) ---
void* H_DeserializeObject(void* str, void* type, void* settings) {
    if (str) {
        std::string jsonStr = ReadIl2CppString(str);
        ProcessJson(jsonStr, "IN");
    }
    return orig_DeserializeObject(str, type, settings);
}

// =============================================================
// УСТАНОВКА
// =============================================================

void GameHooks::Install(uintptr_t baseAddress) {
    LOGI("GameHooks: Initialization started...");

    // 1. Системные хуки (Сеть и Update)
    A64HookFunction((void*)(baseAddress + Config::RVA_UPDATE_FUNC), (void*)H_Update, (void**)&orig_Update);
    A64HookFunction((void*)(baseAddress + Config::RVA_SERIALIZE), (void*)H_SerializeObject, (void**)&orig_SerializeObject);
    A64HookFunction((void*)(baseAddress + Config::RVA_DESERIALIZE), (void*)H_DeserializeObject, (void**)&orig_DeserializeObject);

    // 2. Хук на конструктор контроллера (Ловим this)
    // Используем адрес из конфига
    A64HookFunction((void*)(baseAddress + Config::RVA_CUP_CTOR), (void*)H_CupController_Ctor, (void**)&orig_CupController_Ctor);

    // 3. Получаем адрес функции броска (без хука)
    // Мы будем вызывать её сами
    func_CupController_Roll = (CupController_Roll_t)(baseAddress + Config::RVA_ROLL_METHOD);

    LOGI("GameHooks: All hooks installed. Ready to roll!");
}