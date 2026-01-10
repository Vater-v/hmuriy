#include "Client.h"
#include "../Utils/Logger.h"
#include "../Logic/CommandManager.h"
#include "../Game/GameConfig.h"
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <errno.h>
#include <sstream>

NetworkClient::NetworkClient() : sock(-1), isRunning(true) {}

NetworkClient& NetworkClient::Instance() {
    static NetworkClient instance;
    return instance;
}

void NetworkClient::Start() {
    pthread_create(&netThreadId, NULL, thread_entry, this);
}

void* NetworkClient::thread_entry(void* instance) {
    ((NetworkClient*)instance)->run();
    return NULL;
}

bool NetworkClient::sendData(const std::string& data) {
    if (sock == -1) return false;
    // MSG_NOSIGNAL предотвращает крэш приложения при разрыве трубы
    ssize_t sent = send(sock, data.c_str(), data.length(), MSG_NOSIGNAL);
    if (sent < 0) {
        LOGE("Net: Send failed: %s", strerror(errno));
        return false;
    }
    return true;
}

void NetworkClient::enqueueMessage(std::string msg) {
    if (msg.empty()) return;
    if (msg.back() != '\n') msg += "\n";

    std::lock_guard<std::mutex> lock(queueMutex);
    if (sendQueue.size() > 100) sendQueue.pop(); // Защита от переполнения
    sendQueue.push(msg);
}

void NetworkClient::SendToast(const std::string& text) { enqueueMessage("TOAST: " + text); }
void NetworkClient::SendHint(const std::string& text) { enqueueMessage("HINT: " + text); }
void NetworkClient::SendRaw(const std::string& text) { enqueueMessage(text); }

// --- Логика обработки входящих пакетов ---

void NetworkClient::handlePacket(const std::string& packet) {
    std::string cleanPacket = packet;
    
    // Чистим мусор (\r), который часто прилетает из Python/Telnet
    if (!cleanPacket.empty() && cleanPacket.back() == '\r') {
        cleanPacket.pop_back();
    }
    if (cleanPacket.empty()) return;

    LOGD("RX: %s", cleanPacket.c_str());

    // 1. Проверка на подтверждение успешного выполнения (_success)
    // Сервер прислал, например: "API: move_0_1_success" или просто "roll_dice_success"
    if (cleanPacket.find("_success") != std::string::npos) {
        CommandManager::Instance().ConfirmSuccess(cleanPacket);
        return; 
    }

    // 2. Проверка на новую команду (API: ...)
    const std::string apiPrefix = "API: ";
    if (cleanPacket.rfind(apiPrefix, 0) == 0) {
        std::string cmdId = cleanPacket.substr(apiPrefix.length());
        CommandManager::Instance().AddCommand(cmdId);
        return;
    }
}

void NetworkClient::processIncomingData(char* buffer, int length) {
    incomingBuffer.append(buffer, length);

    size_t pos = 0;
    // Рубим склеенные пакеты по символу новой строки
    while ((pos = incomingBuffer.find('\n')) != std::string::npos) {
        std::string packet = incomingBuffer.substr(0, pos);
        handlePacket(packet);
        incomingBuffer.erase(0, pos + 1);
    }
}

void NetworkClient::run() {
    LOGD("Network Thread Started");

    while (isRunning) {
        // --- Реконнект ---
        if (sock == -1) {
            sock = socket(AF_INET, SOCK_STREAM, 0);
            struct sockaddr_in server;
            server.sin_addr.s_addr = inet_addr(Config::SERVER_IP);
            server.sin_family = AF_INET;
            server.sin_port = htons(Config::SERVER_PORT);

            if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
                close(sock);
                sock = -1;
                sleep(2); // Ждем перед повторной попыткой
                continue;
            }
            LOGD(">>> Connected to Python Server <<<");
            CommandManager::Instance().Clear(); 
        }

        // --- Отправка (из очереди) ---
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            while (!sendQueue.empty()) {
                if (sendData(sendQueue.front())) {
                    sendQueue.pop();
                } else {
                    close(sock);
                    sock = -1;
                    break; // Выход из цикла отправки, идем на реконнект
                }
            }
        }
        
        if (sock == -1) continue;

        // --- Чтение (non-blocking через select) ---
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);
        struct timeval timeout = {0, 50000}; // 50ms таймаут

        int activity = select(sock + 1, &readfds, NULL, NULL, &timeout);

        if (activity > 0 && FD_ISSET(sock, &readfds)) {
            char buffer[4096];
            memset(buffer, 0, sizeof(buffer));
            int read_size = recv(sock, buffer, sizeof(buffer) - 1, 0);

            if (read_size > 0) {
                processIncomingData(buffer, read_size);
            } else {
                LOGE("Server disconnected");
                close(sock);
                sock = -1;
            }
        }
        
        // Небольшая пауза, чтобы поток не ел 100% CPU
        usleep(10000); 
    }
}