#include "chat_server.hpp"
#include <iostream>
#include <cstring>
#include <sstream>
#include <algorithm>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <fcntl.h>

namespace elberr {

ChatServer::ChatServer(int port, MessageCallback onMessage)
    : port_(port), onMessage_(std::move(onMessage)) {}

ChatServer::~ChatServer() {
    stop();
}

void ChatServer::start() {
    if (running_.load()) return;

    serverFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd_ < 0) {
        std::cerr << "[Chat] Failed to create socket\n";
        return;
    }

    int opt = 1;
    setsockopt(serverFd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);

    if (bind(serverFd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "[Chat] Failed to bind port " << port_ << "\n";
        close(serverFd_);
        serverFd_ = -1;
        return;
    }

    listen(serverFd_, 5);
    stopFlag_ = false;
    running_ = true;
    acceptThread_ = std::thread(&ChatServer::acceptLoop, this);

    std::cout << "[Chat] Server started on http://localhost:" << port_ << "\n";
}

void ChatServer::stop() {
    if (!running_.load()) return;
    stopFlag_ = true;
    if (serverFd_ >= 0) {
        shutdown(serverFd_, SHUT_RDWR);
        close(serverFd_);
        serverFd_ = -1;
    }
    if (acceptThread_.joinable()) acceptThread_.join();

    std::lock_guard<std::mutex> lock(clientsMtx_);
    for (auto& c : clients_) {
        close(c.fd);
    }
    clients_.clear();
    running_ = false;
}

void ChatServer::acceptLoop() {
    // Set server socket non-blocking
    fcntl(serverFd_, F_SETFL, O_NONBLOCK);

    while (!stopFlag_.load()) {
        // Poll for new connections and existing client data
        std::vector<struct pollfd> fds;

        {
            struct pollfd pfd;
            pfd.fd = serverFd_;
            pfd.events = POLLIN;
            pfd.revents = 0;
            fds.push_back(pfd);
        }

        {
            std::lock_guard<std::mutex> lock(clientsMtx_);
            for (auto& c : clients_) {
                struct pollfd pfd;
                pfd.fd = c.fd;
                pfd.events = POLLIN;
                pfd.revents = 0;
                fds.push_back(pfd);
            }
        }

        int ret = poll(fds.data(), fds.size(), 200); // 200ms timeout
        if (ret <= 0) continue;

        // Check for new connections
        if (fds[0].revents & POLLIN) {
            struct sockaddr_in clientAddr{};
            socklen_t clientLen = sizeof(clientAddr);
            int clientFd = accept(serverFd_, (struct sockaddr*)&clientAddr, &clientLen);
            if (clientFd >= 0) {
                fcntl(clientFd, F_SETFL, O_NONBLOCK);
                std::lock_guard<std::mutex> lock(clientsMtx_);
                clients_.push_back({clientFd, false, ""});
            }
        }

        // Check existing clients
        for (size_t i = 1; i < fds.size(); ++i) {
            if (fds[i].revents & POLLIN) {
                handleClient(fds[i].fd);
            }
            if (fds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
                removeClient(fds[i].fd);
            }
        }
    }
}

void ChatServer::handleClient(int clientFd) {
    char buf[4096];
    ssize_t n = recv(clientFd, buf, sizeof(buf) - 1, 0);
    if (n <= 0) {
        removeClient(clientFd);
        return;
    }
    buf[n] = '\0';

    // Find client
    std::lock_guard<std::mutex> lock(clientsMtx_);
    Client* client = nullptr;
    for (auto& c : clients_) {
        if (c.fd == clientFd) { client = &c; break; }
    }
    if (!client) return;

    if (!client->isWebSocket) {
        std::string request(buf, n);

        // Check for WebSocket upgrade
        if (request.find("Upgrade: websocket") != std::string::npos ||
            request.find("Upgrade: WebSocket") != std::string::npos) {
            if (upgradeWebSocket(clientFd, request)) {
                client->isWebSocket = true;
                // Send current status
                std::lock_guard<std::mutex> slock(statusMtx_);
                if (!lastStatus_.empty()) {
                    sendWebSocketFrame(clientFd, lastStatus_);
                }
            }
            return;
        }

        // Regular HTTP
        handleHTTP(clientFd, request);
        // Close after HTTP response (not keep-alive for simplicity)
        removeClient(clientFd);
    } else {
        // WebSocket frame
        client->buffer.append(buf, n);
        handleWebSocketFrame(clientFd);
    }
}

void ChatServer::handleHTTP(int fd, const std::string& request) {
    std::string path;
    if (request.find("GET /api/state") == 0 || request.find("GET /api/state") != std::string::npos) {
        // REST API
        std::lock_guard<std::mutex> lock(statusMtx_);
        std::string body = lastStatus_.empty() ? "{}" : lastStatus_;
        std::string response = "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Content-Length: " + std::to_string(body.size()) + "\r\n"
            "Connection: close\r\n\r\n" + body;
        send(fd, response.c_str(), response.size(), MSG_NOSIGNAL);
        return;
    }

    // Serve HTML
    std::string html = generateHTML();
    std::string response = "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: " + std::to_string(html.size()) + "\r\n"
        "Connection: close\r\n\r\n" + html;
    send(fd, response.c_str(), response.size(), MSG_NOSIGNAL);
}

bool ChatServer::upgradeWebSocket(int fd, const std::string& request) {
    // Find Sec-WebSocket-Key
    std::string key;
    size_t keyPos = request.find("Sec-WebSocket-Key: ");
    if (keyPos == std::string::npos) return false;
    keyPos += 19;
    size_t keyEnd = request.find("\r\n", keyPos);
    if (keyEnd == std::string::npos) return false;
    key = request.substr(keyPos, keyEnd - keyPos);

    // SHA-1 + Base64 handshake
    std::string magic = key + "258EAFA5-E914-47DA-95CA-5AB5DC11E5B3";
    std::string hash = sha1(magic);
    std::string accept = base64Encode(hash);

    std::string response = "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: " + accept + "\r\n\r\n";

    send(fd, response.c_str(), response.size(), MSG_NOSIGNAL);
    return true;
}

void ChatServer::handleWebSocketFrame(int fd) {
    Client* client = nullptr;
    for (auto& c : clients_) {
        if (c.fd == fd) { client = &c; break; }
    }
    if (!client || client->buffer.size() < 2) return;

    unsigned char* data = (unsigned char*)client->buffer.data();
    // bool fin = (data[0] & 0x80) != 0;
    int opcode = data[0] & 0x0F;
    bool masked = (data[1] & 0x80) != 0;
    uint64_t payloadLen = data[1] & 0x7F;

    size_t headerLen = 2;
    if (payloadLen == 126) {
        if (client->buffer.size() < 4) return;
        payloadLen = (data[2] << 8) | data[3];
        headerLen = 4;
    } else if (payloadLen == 127) {
        if (client->buffer.size() < 10) return;
        payloadLen = 0;
        for (int i = 0; i < 8; ++i) {
            payloadLen = (payloadLen << 8) | data[2 + i];
        }
        headerLen = 10;
    }

    // Limit frame size (DoS protection)
    if (payloadLen > 65536) {
        removeClient(fd);
        return;
    }

    size_t maskOffset = headerLen;
    if (masked) headerLen += 4;

    if (client->buffer.size() < headerLen + payloadLen) return; // incomplete

    // Unmask
    std::string payload(payloadLen, '\0');
    if (masked) {
        unsigned char mask[4];
        memcpy(mask, data + maskOffset, 4);
        for (uint64_t i = 0; i < payloadLen; ++i) {
            payload[i] = data[headerLen + i] ^ mask[i % 4];
        }
    } else {
        memcpy(&payload[0], data + headerLen, payloadLen);
    }

    // Consume from buffer
    client->buffer.erase(0, headerLen + payloadLen);

    if (opcode == 0x08) {
        // Close frame
        removeClient(fd);
        return;
    }

    if (opcode == 0x09) {
        // Ping → Pong
        sendWebSocketFrame(fd, payload);
        return;
    }

    if (opcode == 0x01 || opcode == 0x02) {
        // Text or binary frame — this is a chat message
        if (onMessage_ && !payload.empty()) {
            onMessage_(payload);
        }
    }
}

void ChatServer::sendWebSocketFrame(int fd, const std::string& payload) {
    std::string frame;
    frame += (char)0x81; // FIN + text opcode

    if (payload.size() < 126) {
        frame += (char)payload.size();
    } else if (payload.size() < 65536) {
        frame += (char)126;
        frame += (char)((payload.size() >> 8) & 0xFF);
        frame += (char)(payload.size() & 0xFF);
    } else {
        frame += (char)127;
        for (int i = 7; i >= 0; --i) {
            frame += (char)((payload.size() >> (i * 8)) & 0xFF);
        }
    }

    frame += payload;
    send(fd, frame.c_str(), frame.size(), MSG_NOSIGNAL);
}

void ChatServer::broadcast(const std::string& msg) {
    std::lock_guard<std::mutex> lock(clientsMtx_);
    for (auto& c : clients_) {
        if (c.isWebSocket) {
            sendWebSocketFrame(c.fd, msg);
        }
    }
}

void ChatServer::broadcastStatus(const std::string& json) {
    {
        std::lock_guard<std::mutex> lock(statusMtx_);
        lastStatus_ = json;
    }
    broadcast(json);
}

void ChatServer::removeClient(int fd) {
    // Note: called with clientsMtx_ held
    close(fd);
    clients_.erase(
        std::remove_if(clients_.begin(), clients_.end(),
            [fd](const Client& c) { return c.fd == fd; }),
        clients_.end()
    );
}

// SHA-1 implementation (RFC 3174)
std::string ChatServer::sha1(const std::string& input) const {
    uint32_t h0 = 0x67452301;
    uint32_t h1 = 0xEFCDAB89;
    uint32_t h2 = 0x98BADCFE;
    uint32_t h3 = 0x10325476;
    uint32_t h4 = 0xC3D2E1F0;

    // Preprocessing
    std::string msg = input;
    uint64_t origBits = msg.size() * 8;
    msg += (char)0x80;
    while (msg.size() % 64 != 56) msg += (char)0x00;
    for (int i = 7; i >= 0; --i) {
        msg += (char)((origBits >> (i * 8)) & 0xFF);
    }

    // Process 512-bit blocks
    for (size_t block = 0; block < msg.size(); block += 64) {
        uint32_t w[80];
        for (int i = 0; i < 16; ++i) {
            w[i] = ((uint32_t)(unsigned char)msg[block + i*4] << 24) |
                   ((uint32_t)(unsigned char)msg[block + i*4 + 1] << 16) |
                   ((uint32_t)(unsigned char)msg[block + i*4 + 2] << 8) |
                   ((uint32_t)(unsigned char)msg[block + i*4 + 3]);
        }
        for (int i = 16; i < 80; ++i) {
            uint32_t val = w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16];
            w[i] = (val << 1) | (val >> 31);
        }

        uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;
        for (int i = 0; i < 80; ++i) {
            uint32_t f, k;
            if (i < 20) { f = (b & c) | ((~b) & d); k = 0x5A827999; }
            else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
            else { f = b ^ c ^ d; k = 0xCA62C1D6; }

            uint32_t temp = ((a << 5) | (a >> 27)) + f + e + k + w[i];
            e = d; d = c; c = (b << 30) | (b >> 2); b = a; a = temp;
        }

        h0 += a; h1 += b; h2 += c; h3 += d; h4 += e;
    }

