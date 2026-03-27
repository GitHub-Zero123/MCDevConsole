#include "MCDevConsole/WebViewHost.h"
#include "MCDevConsole/AppWindow.h"
 
#include <shellapi.h>
#include <wrl/event.h>
#include <windows.h>
#include <string>
#include <vector>

namespace MCDevConsole {
namespace {

std::wstring ExtractJsonStringField(const std::wstring& json, const wchar_t* key) {
    const std::wstring needle = std::wstring(L"\"") + key + L"\":\"";
    const std::size_t start = json.find(needle);
    if (start == std::wstring::npos) {
        return {};
    }

    std::wstring result;
    bool escaping = false;
    for (std::size_t i = start + needle.size(); i < json.size(); ++i) {
        const wchar_t ch = json[i];
        if (escaping) {
            switch (ch) {
            case L'n': result.push_back(L'\n'); break;
            case L'r': result.push_back(L'\r'); break;
            case L't': result.push_back(L'\t'); break;
            case L'\\': result.push_back(L'\\'); break;
            case L'"': result.push_back(L'"'); break;
            default: result.push_back(ch); break;
            }
            escaping = false;
            continue;
        }

        if (ch == L'\\') {
            escaping = true;
            continue;
        }

        if (ch == L'"') {
            break;
        }

        result.push_back(ch);
    }
    return result;
}

std::string WideToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }

    const int required = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (required <= 0) {
        return {};
    }

    std::string result(static_cast<std::size_t>(required), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), &result[0], required, nullptr, nullptr);
    return result;
}

} // namespace

void WebViewHost::SetNetworkServer(NetworkServer* network_server) noexcept {
    network_server_ = network_server;
}

bool WebViewHost::Initialize(HWND parent_window) {
    parent_window_ = parent_window;

    auto on_webview_created = [this](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
        if (FAILED(result) || controller == nullptr) {
            return result;
        }

        controller_ = controller;
        controller_->get_CoreWebView2(&webview_);

        if (webview_) {
            // 启用非客户区支持，让 CSS -webkit-app-region:drag 生效
            Microsoft::WRL::ComPtr<ICoreWebView2Settings> settings;
            if (SUCCEEDED(webview_->get_Settings(&settings))) {
                Microsoft::WRL::ComPtr<ICoreWebView2Settings9> settings9;
                if (SUCCEEDED(settings.As(&settings9))) {
                    settings9->put_IsNonClientRegionSupportEnabled(TRUE);
                }
            }

            // 设置 WebView2 背景色为深色，防止导航期间白屏闪烁，并让四角露出像素跟主题
            {
                Microsoft::WRL::ComPtr<ICoreWebView2Controller2> ctrl2;
                if (SUCCEEDED(controller_.As(&ctrl2))) {
                    COREWEBVIEW2_COLOR bg{ 255, 0x1a, 0x1a, 0x1a };
                    ctrl2->put_DefaultBackgroundColor(bg);
                }
            }

            ResizeToClientArea();
            
            // 注册前端消息监听器
            webview_->add_WebMessageReceived(
                Microsoft::WRL::Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                    [this](ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                        wchar_t* message_raw = nullptr;
                        args->get_WebMessageAsJson(&message_raw);

                        if (message_raw) {
                            std::wstring message(message_raw);
                            CoTaskMemFree(message_raw);

                            if (message.find(L"\"kind\":\"window.command\"") != std::wstring::npos) {
                                if (parent_window_ != nullptr) {
                                    if (message.find(L"\"command\":\"minimize\"") != std::wstring::npos) {
                                        SendMessageW(parent_window_, WM_SYSCOMMAND, SC_MINIMIZE, 0);
                                    } else if (message.find(L"\"command\":\"toggle-maximize\"") != std::wstring::npos) {
                                        SendMessageW(parent_window_, WM_SYSCOMMAND,
                                            IsZoomed(parent_window_) ? SC_RESTORE : SC_MAXIMIZE, 0);
                                    } else if (message.find(L"\"command\":\"close\"") != std::wstring::npos) {
                                        SendMessageW(parent_window_, WM_SYSCOMMAND, SC_CLOSE, 0);
                                    } else if (message.find(L"\"command\":\"drag-start\"") != std::wstring::npos) {
                                        ReleaseCapture();
                                        SendMessageW(parent_window_, WM_NCLBUTTONDOWN, HTCAPTION, 0);
                                    }
                                }

                                std::wstring response = L"{\"kind\":\"window.state\",\"maximized\":";
                                response += (parent_window_ != nullptr && IsZoomed(parent_window_)) ? L"true" : L"false";
                                response += L"}";
                                PostJsonMessage(response);
                                return S_OK;
                            }

                            if (message.find(L"\"kind\":\"window.theme\"") != std::wstring::npos) {
                                // 深色主题 --panel: #1a1a1a，浅色主题 --panel: #ffffff
                                COLORREF color = RGB(0x1a, 0x1a, 0x1a);
                                if (message.find(L"\"theme\":\"light\"") != std::wstring::npos) {
                                    color = RGB(0xff, 0xff, 0xff);
                                }

                                SetBackgroundColor(color);

                                if (parent_window_ != nullptr) {
                                    PostMessageW(parent_window_, AppWindow::kMessageSetTitleBarColor, 0, static_cast<LPARAM>(color));
                                }
                                return S_OK;
                            }

                            if (message.find(L"\"kind\":\"external.open\"") != std::wstring::npos) {
                                size_t url_start = message.find(L"\"url\":\"");
                                if (url_start != std::wstring::npos) {
                                    url_start += 7;
                                    size_t url_end = message.find(L"\"", url_start);
                                    if (url_end != std::wstring::npos) {
                                        std::wstring url = message.substr(url_start, url_end - url_start);
                                        ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                                    }
                                }
                                return S_OK;
                            }

                            if (message.find(L"\"kind\":\"web.command\"") != std::wstring::npos
                                && message.find(L"\"kind\":\"exec.python\"") != std::wstring::npos) {
                                const std::wstring session_id = ExtractJsonStringField(message, L"sessionId");
                                const std::wstring target = ExtractJsonStringField(message, L"target");
                                const std::wstring code = ExtractJsonStringField(message, L"code");

                                if (network_server_ != nullptr && !session_id.empty() && !code.empty()) {
                                    const std::uint16_t type_id =
                                        target == L"server" ? Protocol::TYPE_EXEC_SERVER : Protocol::TYPE_EXEC_CLIENT;
                                    network_server_->SendExecCommand(
                                        WideToUtf8(session_id),
                                        type_id,
                                        WideToUtf8(code));
                                }
                                return S_OK;
                            }

                            if (message.find(L"\"kind\":\"web.ready\"") != std::wstring::npos) {
                                if (network_server_ != nullptr) {
                                    network_server_->ReplaySessionsToFrontend();
                                }
                                return S_OK;
                            }
                        }

                        return S_OK;
                    }).Get(),
                nullptr);
            
#ifdef _DEBUG
            webview_->Navigate(L"http://localhost:5173");
#else
            wchar_t exe_path[MAX_PATH]{};
            GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
            
            std::wstring html_path = exe_path;
            size_t last_slash = html_path.find_last_of(L"\\/");
            if (last_slash != std::wstring::npos) {
                html_path = html_path.substr(0, last_slash);
            }
            html_path += L"\\web\\index.html";
            
            NavigateToFile(html_path);
#endif

            // 导航完成后发送握手消息
            webview_->add_NavigationCompleted(
                Microsoft::WRL::Callback<ICoreWebView2NavigationCompletedEventHandler>(
                    [this](ICoreWebView2* sender, ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT {
                        BOOL success = FALSE;
                        args->get_IsSuccess(&success);
                        
                        if (success) {
                            std::wstring hello = L"{\"kind\":\"hello\",\"message\":\"MCDevConsole Host Ready\"}";
                            PostJsonMessage(hello);
                        }
                        
                        return S_OK;
                    }).Get(),
                nullptr);
        }

        return S_OK;
    };

    auto callback = Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
        [on_webview_created](HRESULT result, ICoreWebView2Controller* controller) {
            return on_webview_created(result, controller);
        });

    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, nullptr, nullptr,
        Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this, callback](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                if (FAILED(result) || env == nullptr) {
                    return result;
                }

                return env->CreateCoreWebView2Controller(parent_window_, callback.Get());
            }).Get());

    return SUCCEEDED(hr);
}

