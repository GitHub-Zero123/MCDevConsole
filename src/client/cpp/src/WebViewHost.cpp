#include "MCDevConsole/WebViewHost.h"

#include <shellapi.h>
#include <wrl/event.h>
#include <windows.h>
#include <string>

namespace MCDevConsole {

bool WebViewHost::Initialize(HWND parent_window) {
    parent_window_ = parent_window;

    auto on_webview_created = [this](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
        if (FAILED(result) || controller == nullptr) {
            return result;
        }

        controller_ = controller;
        controller_->get_CoreWebView2(&webview_);

        if (webview_) {
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
                            
                            // 简单回显测试：收到前端消息后回传确认
                            std::wstring response = L"{\"kind\":\"host.echo\",\"received\":true,\"original\":";
                            response += message;
                            response += L"}";
                            PostJsonMessage(response);
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
    if (!webview_) {
        return;
    }

    webview_->PostWebMessageAsJson(json.c_str());
}

bool WebViewHost::IsReady() const noexcept {
    return webview_ != nullptr && controller_ != nullptr;
}

} // namespace MCDevConsole
