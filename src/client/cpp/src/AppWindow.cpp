#include "MCDevConsole/AppWindow.h"

#include <windowsx.h>

namespace MCDevConsole {

bool AppWindow::Create(HINSTANCE instance, int show_command) {
    instance_ = instance;

    if (!RegisterWindowClass()) {
        return false;
    }

    hwnd_ = CreateWindowExW(
        WS_EX_NOREDIRECTIONBITMAP,
        kWindowClassName,
        kWindowTitle,
        WS_POPUP | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        kInitialWidth,
        kInitialHeight,
        nullptr,
        nullptr,
        instance_,
        this);

    if (hwnd_ == nullptr) {
        return false;
    }

    ShowWindow(hwnd_, show_command);
    UpdateWindow(hwnd_);

    webview_host_ = std::make_unique<WebViewHost>();
    if (!webview_host_->Initialize(hwnd_)) {
        webview_host_.reset();
    }

    return true;
}

int AppWindow::MessageLoop() const {
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return static_cast<int>(message.wParam);
}

LRESULT CALLBACK AppWindow::WindowProc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param) {
    AppWindow* self = nullptr;

    if (message == WM_NCCREATE) {
        auto* create_struct = reinterpret_cast<CREATESTRUCTW*>(l_param);
        self = static_cast<AppWindow*>(create_struct->lpCreateParams);
        self->hwnd_ = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<AppWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self != nullptr) {
        return self->HandleMessage(message, w_param, l_param);
    }

    return DefWindowProcW(hwnd, message, w_param, l_param);
}

bool AppWindow::RegisterWindowClass() {
    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = &AppWindow::WindowProc;
    window_class.hInstance = instance_;
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.hbrBackground = nullptr;
    window_class.lpszClassName = kWindowClassName;

    const ATOM atom = RegisterClassExW(&window_class);
    return atom != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

bool AppWindow::IsInTitleBar(int y) const noexcept {
    return y >= 0 && y < kTitleBarHeight;
}

void AppWindow::HandleNCHitTest(HWND hwnd, int x, int y) {
    RECT rect;
    GetWindowRect(hwnd, &rect);

    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;

    // 四个角落
    if (x < kResizeMargin && y < kResizeMargin) {
        SetCursor(LoadCursorW(nullptr, IDC_SIZENWSE));
    } else if (x >= width - kResizeMargin && y < kResizeMargin) {
        SetCursor(LoadCursorW(nullptr, IDC_SIZENESW));
    } else if (x < kResizeMargin && y >= height - kResizeMargin) {
        SetCursor(LoadCursorW(nullptr, IDC_SIZENESW));
    } else if (x >= width - kResizeMargin && y >= height - kResizeMargin) {
        SetCursor(LoadCursorW(nullptr, IDC_SIZENWSE));
    }
    // 四条边
    else if (x < kResizeMargin) {
        SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
    } else if (x >= width - kResizeMargin) {
        SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
    } else if (y < kResizeMargin) {
        SetCursor(LoadCursorW(nullptr, IDC_SIZENS));
    } else if (y >= height - kResizeMargin) {
        SetCursor(LoadCursorW(nullptr, IDC_SIZENS));
    } else {
        SetCursor(LoadCursorW(nullptr, IDC_ARROW));
    }
}

LRESULT AppWindow::HandleMessage(UINT message, WPARAM w_param, LPARAM l_param) {
    switch (message) {
    case WM_NCHITTEST: {
        int x = GET_X_LPARAM(l_param);
        int y = GET_Y_LPARAM(l_param);
        RECT rect;
        GetWindowRect(hwnd_, &rect);
        x -= rect.left;
        y -= rect.top;

        HandleNCHitTest(hwnd_, x, y);

        int width = rect.right - rect.left;
        int height = rect.bottom - rect.top;

        // 标题栏拖拽（优先级最高，在边框检查之前）
        if (IsInTitleBar(y)) {
            return HTCAPTION;
        }

        // 四个角落
        if (x < kResizeMargin && y < kResizeMargin) {
            return HTTOPLEFT;
        } else if (x >= width - kResizeMargin && y < kResizeMargin) {
            return HTTOPRIGHT;
        } else if (x < kResizeMargin && y >= height - kResizeMargin) {
            return HTBOTTOMLEFT;
        } else if (x >= width - kResizeMargin && y >= height - kResizeMargin) {
            return HTBOTTOMRIGHT;
        }
        // 四条边
        else if (x < kResizeMargin) {
            return HTLEFT;
        } else if (x >= width - kResizeMargin) {
            return HTRIGHT;
        } else if (y < kResizeMargin) {
            return HTTOP;
        } else if (y >= height - kResizeMargin) {
            return HTBOTTOM;
        }

        return HTCLIENT;
    }
    case WM_SIZE:
        if (webview_host_) {
            webview_host_->ResizeToClientArea();
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd_, message, w_param, l_param);
    }
}

} // namespace MCDevConsole
