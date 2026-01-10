#include "GameHooks.h"
#include "../Utils/Logger.h"
#include "../Utils/And64InlineHook.hpp"
#include "../Logic/CommandManager.h"
#include "../Network/Client.h"
#include "../Utils/StringUtils.h" // Подключаем минификатор

#include <string>
#include <vector>
#include <codecvt>
#include <locale>

// =============================================================
// НАСТРОЙКИ АДРЕСОВ (RVAs)
// =============================================================

// 1. UniRx.MainThreadDispatcher.Update
#define RVA_UPDATE_FUNC 0x5A80A84

// 2. SerializeObject (Исходящие)
#define RVA_SERIALIZE   0x5375EA0

// 3. DeserializeObject (Входящие)
#define RVA_DESERIALIZE 0x5376614

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
    
    // Предварительная проверка на безумно большие строки (хотя фильтр потом отсечет)
    if (len > 500000) return ""; 

    try {
        std::u16string u16str(il2cppStr->chars, len);
        std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convert;
        return convert.to_bytes(u16str);
    } catch (...) {
        return "";
    }
}

// Логирование больших сообщений (разбивка на чанки для Logcat)
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

// =============================================================
// УКАЗАТЕЛИ НА ОРИГИНАЛЬНЫЕ ФУНКЦИИ
// =============================================================

void (*orig_Update)(void* instance);
void* (*orig_SerializeObject)(void* value);
void* (*orig_DeserializeObject)(void* str, void* type, void* settings);

// =============================================================
// ФУНКЦИИ-ПЕРЕХВАТЧИКИ
// =============================================================

// --- Обработка JSON (общая логика) ---
void ProcessJson(const std::string& rawJson, const char* tagPrefix) {
    // 1. Проверяем на спам и лимиты (40 < len < 200000)
    if (Utils::IsSpamOrIgnored(rawJson)) {
        return; // Игнорируем
    }

    // 2. Минифицируем
    std::string minifiedJson = Utils::SmartMinify(rawJson);
    
    // Повторная проверка длины после минификации (на всякий случай)
    if (Utils::IsSpamOrIgnored(minifiedJson)) {
        return;
    }

    // 3. Выводим в Logcat (чистый минифицированный JSON)
    LogTraffic(tagPrefix, minifiedJson);

    // 4. Отправляем на сервер (только если подключены)
    if (NetworkClient::Instance().IsConnected()) {
        std::string netMsg = std::string(tagPrefix) + ": " + minifiedJson;
        NetworkClient::Instance().SendRaw(netMsg);
    }
}

// --- 1. Хук Update ---
void H_Update(void* instance) {
    if (orig_Update) orig_Update(instance);

    std::string cmd = CommandManager::Instance().ProcessQueue();
    if (!cmd.empty()) {
        LOGI("[MainThread] Executing command: %s", cmd.c_str());
        NetworkClient::Instance().SendToast("CMD: " + cmd);
    }
}

// --- 2. Хук SerializeObject (OUT) ---
void* H_SerializeObject(void* value) {
    void* resultString = orig_SerializeObject(value);

    if (resultString) {
        std::string jsonStr = ReadIl2CppString(resultString);
        ProcessJson(jsonStr, "OUT");
    }

    return resultString;
}

// --- 3. Хук DeserializeObject (IN) ---
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

    void* addrUpdate = (void*)(baseAddress + RVA_UPDATE_FUNC);
    A64HookFunction(addrUpdate, (void*)H_Update, (void**)&orig_Update);

    void* addrSerialize = (void*)(baseAddress + RVA_SERIALIZE);
    A64HookFunction(addrSerialize, (void*)H_SerializeObject, (void**)&orig_SerializeObject);

    void* addrDeserialize = (void*)(baseAddress + RVA_DESERIALIZE);
    A64HookFunction(addrDeserialize, (void*)H_DeserializeObject, (void**)&orig_DeserializeObject);

    LOGI("GameHooks: Hooks installed. Minifier & Filter active.");
}