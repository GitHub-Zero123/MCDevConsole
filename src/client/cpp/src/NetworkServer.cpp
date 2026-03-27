#include "MCDevConsole/NetworkServer.h"
#include "MCDevConsole/WebViewHost.h"

#include <ws2tcpip.h>

#include <cstring>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#pragma comment(lib, "ws2_32.lib")

namespace MCDevConsole {
namespace {

bool ContainsIgnoreCase(const std::string& text, const std::string& needle) {
    if (needle.empty() || text.size() < needle.size()) {
        return false;
    }

    auto to_lower = [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    };

    for (std::size_t i = 0; i + needle.size() <= text.size(); ++i) {
        bool matched = true;
        for (std::size_t j = 0; j < needle.size(); ++j) {
            if (to_lower(static_cast<unsigned char>(text[i + j])) != to_lower(static_cast<unsigned char>(needle[j]))) {
                matched = false;
                break;
            }
        }
        if (matched) {
            return true;
        }
    }

    return false;
}

std::wstring ClassifyStdoutLevel(const std::string& line) {
    if (line.find("[INFO][Developer]") != std::string::npos) {
        return L"DEVELOPER";
    }
    if (ContainsIgnoreCase(line, "SUC")) {
        return L"SUCCESS";
    }
    if (ContainsIgnoreCase(line, "ERROR")) {
        return L"ERROR";
    }
    if (ContainsIgnoreCase(line, "WARN")) {
        return L"WARN";
    }
    if (ContainsIgnoreCase(line, "DEBUG")) {
        return L"DEBUG";
    }
    return L"INFO";
}

std::wstring Utf8ToWide(const std::string& input) {
    if (input.empty()) {
        return {};
    }

    const int required = MultiByteToWideChar(CP_UTF8, 0, input.c_str(), static_cast<int>(input.size()), nullptr, 0);
    if (required <= 0) {
        return std::wstring(input.begin(), input.end());
    }

    std::wstring result(static_cast<std::size_t>(required), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, input.c_str(), static_cast<int>(input.size()), &result[0], required);
    return result;
}

std::wstring EscapeJsonString(const std::wstring& value) {
    std::wstring result;
    result.reserve(value.size() + 16);

    for (wchar_t ch : value) {
        switch (ch) {
        case L'\\': result += L"\\\\"; break;
        case L'"': result += L"\\\""; break;
        case L'\n': result += L"\\n"; break;
        case L'\r': break;
        case L'\t': result += L"\\t"; break;
        default: result += ch; break;
        }
    }

    return result;
}

std::string ExtractJsonStringField(const std::string& json, const char* key) {
    const std::string needle = std::string("\"") + key + "\":\"";
    const std::size_t start = json.find(needle);
    if (start == std::string::npos) {
        return {};
    }

    std::string result;
    result.reserve(64);

    bool escaping = false;
    for (std::size_t i = start + needle.size(); i < json.size(); ++i) {
        const char ch = json[i];
        if (escaping) {
            switch (ch) {
            case 'n': result.push_back('\n'); break;
            case 'r': result.push_back('\r'); break;
            case 't': result.push_back('\t'); break;
            case '\\': result.push_back('\\'); break;
            case '"': result.push_back('"'); break;
            default: result.push_back(ch); break;
            }
            escaping = false;
            continue;
        }

        if (ch == '\\') {
            escaping = true;
            continue;
        }

        if (ch == '"') {
            break;
        }

        result.push_back(ch);
    }

    return result;
}

std::wstring MakeLogJson(const std::wstring& session_id,
                         const std::wstring& level,
                         const std::wstring& message,
                         const std::wstring& time_text,
                         const std::wstring& name,
                         const std::wstring& endpoint,
                         const std::wstring& platform) {
    std::wstring json = L"{\"kind\":\"log\",\"sessionId\":\"";
    json += EscapeJsonString(session_id);
    json += L"\",\"level\":\"";
    json += EscapeJsonString(level);
    json += L"\",\"message\":\"";
    json += EscapeJsonString(message);
    json += L"\",\"name\":\"";
    json += EscapeJsonString(name);
    json += L"\",\"endpoint\":\"";
    json += EscapeJsonString(endpoint);
    json += L"\",\"platform\":\"";
    json += EscapeJsonString(platform);
    json += L"\"";

    if (!time_text.empty()) {
        json += L",\"time\":\"";
        json += EscapeJsonString(time_text);
        json += L"\"";
    }

    json += L"}";
    return json;
}

} // namespace

NetworkServer::~NetworkServer() {
    Stop();
}

bool NetworkServer::Start(WebViewHost* webview_host) {
    if (running_.load()) {
        return false;
    }

    webview_host_ = webview_host;

    WSADATA wsa_data{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        return false;
    }

    udp_socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udp_socket_ == INVALID_SOCKET) {
        WSACleanup();
        return false;
    }

