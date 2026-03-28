#pragma once

#include <string>
#include <wrl/client.h>
#include <windows.h>

#include <WebView2.h>

#include "MCDevConsole/NetworkServer.h"

namespace MCDevConsole {

class WebViewHost {
public:
    WebViewHost() = default;
    WebViewHost(const WebViewHost&) = delete;
    WebViewHost& operator=(const WebViewHost&) = delete;
    WebViewHost(WebViewHost&&) = delete;
    WebViewHost& operator=(WebViewHost&&) = delete;
    ~WebViewHost() = default;

    bool Initialize(HWND parent_window);
    void SetNetworkServer(NetworkServer* network_server) noexcept;
    void ResizeToClientArea() const;
    void NavigateToString(const std::wstring& html) const;
    void NavigateToFile(const std::wstring& file_path) const;
    void PostJsonMessage(const std::wstring& json) const;
    void PostJsonMessageNow(const std::wstring& json) const;
    void SetBackgroundColor(COLORREF color) const;

    [[nodiscard]] bool IsReady() const noexcept;

private:
    void ReplaySessionsIfReady() const;

    HWND parent_window_ = nullptr;
    NetworkServer* network_server_ = nullptr;
    bool frontend_ready_ = false;
    Microsoft::WRL::ComPtr<ICoreWebView2Controller> controller_;
    Microsoft::WRL::ComPtr<ICoreWebView2> webview_;
};

} // namespace MCDevConsole