    std::string result(20, '\0');
    for (int i = 0; i < 4; ++i) {
        result[i]    = (h0 >> (24 - i*8)) & 0xFF;
        result[4+i]  = (h1 >> (24 - i*8)) & 0xFF;
        result[8+i]  = (h2 >> (24 - i*8)) & 0xFF;
        result[12+i] = (h3 >> (24 - i*8)) & 0xFF;
        result[16+i] = (h4 >> (24 - i*8)) & 0xFF;
    }
    return result;
}

std::string ChatServer::base64Encode(const std::string& input) const {
    static const char table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string encoded;
    int val = 0, valb = -6;
    for (unsigned char c : input) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            encoded += table[(val >> valb) & 0x3F];
            valb -= 6;
        }
    }
    if (valb > -6) encoded += table[((val << 8) >> (valb + 8)) & 0x3F];
    while (encoded.size() % 4) encoded += '=';
    return encoded;
}

std::string ChatServer::generateHTML() const {
    return R"HTML(<!DOCTYPE html>
<html lang="ru">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>E.L.B.E.R.R — Chat</title>
<style>
* { margin: 0; padding: 0; box-sizing: border-box; }
body {
    background: #0a0a0f;
    color: #e0e0e0;
    font-family: 'Segoe UI', system-ui, sans-serif;
    display: flex;
    flex-direction: column;
    height: 100vh;
}
header {
    background: linear-gradient(135deg, #1a1a2e, #16213e);
    padding: 12px 20px;
    border-bottom: 1px solid #0f3460;
    display: flex;
    justify-content: space-between;
    align-items: center;
}
header h1 {
    font-size: 1.2em;
    color: #00d4ff;
    text-shadow: 0 0 10px rgba(0,212,255,0.3);
}
.status-bar {
    display: flex;
    gap: 16px;
    font-size: 0.8em;
    color: #888;
}
.status-bar span { color: #00d4ff; }
.main-area {
    display: flex;
    flex: 1;
    overflow: hidden;
}
.sidebar {
    width: 280px;
    background: #0d0d15;
    border-right: 1px solid #1a1a2e;
    padding: 12px;
    overflow-y: auto;
    font-size: 0.85em;
}
.sidebar h3 {
    color: #888;
    font-size: 0.75em;
    text-transform: uppercase;
    margin: 12px 0 6px 0;
    letter-spacing: 1px;
}
.goal {
    padding: 6px 8px;
    margin: 2px 0;
    border-radius: 4px;
    background: #111;
}
.goal.done { opacity: 0.5; }
.goal.done::before { content: "✓ "; color: #4caf50; }
.goal.active { border-left: 3px solid #00d4ff; }
.drive-bar {
    height: 6px;
    background: #1a1a2e;
    border-radius: 3px;
    margin: 4px 0;
    overflow: hidden;
}
.drive-fill {
    height: 100%;
    border-radius: 3px;
    transition: width 0.5s;
}
.drive-curiosity .drive-fill { background: #00d4ff; }
.drive-sociality .drive-fill { background: #ff6b9d; }
.drive-conservatism .drive-fill { background: #ffd93d; }
.drive-restlessness .drive-fill { background: #6bcb77; }
.chat-area {
    flex: 1;
    display: flex;
    flex-direction: column;
}
#messages {
    flex: 1;
    overflow-y: auto;
    padding: 16px;
}
.msg {
    margin: 8px 0;
    padding: 10px 14px;
    border-radius: 12px;
    max-width: 70%;
    word-wrap: break-word;
}
.msg.user {
    background: #1a3a5c;
    margin-left: auto;
    border-bottom-right-radius: 4px;
}
.msg.agent {
    background: #1a1a2e;
    border-bottom-left-radius: 4px;
    border: 1px solid #2a2a3e;
}
.msg.event {
    background: none;
    color: #555;
    font-size: 0.8em;
    max-width: 100%;
    text-align: center;
    padding: 4px;
}
.input-area {
    padding: 12px 16px;
    border-top: 1px solid #1a1a2e;
    display: flex;
    gap: 8px;
}
#chatInput {
    flex: 1;
    background: #111;
    border: 1px solid #2a2a3e;
    border-radius: 8px;
    padding: 10px 14px;
    color: #e0e0e0;
    font-size: 1em;
    outline: none;
}
#chatInput:focus { border-color: #00d4ff; }
#sendBtn {
    background: #00d4ff;
    color: #000;
    border: none;
    border-radius: 8px;
    padding: 10px 20px;
    cursor: pointer;
    font-weight: bold;
}
#sendBtn:hover { background: #00b8d9; }
.speak-indicator {
    display: inline-block;
    width: 8px;
    height: 8px;
    border-radius: 50%;
    margin-right: 6px;
}
.speak-yes { background: #4caf50; box-shadow: 0 0 6px #4caf50; }
.speak-no { background: #ff5252; }
</style>
</head>
<body>
<header>
    <h1>E.L.B.E.R.R</h1>
    <div class="status-bar">
        <div id="speakStatus"><span class="speak-indicator speak-no"></span>learning...</div>
        <div>Cycle: <span id="cycleCount">0</span></div>
        <div>Facts: <span id="beliefCount">0</span></div>
        <div>Words: <span id="wordCount">0</span></div>
        <div>Pages: <span id="pageCount">0</span></div>
    </div>
</header>
<div class="main-area">
    <div class="sidebar">
        <h3>Goals</h3>
        <div id="goalList"></div>
        <h3>Drives</h3>
        <div id="driveList"></div>
        <h3>Self-Modification</h3>
        <div id="codeStats" style="font-size:0.8em;color:#888;"></div>
    </div>
    <div class="chat-area">
        <div id="messages"></div>
        <div class="input-area">
            <input type="text" id="chatInput" placeholder="Write a message..." autocomplete="off">
            <button id="sendBtn">Send</button>
        </div>
    </div>
</div>
<script>
let ws;
function connect() {
    ws = new WebSocket('ws://' + location.host);
    ws.onmessage = function(e) {
        try {
            const data = JSON.parse(e.data);
            if (data.type === 'status') updateStatus(data);
            else if (data.type === 'response') addMessage(data.text, 'agent');
            else if (data.type === 'event') addMessage(data.text, 'event');
        } catch(err) {}
    };
    ws.onclose = function() { setTimeout(connect, 2000); };
    ws.onerror = function() { ws.close(); };
}
function updateStatus(s) {
    document.getElementById('cycleCount').textContent = s.cycle;
    document.getElementById('beliefCount').textContent = s.beliefs;
    document.getElementById('wordCount').textContent = s.words;
    document.getElementById('pageCount').textContent = s.pages;

    const si = document.getElementById('speakStatus');
    if (s.canSpeak) {
        si.innerHTML = '<span class="speak-indicator speak-yes"></span>can speak';
    } else {
        si.innerHTML = '<span class="speak-indicator speak-no"></span>learning... (' + s.words + ' words, ' + s.patterns + ' patterns)';
    }

    const gl = document.getElementById('goalList');
    gl.innerHTML = '';
    (s.goals || []).forEach(g => {
        const div = document.createElement('div');
        div.className = 'goal' + (g.done ? ' done' : ' active');
        div.textContent = g.id;
        gl.appendChild(div);
    });

    const dl = document.getElementById('driveList');
    dl.innerHTML = '';
    // Code stats
    const cs = document.getElementById('codeStats');
    if (s.codeFiles !== undefined) {
        cs.innerHTML = '<div>Source files: <span style="color:#00d4ff">' + (s.codeFiles||0) + '</span></div>' +
            '<div>Functions: <span style="color:#00d4ff">' + (s.codeFunctions||0) + '</span></div>' +
            '<div>Patches applied: <span style="color:#4caf50">' + (s.codePatches||0) + '</span>' +
            ' / failed: <span style="color:#ff5252">' + (s.codeFailedPatches||0) + '</span></div>';
    }

    (s.drives || []).forEach(d => {
        const label = document.createElement('div');
        label.style.cssText = 'display:flex;justify-content:space-between;font-size:0.8em;color:#888';
        label.innerHTML = '<span>' + d.name + '</span><span>' + (d.val*100).toFixed(0) + '%</span>';
        dl.appendChild(label);
        const bar = document.createElement('div');
        bar.className = 'drive-bar drive-' + d.name;
        bar.innerHTML = '<div class="drive-fill" style="width:' + (d.val*100) + '%"></div>';
        dl.appendChild(bar);
    });
}
function addMessage(text, type) {
    const div = document.createElement('div');
    div.className = 'msg ' + type;
    div.textContent = text;
    const msgs = document.getElementById('messages');
    msgs.appendChild(div);
    msgs.scrollTop = msgs.scrollHeight;
}
function sendMessage() {
    const input = document.getElementById('chatInput');
    const text = input.value.trim();
    if (!text || !ws || ws.readyState !== 1) return;
    ws.send(text);
    addMessage(text, 'user');
    input.value = '';
}
document.getElementById('sendBtn').onclick = sendMessage;
document.getElementById('chatInput').onkeydown = function(e) {
    if (e.key === 'Enter') sendMessage();
};
connect();
</script>
</body>
</html>)HTML";
}

} // namespace elberr
