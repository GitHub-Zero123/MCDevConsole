#include "MCDevConsole/AppWindow.h"

#include <windowsx.h>
#include <dwmapi.h>

#pragma comment(lib, "dwmapi.lib")

namespace MCDevConsole {

bool AppWindow::Create(HINSTANCE instance, int show_command) {
    instance_ = instance;

    if (!RegisterWindowClass()) {
        return false;
    }

    // 基于系统工作区居中，避免覆盖任务栏区域
    RECT work_area{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work_area, 0);
    int work_width = work_area.right - work_area.left;
    int work_height = work_area.bottom - work_area.top;
    int window_x = work_area.left + (work_width - kInitialWidth) / 2;
    int window_y = work_area.top + (work_height - kInitialHeight) / 2;

    hwnd_ = CreateWindowExW(
        0,
        kWindowClassName,
        kWindowTitle,
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        window_x,
        window_y,
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

    // 扩展框架到整个客户区，隐藏原生标题栏但保留边框和动画
    MARGINS margins{ -1, -1, -1, -1 };
    DwmExtendFrameIntoClientArea(hwnd_, &margins);

    // 设置初始 DWM 深色模式属性（深色主题）
    // DWMWA_USE_IMMERSIVE_DARK_MODE = 20
    BOOL use_dark = TRUE;
    DwmSetWindowAttribute(hwnd_, 20, &use_dark, sizeof(use_dark));

    // 强制刷新窗口框架，立即应用 DWM 扩展效果
    SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

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
    
    // 初始化实例级背景刷
    background_brush_ = CreateSolidBrush(titlebar_color_);
    
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

void AppWindow::SetTitleBarColor(COLORREF color) noexcept {
    titlebar_color_ = color;
    if (hwnd_ != nullptr) {
        HBRUSH new_brush = CreateSolidBrush(color);
        HBRUSH old_brush = reinterpret_cast<HBRUSH>(
            SetClassLongPtrW(hwnd_, GCLP_HBRBACKGROUND,
                             reinterpret_cast<LONG_PTR>(new_brush)));
        
        if (old_brush && old_brush != background_brush_) {
            DeleteObject(old_brush);
        }
        
        if (background_brush_) {
            DeleteObject(background_brush_);
        }
        background_brush_ = new_brush;
        
        // 根据颜色判断是否为深色主题（#1a1a1a 深色，#ffffff 浅色）
        bool is_dark = (color == RGB(0x1a, 0x1a, 0x1a));
        
        // 设置 DWM 深色模式属性，让系统边框和标题栏跟随主题
        // DWMWA_USE_IMMERSIVE_DARK_MODE = 20
        BOOL use_dark = is_dark ? TRUE : FALSE;
        DwmSetWindowAttribute(hwnd_, 20, &use_dark, sizeof(use_dark));
        
        SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    }
}

LRESULT AppWindow::HandleMessage(UINT message, WPARAM w_param, LPARAM l_param) {
    switch (message) {
    case WM_ERASEBKGND: {
        if (background_brush_) {
            HDC hdc = reinterpret_cast<HDC>(w_param);
            RECT rect;
            GetClientRect(hwnd_, &rect);
            FillRect(hdc, &rect, background_brush_);
            return 1;
        }
        break;
    }
    case WM_NCCALCSIZE: {
        if (w_param) {
            auto* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(l_param);
            auto& rect = params->rgrc[0];

            // 最大化时需要补回系统预留的边框宽度，防止遮住任务栏
            if (IsZoomed(hwnd_)) {
                int frame_x = GetSystemMetrics(SM_CXFRAME) +
                              GetSystemMetrics(SM_CXPADDEDBORDER);
                int frame_y = GetSystemMetrics(SM_CYFRAME) +
                              GetSystemMetrics(SM_CXPADDEDBORDER);
                rect.left   += frame_x;
                rect.right  -= frame_x;
                rect.top    += frame_y;
                rect.bottom -= frame_y;
            } else {
                // 普通窗口：仅移除顶部标题栏，保留1px给DWM圆角
                rect.top += 1;
            }
            return 0;
        }
        return DefWindowProcW(hwnd_, message, w_param, l_param);
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd_, &ps);

        RECT client_rect;
        GetClientRect(hwnd_, &client_rect);

        HBRUSH brush = CreateSolidBrush(titlebar_color_);
        FillRect(hdc, &client_rect, brush);
        DeleteObject(brush);

        EndPaint(hwnd_, &ps);
        return 0;
    }
    case WM_GETMINMAXINFO: {
        // 限制最大化窗口不覆盖任务栏
        auto* mmi = reinterpret_cast<MINMAXINFO*>(l_param);
        HMONITOR monitor = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
        if (monitor) {
            MONITORINFO monitor_info{};
            monitor_info.cbSize = sizeof(monitor_info);
            if (GetMonitorInfoW(monitor, &monitor_info)) {
                mmi->ptMaxPosition.x = monitor_info.rcWork.left - monitor_info.rcMonitor.left;
                mmi->ptMaxPosition.y = monitor_info.rcWork.top - monitor_info.rcMonitor.top;
                mmi->ptMaxSize.x = monitor_info.rcWork.right - monitor_info.rcWork.left;
                mmi->ptMaxSize.y = monitor_info.rcWork.bottom - monitor_info.rcWork.top;
            }
        }
        return 0;
    }
    case kMessageSetTitleBarColor:
        SetTitleBarColor(static_cast<COLORREF>(l_param));
        return 0;
    case WM_NCHITTEST: {
        // 最大化状态下不需要边框缩放
        if (IsZoomed(hwnd_)) {
            return DefWindowProcW(hwnd_, message, w_param, l_param);
        }

        RECT rc{};
        GetWindowRect(hwnd_, &rc);
        int mx = GET_X_LPARAM(l_param);
        int my = GET_Y_LPARAM(l_param);

        // 边框感应宽度（物理像素）
        constexpr int BORDER = 8;

        bool onLeft   = mx <  rc.left   + BORDER;
        bool onRight  = mx >  rc.right  - BORDER;
        bool onTop    = my <  rc.top    + BORDER;
        bool onBottom = my >  rc.bottom - BORDER;

        // 四角
        if (onTop && onLeft)     return HTTOPLEFT;
        if (onTop && onRight)    return HTTOPRIGHT;
        if (onBottom && onLeft)  return HTBOTTOMLEFT;
        if (onBottom && onRight) return HTBOTTOMRIGHT;
        // 单边
        if (onLeft)              return HTLEFT;
        if (onRight)             return HTRIGHT;
        if (onTop)               return HTTOP;
        if (onBottom)            return HTBOTTOM;

        // 其余交给 DefWindowProc（拖拽区域等）
        return DefWindowProcW(hwnd_, message, w_param, l_param);
    }
    case WM_NCACTIVATE:
        // 阻止系统绘制非客户区
        return DefWindowProcW(hwnd_, message, w_param, -1);
    case WM_NCLBUTTONDBLCLK: {
        // 标题栏双击最大化/还原
        if (w_param == HTCAPTION) {
            SendMessageW(hwnd_, WM_SYSCOMMAND,
                IsZoomed(hwnd_) ? SC_RESTORE : SC_MAXIMIZE, 0);
            return 0;
        }
        break;
    }
    case WM_SIZE:
        if (webview_host_) {
            webview_host_->ResizeToClientArea();
        }
        // 回发窗口状态给前端，更新最大化按钮
        if (webview_host_) {
            std::wstring state = L"{\"kind\":\"window.state\",\"maximized\":";
            state += IsZoomed(hwnd_) ? L"true" : L"false";
            state += L"}";
            webview_host_->PostJsonMessage(state);
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
