#include "CommandManager.h"
#include "../Utils/Logger.h"

CommandManager& CommandManager::Instance() {
    static CommandManager instance;
    return instance;
}

void CommandManager::AddCommand(const std::string& cmdId) {
    std::lock_guard<std::mutex> lock(mtx);

    // Дедупликация: если такая команда уже висит и не подтверждена, игнорируем дубль
    for (const auto& cmd : queue) {
        if (cmd.id == cmdId && !cmd.confirmedByServer) {
            LOGW("CmdMgr: Duplicate ignored: %s", cmdId.c_str());
            return;
        }
    }

    GameCommand newCmd;
    newCmd.id = cmdId;
    newCmd.executedLocally = false;
    newCmd.confirmedByServer = false;
    newCmd.retryCount = 0;
    newCmd.lastAttemptTime = std::chrono::steady_clock::now();

    queue.push_back(newCmd);
    LOGI("CmdMgr: [+] Enqueued: %s", cmdId.c_str());
}

void CommandManager::AnalyzeGameResponse(const std::string& jsonResponse) {
    std::lock_guard<std::mutex> lock(mtx);

    if (queue.empty()) return;

    GameCommand& current = queue.front();

    // --- ЛОГИКА ВАЛИДАЦИИ ---
    // Здесь нужно определить, какие признаки в JSON говорят об успехе текущей команды.
    
    bool isSuccess = false;

    // ПРИМЕР 1: Универсальный триггер (если игра возвращает "success":true или status:ok)
    if (jsonResponse.find("\"status\":\"ok\"") != std::string::npos || 
        jsonResponse.find("\"success\":true") != std::string::npos) {
        isSuccess = true;
    }

    // ПРИМЕР 2: Зависимость от ID команды.
    // Если мы отправляли "collect_bonus", мы ждем в ответе изменения баланса или "bonus_collected"
    if (current.id.find("collect") != std::string::npos) {
        if (jsonResponse.find("\"currency\":") != std::string::npos) {
            isSuccess = true;
        }
    }
    
    // ПРИМЕР 3: Просто факт получения ЛЮБОГО валидного (не пустого) ответа на входящем потоке
    // Это самый простой вариант: если после отправки команды пришел JSON, считаем, что все ок.
    // if (jsonResponse.length() > 2) isSuccess = true; 

    if (isSuccess) {
        current.confirmedByServer = true;
        LOGI("CmdMgr: [INTERNAL VALIDATION] Confirmed '%s' via Game Traffic.", current.id.c_str());
        queue.pop_front();
    }
}

void CommandManager::ConfirmSuccess(const std::string& receivedSuccessMsg) {
    std::lock_guard<std::mutex> lock(mtx);

    if (queue.empty()) return;

    // Предполагаем, что сервер шлет "ID_success" или просто ID, если протокол позволяет.
    // Здесь логика: если пришло сообщение, содержащее ID текущей команды.
    GameCommand& current = queue.front();
    
    // Простая проверка: если сообщение содержит ID команды (или равно ему)
    // Можно ужесточить проверку: if (receivedSuccessMsg == current.id + "_ACK")
    if (receivedSuccessMsg.find(current.id) != std::string::npos) {
        current.confirmedByServer = true;
        LOGI("CmdMgr: [OK] Confirmed '%s'. Removing from queue.", current.id.c_str());
        queue.pop_front();
    } else {
        LOGD("CmdMgr: Ignored confirmation '%s' (Waiting for '%s')", receivedSuccessMsg.c_str(), current.id.c_str());
    }
}

std::string CommandManager::ProcessQueue() {
    std::lock_guard<std::mutex> lock(mtx);

    if (queue.empty()) return "";

    GameCommand& current = queue.front();
    auto now = std::chrono::steady_clock::now();

    // 1. Первая попытка исполнения
    if (!current.executedLocally) {
        current.executedLocally = true;
        current.lastAttemptTime = now;
        LOGD("CmdMgr: Executing first time: %s", current.id.c_str());
        return current.id;
    }

    // 2. Логика повторов (Retry)
    // Если исполнили, но сервер не подтвердил, и прошло время таймаута
    if (current.executedLocally && !current.confirmedByServer) {
        long elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - current.lastAttemptTime).count();

        if (elapsedMs >= CmdConfig::RETRY_INTERVAL_MS) {
            if (current.retryCount < CmdConfig::MAX_RETRIES) {
                current.retryCount++;
                current.lastAttemptTime = now;
                
                LOGW("CmdMgr: [RETRY %d/%d] %s", current.retryCount, CmdConfig::MAX_RETRIES, current.id.c_str());
                return current.id; // Возвращаем ID, чтобы main thread исполнил снова
            } else {
                LOGE("CmdMgr: [TIMEOUT] Give up on %s", current.id.c_str());
                queue.pop_front(); // Удаляем, так и не дождавшись
                return "";
            }
        }
    }

    return ""; // Ждем, ничего делать не надо
}

void CommandManager::Clear() {
    std::lock_guard<std::mutex> lock(mtx);
    queue.clear();
    LOGW("CmdMgr: Queue flushed.");
}