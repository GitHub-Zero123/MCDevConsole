#include "MCDevConsole/WebViewHost.h"
#include "MCDevConsole/AppWindow.h"

#ifdef MCDEVCONSOLE_EMBED_WEB_RESOURCE
#include "MCDevConsole/WebResource.hpp"
#endif
 
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
                
#ifndef _DEBUG
                // Release 模式下禁用开发者工具
                settings->put_AreDevToolsEnabled(FALSE);
                settings->put_AreDefaultContextMenusEnabled(FALSE);
#endif
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
    #ifdef MCDEVCONSOLE_EMBED_WEB_RESOURCE
            // Release 模式：注册资源请求拦截器
            Microsoft::WRL::ComPtr<ICoreWebView2Environment> env_ptr;
            webview_->AddWebResourceRequestedFilter(L"*", COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
            webview_->add_WebResourceRequested(
                Microsoft::WRL::Callback<ICoreWebView2WebResourceRequestedEventHandler>(
                    [env_ptr](ICoreWebView2* sender, ICoreWebView2WebResourceRequestedEventArgs* args) mutable -> HRESULT {
                        Microsoft::WRL::ComPtr<ICoreWebView2WebResourceRequest> request;
                        args->get_Request(&request);
                        
                        wchar_t* uri_raw = nullptr;
                        request->get_Uri(&uri_raw);
                        if (!uri_raw) return S_OK;
                        
                        std::wstring uri(uri_raw);
                        CoTaskMemFree(uri_raw);
                        
                        // 提取路径部分
                        std::string path;
                        size_t scheme_end = uri.find(L"://");
                        if (scheme_end != std::wstring::npos) {
                            size_t path_start = uri.find(L'/', scheme_end + 3);
                            if (path_start != std::wstring::npos) {
                                std::wstring wpath = uri.substr(path_start);
                                path = WideToUtf8(wpath);
                            }
                        }
                        
                        if (path.empty()) path = "/index.html";
                        
                        // 查找资源
                        auto it = WebResource::resourceMap.find(path);
                        if (it == WebResource::resourceMap.end() || it->second.first == nullptr) {
                            return S_OK;
                        }
                        
                        // 创建内存流
                        Microsoft::WRL::ComPtr<IStream> stream;
                        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, it->second.second);
                        if (!hMem) return E_OUTOFMEMORY;
                        
                        void* pMem = GlobalLock(hMem);
                        if (pMem) {
                            memcpy(pMem, it->second.first, it->second.second);
                            GlobalUnlock(hMem);
                            CreateStreamOnHGlobal(hMem, TRUE, &stream);
                        }
                        
                        if (!stream) {
                            GlobalFree(hMem);
                            return E_FAIL;
                        }
                        
                        // 确定 MIME 类型
                        std::wstring mime = L"application/octet-stream";
                        if (path.size() >= 5 && path.substr(path.size() - 5) == ".html") mime = L"text/html";
                        else if (path.size() >= 3 && path.substr(path.size() - 3) == ".js") mime = L"application/javascript";
                        else if (path.size() >= 4 && path.substr(path.size() - 4) == ".css") mime = L"text/css";
                        else if (path.size() >= 5 && path.substr(path.size() - 5) == ".json") mime = L"application/json";
                        else if (path.size() >= 4 && path.substr(path.size() - 4) == ".png") mime = L"image/png";
                        else if (path.size() >= 4 && (path.substr(path.size() - 4) == ".jpg" || path.substr(path.size() - 5) == ".jpeg")) mime = L"image/jpeg";
                        else if (path.size() >= 4 && path.substr(path.size() - 4) == ".svg") mime = L"image/svg+xml";
                        
                        // 获取环境对象
                        if (!env_ptr) {
                            Microsoft::WRL::ComPtr<ICoreWebView2_2> webview2;
                            if (SUCCEEDED(sender->QueryInterface(IID_PPV_ARGS(&webview2)))) {
                                webview2->get_Environment(&env_ptr);
                            }
                        }
                        
                        if (env_ptr) {
                            Microsoft::WRL::ComPtr<ICoreWebView2WebResourceResponse> response;
                            env_ptr->CreateWebResourceResponse(stream.Get(), 200, L"OK",
                                (L"Content-Type: " + mime).c_str(), &response);
                            args->put_Response(response.Get());
                        }
                        
                        return S_OK;
                    }).Get(),
                nullptr);
            
            // 导航到虚拟 URL
            webview_->Navigate(L"https://mcdevconsole.local/index.html");
    #else
            // Fallback：使用文件夹映射
            wchar_t exe_path[MAX_PATH]{};
            GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
            
            std::wstring web_folder = exe_path;
            size_t last_slash = web_folder.find_last_of(L"\\/");
            if (last_slash != std::wstring::npos) {
                web_folder = web_folder.substr(0, last_slash);
            }
            web_folder += L"\\web";
            
            Microsoft::WRL::ComPtr<ICoreWebView2_3> webview3;
            if (SUCCEEDED(webview_.As(&webview3))) {
                webview3->SetVirtualHostNameToFolderMapping(
                    L"mcdevconsole.local",
                    web_folder.c_str(),
                    COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_DENY_CORS
                );
            }
            
            webview_->Navigate(L"https://mcdevconsole.local/index.html");
    #endif
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
