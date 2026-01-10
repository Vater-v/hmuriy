#pragma once

#include <string>
#include <deque>
#include <mutex>
#include <chrono>
#include <memory>

// Конфигурация внутри хедера для простоты, либо вынести в Config.hpp
namespace CmdConfig {
    const int RETRY_INTERVAL_MS = 2000;
    const int MAX_RETRIES = 3;
}

struct GameCommand {
    std::string id;             
    bool executedLocally;       
    bool confirmedByServer;     
    int retryCount;             
    std::chrono::steady_clock::time_point lastAttemptTime;
};

class CommandManager {
private:
    std::deque<GameCommand> queue;
    std::mutex mtx;

    // Singleton
    CommandManager() = default;
    ~CommandManager() = default;

public:
    CommandManager(const CommandManager&) = delete;
    void operator=(const CommandManager&) = delete;
    static CommandManager& Instance();

    // Добавить команду в очередь (вызывается из NetworkClient)
    void AddCommand(const std::string& cmdId);

    // Подтвердить выполнение (вызывается из NetworkClient при получении ACK)
    void ConfirmSuccess(const std::string& receivedSuccessMsg);

    void AnalyzeGameResponse(const std::string& jsonResponse);

    // Получить команду для исполнения (вызывается из Main Thread)
    // Возвращает пустую строку, если команд нет или ждать рано
    std::string ProcessQueue();
    
    void Clear();
};