void WebViewHost::ResizeToClientArea() const {
    if (!controller_ || parent_window_ == nullptr) {
        return;
    }

    RECT bounds{};
    GetClientRect(parent_window_, &bounds);
    
    // 给四边留出 6px 缩放热区，避免 WebView2 吃掉边框事件
    constexpr int INSET = 6;
    bounds.left   += INSET;
    bounds.top    += INSET;
    bounds.right  -= INSET;
    bounds.bottom -= INSET;
    
    controller_->put_Bounds(bounds);
}

void WebViewHost::NavigateToString(const std::wstring& html) const {
    if (!webview_) {
        return;
    }

    webview_->NavigateToString(html.c_str());
}

void WebViewHost::NavigateToFile(const std::wstring& file_path) const {
    if (!webview_) {
        return;
    }

    std::wstring uri = L"file:///" + file_path;
    for (auto& ch : uri) {
        if (ch == L'\\') {
            ch = L'/';
        }
    }

    webview_->Navigate(uri.c_str());
}

void WebViewHost::PostJsonMessage(const std::wstring& json) const {
    if (parent_window_ == nullptr) {
        return;
    }

    auto* payload = new std::wstring(json);
    if (!PostMessageW(parent_window_, AppWindow::kMessageDispatchWebMessage, 0, reinterpret_cast<LPARAM>(payload))) {
        delete payload;
    }
}

void WebViewHost::PostJsonMessageNow(const std::wstring& json) const {
    if (!webview_) {
        return;
    }

    webview_->PostWebMessageAsJson(json.c_str());
}

void WebViewHost::SetBackgroundColor(COLORREF color) const {
    if (!controller_) {
        return;
    }

    Microsoft::WRL::ComPtr<ICoreWebView2Controller2> ctrl2;
    if (SUCCEEDED(controller_.As(&ctrl2))) {
        COREWEBVIEW2_COLOR bg{ 255, GetRValue(color), GetGValue(color), GetBValue(color) };
        ctrl2->put_DefaultBackgroundColor(bg);
    }
}

bool WebViewHost::IsReady() const noexcept {
    return webview_ != nullptr && controller_ != nullptr;
}

} // namespace MCDevConsole
