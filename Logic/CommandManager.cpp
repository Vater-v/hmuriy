#include "CommandManager.h"
#include "../Utils/Logger.h"

CommandManager& CommandManager::Instance() {
    static CommandManager instance;
    return instance;
}

void CommandManager::AddCommand(const std::string& cmdId) {
    std::lock_guard<std::mutex> lock(mtx);

    // Оставляем простую дедупликацию, чтобы не заспамить очередь одной и той же кнопкой
    // Если такая команда уже есть в очереди - не добавляем новую.
    for (const auto& cmd : queue) {
        if (cmd.id == cmdId) {
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
    /* // --- [DISABLED] СТАРАЯ ЛОГИКА ВАЛИДАЦИИ ---
    std::lock_guard<std::mutex> lock(mtx);
    if (queue.empty()) return;

    GameCommand& current = queue.front();
    bool isSuccess = false;

    // Проверяем JSON на признаки успеха...
    if (jsonResponse.find("\"status\":\"ok\"") != std::string::npos || 
        jsonResponse.find("\"success\":true") != std::string::npos) {
        isSuccess = true;
    }

    if (isSuccess) {
        current.confirmedByServer = true;
        LOGI("CmdMgr: [VALIDATION] Confirmed '%s'", current.id.c_str());
        queue.pop_front();
    }
    */
}

void CommandManager::ConfirmSuccess(const std::string& receivedSuccessMsg) {
    /*
    // --- [DISABLED] СТАРАЯ ЛОГИКА ПОДТВЕРЖДЕНИЯ (ACK) ---
    std::lock_guard<std::mutex> lock(mtx);
    if (queue.empty()) return;

    GameCommand& current = queue.front();
    if (receivedSuccessMsg.find(current.id) != std::string::npos) {
        current.confirmedByServer = true;
        queue.pop_front();
    }
    */
}

std::string CommandManager::ProcessQueue() {
    std::lock_guard<std::mutex> lock(mtx);

    if (queue.empty()) return "";

    GameCommand& current = queue.front();
    
    // === FIRE & FORGET MODE (Одна попытка) ===
    
    std::string commandId = current.id;
    
    // Удаляем команду из очереди СРАЗУ, не дожидаясь результата.
    queue.pop_front();
    
    LOGD("CmdMgr: Executing (One-Shot): %s", commandId.c_str());
    return commandId;

    /*
    // --- [DISABLED] СТАРАЯ ЛОГИКА RETRY И ОЖИДАНИЯ ---
    
    auto now = std::chrono::steady_clock::now();

    // 1. Первая попытка
    if (!current.executedLocally) {
        current.executedLocally = true;
        current.lastAttemptTime = now;
        return current.id;
    }

    // 2. Повторы (Retry) по таймауту
    if (current.executedLocally && !current.confirmedByServer) {
        long elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - current.lastAttemptTime).count();

        if (elapsedMs >= CmdConfig::RETRY_INTERVAL_MS) {
            if (current.retryCount < CmdConfig::MAX_RETRIES) {
                current.retryCount++;
                current.lastAttemptTime = now;
                LOGW("CmdMgr: [RETRY %d/%d] %s", current.retryCount, CmdConfig::MAX_RETRIES, current.id.c_str());
                return current.id;
            } else {
                LOGE("CmdMgr: [TIMEOUT] Give up on %s", current.id.c_str());
                queue.pop_front();
                return "";
            }
        }
    }
    return "";
    */
}

void CommandManager::Clear() {
    std::lock_guard<std::mutex> lock(mtx);
    queue.clear();
    LOGW("CmdMgr: Queue flushed.");
}