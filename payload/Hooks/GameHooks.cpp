#include "GameHooks.h"
#include "../Game/GameConfig.h"
#include "../Game/Il2Cpp.h"
#include "../Utils/Logger.h"
#include "../Utils/JsonUtils.h" // Твой минификатор
#include "../Network/Client.h"
#include "../Logic/CommandManager.h"
#include "../Utils/And64InlineHook.hpp" // Библиотека хуков

// --- Указатели на оригиналы ---
void (*orig_CupFixedUpdate)(void* instance);
void* (*orig_DeserializeObject)(void* value_str, void* type_obj, void* settings);

// Указатель на функцию игры (без хука, просто для вызова)
void (*Game_ThrowDice)(void* cupInstance); 

// --- Исполнение команд (Теперь безопасно, внутри потока Unity) ---
void ExecuteGameAction(const std::string& action, void* cupInstance) {
    LOGD("GameHooks: Executing action '%s'", action.c_str());
    
    if (action == "roll_dice") {
        if (Game_ThrowDice && cupInstance) {
            Game_ThrowDice(cupInstance);
            NetworkClient::Instance().SendToast("Dice Rolled! 🎲");
        } else {
            LOGE("GameHooks: Cannot roll dice (ptr null)");
        }
    }
    // Добавь сюда другие команды (например, "emote_laugh")
}

// --- ХУК: FixedUpdate (Тикер игры) ---
void H_CupFixedUpdate(void* instance) {
    // 1. Сначала вызываем оригинал, чтобы физика игры отработала
    if (orig_CupFixedUpdate) orig_CupFixedUpdate(instance);

    // 2. Проверяем, жив ли объект
    if (instance) {
        // 3. Спрашиваем у менеджера: "Есть че?"
        // ProcessQueue вернет ID команды, если пришло время её исполнить (с учетом retry)
        std::string actionToRun = CommandManager::Instance().ProcessQueue();
        
        if (!actionToRun.empty()) {
            ExecuteGameAction(actionToRun, instance);
        }
    }
}

// --- ХУК: DeserializeObject (Логи) ---
void* H_DeserializeObject(void* value_str, void* type_obj, void* settings) {
    if (value_str) {
         auto* str = (Il2CppString*)value_str;
         if (str->length >= 40 && str->length < 200000) { 
             std::string jsonContent = Il2CppUtils::Utf16ToUtf8(str->chars, str->length);
             
             // Используем твою логику фильтрации
             bool isSpam = (jsonContent.find("\"verbose_localised_name\"") != std::string::npos) || 
                           (jsonContent.find("\"keys\":{\"APIError") != std::string::npos);

             if (!isSpam) {
                 NetworkClient::Instance().SendRaw(JsonUtils::SmartMinify(jsonContent));
             }
         }
    }
    return orig_DeserializeObject(value_str, type_obj, settings);
}

// --- Установка ---
void GameHooks::Install(uintptr_t baseAddress) {
    // Инициализируем адрес функции броска
    Game_ThrowDice = (void(*)(void*))(baseAddress + Config::RVA_CUP_THROW_DICE);

    // Ставим хуки
    A64HookFunction((void*)(baseAddress + Config::RVA_DESERIALIZE_OBJ), (void*)H_DeserializeObject, (void**)&orig_DeserializeObject);
    A64HookFunction((void*)(baseAddress + Config::RVA_CUP_FIXED_UPDATE), (void*)H_CupFixedUpdate, (void**)&orig_CupFixedUpdate);
    
    LOGI("GameHooks: Installed. Ready to rock.");
}