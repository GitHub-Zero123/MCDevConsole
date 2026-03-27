#pragma once

#include <string>
#include <wrl/client.h>
#include <windows.h>

#include <WebView2.h>

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
    void ResizeToClientArea() const;
    void NavigateToString(const std::wstring& html) const;
    void NavigateToFile(const std::wstring& file_path) const;
    void PostJsonMessage(const std::wstring& json) const;

    [[nodiscard]] bool IsReady() const noexcept;

private:
    HWND parent_window_ = nullptr;
    Microsoft::WRL::ComPtr<ICoreWebView2Controller> controller_;
    Microsoft::WRL::ComPtr<ICoreWebView2> webview_;
};

} // namespace MCDevConsole
