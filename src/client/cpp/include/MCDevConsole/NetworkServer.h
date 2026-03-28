#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <winsock2.h>

#include "MCDevConsole/Protocol.h"

namespace MCDevConsole {

class WebViewHost;

struct ClientSession {
    SOCKET socket = INVALID_SOCKET;
    std::string session_id;
    std::string display_name;
    std::string endpoint;
    std::string platform;
    std::atomic<bool> online{false};
    std::atomic<unsigned long long> last_activity_tick{0};
    std::vector<std::uint8_t> recv_buffer;
};

class NetworkServer {
public:
    static constexpr std::uint16_t kDiscoveryPort = 18232;
    static constexpr std::uint16_t kTcpPort = 18233;

    NetworkServer() = default;
    ~NetworkServer();

    NetworkServer(const NetworkServer&) = delete;
    NetworkServer& operator=(const NetworkServer&) = delete;
    NetworkServer(NetworkServer&&) = delete;
    NetworkServer& operator=(NetworkServer&&) = delete;

    bool Start(WebViewHost* webview_host);
    void SetWebViewHost(WebViewHost* webview_host) noexcept;
    void Stop();
    void ReplaySessionsToFrontend();

    void SendExecCommand(const std::string& session_id, std::uint16_t type_id, const std::string& code);
    
    static bool IsPortInUse(std::uint16_t port);

private:
    void UdpDiscoveryThread();
    void TcpAcceptThread();
    void TcpClientThread(std::shared_ptr<ClientSession> session);

    void HandlePacket(const std::shared_ptr<ClientSession>& session, const Protocol::Packet& packet);
    void NotifyDeviceUpsert(const std::shared_ptr<ClientSession>& session);
    void NotifyDeviceOffline(const std::shared_ptr<ClientSession>& session);

    WebViewHost* webview_host_ = nullptr;
    SOCKET udp_socket_ = INVALID_SOCKET;
    SOCKET tcp_socket_ = INVALID_SOCKET;
    std::atomic<bool> running_{false};
    std::thread udp_thread_;
    std::thread tcp_thread_;
    std::vector<std::shared_ptr<ClientSession>> sessions_;
    std::mutex sessions_mutex_;
};

} // namespace MCDevConsole
