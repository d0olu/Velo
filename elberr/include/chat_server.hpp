#pragma once
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>

namespace elberr {

// Minimal WebSocket + HTTP chat server on POSIX sockets
// Serves HTML UI and handles real-time chat via WebSocket (RFC 6455)
class ChatServer {
public:
    using MessageCallback = std::function<void(const std::string&)>;

    ChatServer(int port, MessageCallback onMessage);
    ~ChatServer();

    void start();
    void stop();

    // Send message to all connected WebSocket clients
    void broadcast(const std::string& msg);

    // Send JSON status update
    void broadcastStatus(const std::string& json);

    bool isRunning() const { return running_.load(); }

private:
    int port_;
    int serverFd_ = -1;
    MessageCallback onMessage_;

    std::thread acceptThread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stopFlag_{false};

    struct Client {
        int fd;
        bool isWebSocket;
        std::string buffer;
    };

    std::vector<Client> clients_;
    std::mutex clientsMtx_;

    void acceptLoop();
    void handleClient(int clientFd);
    void handleHTTP(int fd, const std::string& request);
    bool upgradeWebSocket(int fd, const std::string& request);
    void handleWebSocketFrame(int fd);
    void sendWebSocketFrame(int fd, const std::string& payload);
    void removeClient(int fd);

    std::string generateHTML() const;
    std::string sha1(const std::string& input) const;
    std::string base64Encode(const std::string& input) const;

    // State for UI
    std::string lastStatus_;
    std::mutex statusMtx_;
};

} // namespace elberr
