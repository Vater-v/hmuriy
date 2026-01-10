#include "CommandManager.h"
#include "../Utils/Logger.h"

CommandManager& CommandManager::Instance() {
    static CommandManager instance;
    return instance;
}

void CommandManager::AddCommand(const std::string& payload) {
    std::lock_guard<std::mutex> lock(mtx);

    // Простая защита от дублей, если сервер спамит одной и той же JSON строкой
    for (const auto& cmd : queue) {
        if (cmd.id == payload) {
            LOGW("CmdMgr: Duplicate payload ignored.");
            return;
        }
    }

    GameCommand newCmd;
    newCmd.id = payload; // Здесь теперь лежит JSON, например {"stage":"GamePlay"...}
    newCmd.executedLocally = false;
    newCmd.confirmedByServer = false;
    newCmd.retryCount = 0;
    newCmd.lastAttemptTime = std::chrono::steady_clock::now();

    queue.push_back(newCmd);
    LOGI("CmdMgr: [+] Payload Enqueued. Size: %zu chars", payload.length());
}

void CommandManager::AnalyzeGameResponse(const std::string& jsonResponse) {
    // В текущей логике валидация отключена, полагаемся на сервер
}

void CommandManager::ConfirmSuccess(const std::string& receivedSuccessMsg) {
    // ACK не используется в этом режиме
}

std::string CommandManager::ProcessQueue() {
    std::lock_guard<std::mutex> lock(mtx);

    if (queue.empty()) return "";

    GameCommand& current = queue.front();
    
    // === FIRE & FORGET MODE ===
    // Забираем строку (JSON payload)
    std::string payload = current.id;
    
    // Удаляем из очереди СРАЗУ. Клиент просто исполнитель.
    queue.pop_front();
    
    return payload;
}

void CommandManager::Clear() {
    std::lock_guard<std::mutex> lock(mtx);
    queue.clear();
    LOGW("CmdMgr: Queue flushed.");
}