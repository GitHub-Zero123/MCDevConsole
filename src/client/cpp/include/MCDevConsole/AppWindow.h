#pragma once

#include <memory>
#include <windows.h>

#include "MCDevConsole/WebViewHost.h"

namespace MCDevConsole {

class AppWindow {
public:
    AppWindow() = default;
    AppWindow(const AppWindow&) = delete;
    AppWindow& operator=(const AppWindow&) = delete;
    AppWindow(AppWindow&&) = delete;
    AppWindow& operator=(AppWindow&&) = delete;
    ~AppWindow() = default;

    bool Create(HINSTANCE instance, int show_command);
    int MessageLoop() const;

    [[nodiscard]] HWND Handle() const noexcept { return hwnd_; }
    [[nodiscard]] HINSTANCE Instance() const noexcept { return instance_; }

private:
    static constexpr const wchar_t* kWindowClassName = L"MCDevConsole.AppWindow";
    static constexpr const wchar_t* kWindowTitle = L"MCDevConsole";
    static constexpr int kInitialWidth = 1280;
    static constexpr int kInitialHeight = 800;
    static constexpr int kTitleBarHeight = 48;
    static constexpr int kResizeMargin = 10;

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param);

    bool RegisterWindowClass();
    LRESULT HandleMessage(UINT message, WPARAM w_param, LPARAM l_param);
    void HandleNCHitTest(HWND hwnd, int x, int y);
    bool IsInTitleBar(int y) const noexcept;

    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;
    std::unique_ptr<WebViewHost> webview_host_;
};

} // namespace MCDevConsole