    BOOL reuse = TRUE;
    setsockopt(udp_socket_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    sockaddr_in udp_addr{};
    udp_addr.sin_family = AF_INET;
    udp_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    udp_addr.sin_port = htons(kDiscoveryPort);

    if (bind(udp_socket_, reinterpret_cast<sockaddr*>(&udp_addr), sizeof(udp_addr)) == SOCKET_ERROR) {
        closesocket(udp_socket_);
        udp_socket_ = INVALID_SOCKET;
        WSACleanup();
        return false;
    }

    tcp_socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (tcp_socket_ == INVALID_SOCKET) {
        closesocket(udp_socket_);
        udp_socket_ = INVALID_SOCKET;
        WSACleanup();
        return false;
    }

    setsockopt(tcp_socket_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    sockaddr_in tcp_addr{};
    tcp_addr.sin_family = AF_INET;
    tcp_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    tcp_addr.sin_port = htons(kTcpPort);

    if (bind(tcp_socket_, reinterpret_cast<sockaddr*>(&tcp_addr), sizeof(tcp_addr)) == SOCKET_ERROR) {
        closesocket(tcp_socket_);
        closesocket(udp_socket_);
        tcp_socket_ = INVALID_SOCKET;
        udp_socket_ = INVALID_SOCKET;
        WSACleanup();
        return false;
    }

    if (listen(tcp_socket_, SOMAXCONN) == SOCKET_ERROR) {
        closesocket(tcp_socket_);
        closesocket(udp_socket_);
        tcp_socket_ = INVALID_SOCKET;
        udp_socket_ = INVALID_SOCKET;
        WSACleanup();
        return false;
    }

    running_.store(true);
    udp_thread_ = std::thread(&NetworkServer::UdpDiscoveryThread, this);
    tcp_thread_ = std::thread(&NetworkServer::TcpAcceptThread, this);
    return true;
}

void NetworkServer::Stop() {
    if (!running_.load()) {
        return;
    }

    running_.store(false);

    if (udp_socket_ != INVALID_SOCKET) {
        closesocket(udp_socket_);
        udp_socket_ = INVALID_SOCKET;
    }

    if (tcp_socket_ != INVALID_SOCKET) {
        closesocket(tcp_socket_);
        tcp_socket_ = INVALID_SOCKET;
    }

    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        for (auto& session : sessions_) {
            session->online.store(false);
            if (session->socket != INVALID_SOCKET) {
                closesocket(session->socket);
                session->socket = INVALID_SOCKET;
            }
        }
        sessions_.clear();
    }

    if (udp_thread_.joinable()) {
        udp_thread_.join();
    }

    if (tcp_thread_.joinable()) {
        tcp_thread_.join();
    }

    WSACleanup();
}

void NetworkServer::UdpDiscoveryThread() {
    while (running_.load()) {
        char buffer[512]{};
        sockaddr_in from_addr{};
        int from_len = sizeof(from_addr);

        const int received = recvfrom(
            udp_socket_,
            buffer,
            static_cast<int>(sizeof(buffer) - 1),
            0,
            reinterpret_cast<sockaddr*>(&from_addr),
            &from_len);

        if (received == SOCKET_ERROR) {
            if (!running_.load()) {
                break;
            }
            continue;
        }

        buffer[received] = '\0';

        char host_name[256]{};
        DWORD host_name_size = static_cast<DWORD>(sizeof(host_name));
        if (gethostname(host_name, static_cast<int>(host_name_size)) == SOCKET_ERROR) {
            host_name[0] = '\0';
        }

        char ip_text[INET_ADDRSTRLEN]{};
        SOCKET probe_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        bool resolved = false;
        if (probe_socket != INVALID_SOCKET) {
            sockaddr_in probe_target{};
            probe_target.sin_family = AF_INET;
            probe_target.sin_addr = from_addr.sin_addr;
            probe_target.sin_port = htons(kDiscoveryPort);

            if (connect(probe_socket, reinterpret_cast<const sockaddr*>(&probe_target), sizeof(probe_target)) != SOCKET_ERROR) {
                sockaddr_in local_addr{};
                int local_len = sizeof(local_addr);
                if (getsockname(probe_socket, reinterpret_cast<sockaddr*>(&local_addr), &local_len) != SOCKET_ERROR) {
                    inet_ntop(AF_INET, &local_addr.sin_addr, ip_text, sizeof(ip_text));
                    resolved = std::string(ip_text) != "0.0.0.0";
                }
            }
            closesocket(probe_socket);
        }

        if (!resolved) {
            char host_buffer[256]{};
            if (gethostname(host_buffer, static_cast<int>(sizeof(host_buffer))) != SOCKET_ERROR) {
                addrinfo hints{};
                hints.ai_family = AF_INET;
                hints.ai_socktype = SOCK_STREAM;
                addrinfo* info = nullptr;
                if (getaddrinfo(host_buffer, nullptr, &hints, &info) == 0) {
                    for (addrinfo* current = info; current != nullptr; current = current->ai_next) {
                        auto* ipv4 = reinterpret_cast<sockaddr_in*>(current->ai_addr);
                        if (ipv4 != nullptr && ipv4->sin_addr.s_addr != htonl(INADDR_LOOPBACK)) {
                            inet_ntop(AF_INET, &ipv4->sin_addr, ip_text, sizeof(ip_text));
                            resolved = true;
                            break;
                        }
                    }
                    freeaddrinfo(info);
                }
            }
        }

        if (!resolved) {
            std::strcpy(ip_text, "127.0.0.1");
        }

        std::ostringstream response;
        response << "MCDEVCONSOLE_TCP " << ip_text << ' ' << kTcpPort << ' ' << (host_name[0] ? host_name : "MCDevConsole");
        const std::string payload = response.str();

        sendto(
            udp_socket_,
            payload.c_str(),
            static_cast<int>(payload.size()),
            0,
            reinterpret_cast<const sockaddr*>(&from_addr),
            from_len);
    }
}

void NetworkServer::TcpAcceptThread() {
    while (running_.load()) {
        sockaddr_in client_addr{};
        int client_len = sizeof(client_addr);
        SOCKET client_socket = accept(tcp_socket_, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
        if (client_socket == INVALID_SOCKET) {
            if (!running_.load()) {
                break;
            }
            continue;
        }

        char ip_text[INET_ADDRSTRLEN]{};
        inet_ntop(AF_INET, &client_addr.sin_addr, ip_text, sizeof(ip_text));
        const std::string client_ip = ip_text;
        const std::string client_port = std::to_string(ntohs(client_addr.sin_port));

        auto session = std::make_shared<ClientSession>();
        session->socket = client_socket;
        session->session_id = client_ip + ":" + client_port;
        session->display_name = client_ip + ":" + client_port;
        session->endpoint = client_ip + ":" + client_port;
        session->platform = "Unknown";
        session->online.store(true);
        session->last_activity_tick.store(GetTickCount64());

        constexpr DWORD RECV_TIMEOUT_MS = 1500;
        setsockopt(
            client_socket,
            SOL_SOCKET,
            SO_RCVTIMEO,
            reinterpret_cast<const char*>(&RECV_TIMEOUT_MS),
            sizeof(RECV_TIMEOUT_MS));

        {
            std::lock_guard<std::mutex> lock(sessions_mutex_);
            sessions_.push_back(session);
        }

        NotifyDeviceUpsert(session);
        std::thread(&NetworkServer::TcpClientThread, this, session).detach();
    }
}

void NetworkServer::TcpClientThread(std::shared_ptr<ClientSession> session) {
    std::vector<std::uint8_t> temp_buffer(4096);

    while (running_.load() && session->online.load()) {
        const int received = recv(
            session->socket,
            reinterpret_cast<char*>(temp_buffer.data()),
            static_cast<int>(temp_buffer.size()),
            0);

        if (received <= 0) {
            if (received == 0) {
                break;
            }

            const int err = WSAGetLastError();
            if (err == WSAETIMEDOUT || err == WSAEWOULDBLOCK) {
                const unsigned long long now = GetTickCount64();
                const unsigned long long last = session->last_activity_tick.load();
                if (now > last && (now - last) > 5000) {
                    break;
                }
                continue;
            }

            break;
        }

        session->last_activity_tick.store(GetTickCount64());
        session->recv_buffer.insert(session->recv_buffer.end(), temp_buffer.begin(), temp_buffer.begin() + received);

        while (true) {
            Protocol::Packet packet;
            std::size_t consumed_size = 0;
            if (!Protocol::TryDeserializePacket(session->recv_buffer, packet, consumed_size)) {
                break;
            }

            session->recv_buffer.erase(session->recv_buffer.begin(), session->recv_buffer.begin() + static_cast<std::ptrdiff_t>(consumed_size));
            HandlePacket(session, packet);
        }
    }

    session->online.store(false);
    if (session->socket != INVALID_SOCKET) {
        closesocket(session->socket);
        session->socket = INVALID_SOCKET;
    }

    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        sessions_.erase(
            std::remove_if(
                sessions_.begin(),
                sessions_.end(),
                [&session](const std::shared_ptr<ClientSession>& item) {
                    return item == session;
                }),
            sessions_.end());
    }

    NotifyDeviceOffline(session);
}

void NetworkServer::HandlePacket(const std::shared_ptr<ClientSession>& session, const Protocol::Packet& packet) {
    if (!webview_host_) {
        return;
    }

    const std::string payload(packet.data.begin(), packet.data.end());
    const std::string time_text = ExtractJsonStringField(payload, "time");
    const std::string message_text = [&payload]() -> std::string {
        const std::string extracted = ExtractJsonStringField(payload, "message");
        return extracted.empty() ? payload : extracted;
    }();
    const std::string platform_text = ExtractJsonStringField(payload, "platform");
    const std::string session_name = ExtractJsonStringField(payload, "name");

    bool metadata_changed = false;
    if (!platform_text.empty() && session->platform != platform_text) {
        session->platform = platform_text;
        metadata_changed = true;
    }
    if (!session_name.empty() && session->display_name != session_name) {
        session->display_name = session_name;
        metadata_changed = true;
    }

    if (metadata_changed) {
        NotifyDeviceUpsert(session);
    }

    if (packet.type_id == Protocol::TYPE_STDOUT_LOG || packet.type_id == Protocol::TYPE_STDERR_LOG) {
        if (packet.type_id == Protocol::TYPE_STDOUT_LOG
            && message_text.find(" [INFO][Engine] ") != std::string::npos) {
            return;
        }

        const std::wstring level = packet.type_id == Protocol::TYPE_STDERR_LOG
            ? L"STDERR"
            : ClassifyStdoutLevel(message_text);

        webview_host_->PostJsonMessage(
            MakeLogJson(
                Utf8ToWide(session->session_id),
                level,
                Utf8ToWide(message_text),
                Utf8ToWide(time_text),
                Utf8ToWide(session->display_name.empty() ? session->session_id : session->display_name),
                Utf8ToWide(session->endpoint),
                Utf8ToWide(session->platform))
        );
        return;
    }
}

void NetworkServer::ReplaySessionsToFrontend() {
    if (!webview_host_) {
        return;
    }

    std::lock_guard<std::mutex> lock(sessions_mutex_);
    for (const auto& session : sessions_) {
        if (session && session->online.load()) {
            NotifyDeviceUpsert(session);
        }
    }
}

void NetworkServer::NotifyDeviceUpsert(const std::shared_ptr<ClientSession>& session) {
    if (!webview_host_) {
        return;
    }

    const std::wstring session_id = Utf8ToWide(session->session_id);
    const std::wstring display_name = Utf8ToWide(session->display_name.empty() ? session->session_id : session->display_name);
    const std::wstring endpoint = Utf8ToWide(session->endpoint);
    const std::wstring platform = Utf8ToWide(session->platform);

    std::wstring json = L"{\"kind\":\"device.upsert\",\"id\":\"";
    json += EscapeJsonString(session_id);
    json += L"\",\"sessionId\":\"";
    json += EscapeJsonString(session_id);
    json += L"\",\"name\":\"";
    json += EscapeJsonString(display_name);
    json += L"\",\"online\":true,\"endpoint\":\"";
    json += EscapeJsonString(endpoint);
    json += L"\",\"platform\":\"";
    json += EscapeJsonString(platform);
    json += L"\"}";

    webview_host_->PostJsonMessage(json);
}

void NetworkServer::NotifyDeviceOffline(const std::shared_ptr<ClientSession>& session) {
    if (!webview_host_) {
        return;
    }

    const std::wstring session_id = Utf8ToWide(session->session_id);
    std::wstring json = L"{\"kind\":\"device.offline\",\"id\":\"";
    json += EscapeJsonString(session_id);
    json += L"\",\"sessionId\":\"";
    json += EscapeJsonString(session_id);
    json += L"\"}";

    webview_host_->PostJsonMessage(json);
}

void NetworkServer::SendExecCommand(const std::string& session_id, std::uint16_t type_id, const std::string& code) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);

    for (const auto& session : sessions_) {
        if (!session->online.load() || session->session_id != session_id || session->socket == INVALID_SOCKET) {
            continue;
        }

        Protocol::Packet packet;
        packet.type_id = type_id;
        packet.data.assign(code.begin(), code.end());

        const auto serialized = Protocol::SerializePacket(packet);
        send(
            session->socket,
            reinterpret_cast<const char*>(serialized.data()),
            static_cast<int>(serialized.size()),
            0);
        break;
    }
}

} // namespace MCDevConsole
