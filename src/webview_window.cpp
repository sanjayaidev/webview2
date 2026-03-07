#include "webview_window.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/project_settings.hpp>

#include <shlwapi.h>
#include <string>

#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "ole32.lib")

using namespace godot;

// ── String helpers ────────────────────────────────────────────────────────────

static std::wstring to_wstr(const String& s) {
    const char* utf8 = s.utf8().get_data();
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
    if (len <= 0) return L"";
    std::wstring out(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, out.data(), len);
    return out;
}

static String from_wstr(const wchar_t* ws) {
    if (!ws) return String();
    int len = WideCharToMultiByte(CP_UTF8, 0, ws, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) return String();
    std::string out(len, 0);
    WideCharToMultiByte(CP_UTF8, 0, ws, -1, out.data(), len, nullptr, nullptr);
    return String(out.c_str());
}

// ── _bind_methods ─────────────────────────────────────────────────────────────

void WebViewWindow::_bind_methods() {
    ClassDB::bind_method(D_METHOD("load_url", "url"),              &WebViewWindow::load_url);
    ClassDB::bind_method(D_METHOD("load_file", "res_path"),        &WebViewWindow::load_file);
    ClassDB::bind_method(D_METHOD("send_message", "event","data"), &WebViewWindow::send_message);
    ClassDB::bind_method(D_METHOD("show_webview"),                 &WebViewWindow::show_webview);
    ClassDB::bind_method(D_METHOD("hide_webview"),                 &WebViewWindow::hide_webview);
    ClassDB::bind_method(D_METHOD("set_webview_size", "size"),     &WebViewWindow::set_webview_size);
    ClassDB::bind_method(D_METHOD("get_is_ready"),                 &WebViewWindow::get_is_ready);

    ADD_SIGNAL(MethodInfo("message_received",
        PropertyInfo(Variant::STRING,     "event"),
        PropertyInfo(Variant::DICTIONARY, "data")));
    ADD_SIGNAL(MethodInfo("page_loaded"));
    ADD_SIGNAL(MethodInfo("page_error",
        PropertyInfo(Variant::STRING, "message")));
    ADD_SIGNAL(MethodInfo("webview_ready"));
}

// ── Constructor / Destructor ──────────────────────────────────────────────────

WebViewWindow::WebViewWindow() {}

WebViewWindow::~WebViewWindow() {
    if (webview_controller) {
        webview_controller->Close();
        webview_controller = nullptr;
    }
}

// ── Godot lifecycle ───────────────────────────────────────────────────────────

void WebViewWindow::_ready() {
    if (Engine::get_singleton()->is_editor_hint()) return;

    int64_t handle = DisplayServer::get_singleton()->window_get_native_handle(
        DisplayServer::WINDOW_HANDLE, 0);
    godot_hwnd = reinterpret_cast<HWND>(static_cast<intptr_t>(handle));

    if (!godot_hwnd) {
        UtilityFunctions::printerr("[WebView] Failed to get native HWND");
        return;
    }
    _init_webview2();
}

void WebViewWindow::_process(double delta) {
    if (!is_ready || !webview_controller) return;
    _sync_bounds();
}

void WebViewWindow::_notification(int what) {
    if (what == NOTIFICATION_RESIZED || what == NOTIFICATION_TRANSFORM_CHANGED) {
        if (is_ready && webview_controller) _sync_bounds();
    }
    if (what == NOTIFICATION_PREDELETE) {
        if (webview_controller) {
            webview_controller->Close();
            webview_controller = nullptr;
        }
    }
}

// ── WebView2 init ─────────────────────────────────────────────────────────────

void WebViewWindow::_init_webview2() {
    String exe_dir = OS::get_singleton()->get_executable_path().get_base_dir();
    std::wstring user_data = to_wstr(exe_dir + "/webview2_data");

    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr,
        user_data.c_str(),
        nullptr,
        Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                WebViewWindow::_on_env_created(this, result, env);
                return S_OK;
            }).Get()
    );

    if (FAILED(hr)) {
        UtilityFunctions::printerr("[WebView] CreateCoreWebView2EnvironmentWithOptions failed: ", (int64_t)hr);
    }
}

void WebViewWindow::_on_env_created(WebViewWindow* self, HRESULT hr, ICoreWebView2Environment* env) {
    if (FAILED(hr) || !env) {
        UtilityFunctions::printerr("[WebView] Env creation failed: ", (int64_t)hr);
        self->call_deferred("emit_signal", "page_error", String("WebView2 env failed"));
        return;
    }
    self->webview_env = env;
    env->CreateCoreWebView2Controller(
        self->godot_hwnd,
        Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
            [self](HRESULT result, ICoreWebView2Controller* ctrl) -> HRESULT {
                WebViewWindow::_on_controller_created(self, result, ctrl);
                return S_OK;
            }).Get()
    );
}

void WebViewWindow::_on_controller_created(WebViewWindow* self, HRESULT hr, ICoreWebView2Controller* ctrl) {
    if (FAILED(hr) || !ctrl) {
        UtilityFunctions::printerr("[WebView] Controller creation failed: ", (int64_t)hr);
        self->call_deferred("emit_signal", "page_error", String("WebView2 controller failed"));
        return;
    }

    self->webview_controller = ctrl;
    ctrl->get_CoreWebView2(self->webview.put());

    // Settings
    wil::com_ptr<ICoreWebView2Settings> settings;
    self->webview->get_Settings(&settings);
    if (settings) {
        settings->put_IsScriptEnabled(TRUE);
        settings->put_AreDefaultScriptDialogsEnabled(TRUE);
        settings->put_IsWebMessageEnabled(TRUE);
        settings->put_IsStatusBarEnabled(FALSE);
        settings->put_AreDevToolsEnabled(TRUE);
    }

    // Auto-allow mic/camera
    wil::com_ptr<ICoreWebView2_11> webview11;
    if (SUCCEEDED(self->webview->QueryInterface(IID_PPV_ARGS(&webview11)))) {
        webview11->add_PermissionRequested(
            Microsoft::WRL::Callback<ICoreWebView2PermissionRequestedEventHandler>(
                [](ICoreWebView2* sender, ICoreWebView2PermissionRequestedEventArgs* args) -> HRESULT {
                    COREWEBVIEW2_PERMISSION_KIND kind = COREWEBVIEW2_PERMISSION_KIND_UNKNOWN_PERMISSION;
                    args->get_PermissionKind(&kind);
                    if (kind == COREWEBVIEW2_PERMISSION_KIND_MICROPHONE ||
                        kind == COREWEBVIEW2_PERMISSION_KIND_CAMERA     ||
                        kind == COREWEBVIEW2_PERMISSION_KIND_GEOLOCATION) {
                        args->put_State(COREWEBVIEW2_PERMISSION_STATE_ALLOW);
                    }
                    return S_OK;
                }).Get(),
            nullptr
        );
    }

    // JS bridge
    self->webview->add_WebMessageReceived(
        Microsoft::WRL::Callback<ICoreWebView2WebMessageReceivedEventHandler>(
            [self](ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                LPWSTR raw = nullptr;
                if (FAILED(args->TryGetWebMessageAsString(&raw)) || !raw) return S_OK;
                String json_str = from_wstr(raw);
                CoTaskMemFree(raw);

                Variant parsed = JSON::parse_string(json_str);
                if (parsed.get_type() != Variant::DICTIONARY) return S_OK;

                Dictionary msg  = parsed;
                String event    = msg.get("event", String());
                Dictionary data = msg.get("data",  Dictionary());
                self->call_deferred("emit_signal", "message_received", event, data);
                return S_OK;
            }).Get(),
        nullptr
    );

    // Navigation completed
    self->webview->add_NavigationCompleted(
        Microsoft::WRL::Callback<ICoreWebView2NavigationCompletedEventHandler>(
            [self](ICoreWebView2* sender, ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT {
                BOOL success = FALSE;
                args->get_IsSuccess(&success);
                if (success) {
                    self->_inject_bridge();
                    self->call_deferred("emit_signal", "page_loaded");
                } else {
                    COREWEBVIEW2_WEB_ERROR_STATUS status = COREWEBVIEW2_WEB_ERROR_STATUS_UNKNOWN;
                    args->get_WebErrorStatus(&status);
                    self->call_deferred("emit_signal", "page_error",
                        String("Navigation failed: ") + String::num_int64((int64_t)status));
                }
                return S_OK;
            }).Get(),
        nullptr
    );

    self->_sync_bounds();
    self->webview_controller->put_IsVisible(self->is_visible_wv ? TRUE : FALSE);
    self->is_ready = true;
    self->call_deferred("emit_signal", "webview_ready");

    if (!self->pending_url.is_empty()) {
        String url = self->pending_url; self->pending_url = "";
        self->load_url(url);
    } else if (!self->pending_file.is_empty()) {
        String f = self->pending_file; self->pending_file = "";
        self->load_file(f);
    }
}

// ── Sync bounds ───────────────────────────────────────────────────────────────

void WebViewWindow::_sync_bounds() {
    if (!webview_controller) return;
    Rect2 rect = get_global_rect();
    RECT r;
    r.left   = (LONG)rect.position.x;
    r.top    = (LONG)rect.position.y;
    r.right  = (LONG)(rect.position.x + rect.size.x);
    r.bottom = (LONG)(rect.position.y + rect.size.y);
    webview_controller->put_Bounds(r);
}

// ── Inject JS bridge ──────────────────────────────────────────────────────────

void WebViewWindow::_inject_bridge() {
    const wchar_t* js = LR"JS(
        (function() {
            if (window.__godot_bridge_ready) return;
            window.__godot_bridge_ready = true;
            var _L = {};
            window.godot = {
                send: function(e, d) {
                    window.chrome.webview.postMessage(JSON.stringify({event:e, data:d||{}}));
                },
                _dispatch: function(e, d) {
                    (_L[e]||[]).forEach(function(f){ try{f(d);}catch(ex){} });
                },
                on:  function(e,f){ if(!_L[e])_L[e]=[]; _L[e].push(f); },
                off: function(e,f){ if(_L[e]) _L[e]=_L[e].filter(function(x){return x!==f;}); }
            };
            console.log('[GodotBridge] Ready');
        })();
    )JS";
    webview->ExecuteScript(js, nullptr);
}

// ── Public API ────────────────────────────────────────────────────────────────

void WebViewWindow::load_url(const String& url) {
    if (!is_ready) { pending_url = url; return; }
    webview->Navigate(to_wstr(url).c_str());
}

void WebViewWindow::load_file(const String& res_path) {
    if (!is_ready) { pending_file = res_path; return; }
    String abs      = ProjectSettings::get_singleton()->globalize_path(res_path);
    String file_url = "file:///" + abs.replace("\\", "/");
    webview->Navigate(to_wstr(file_url).c_str());
}

void WebViewWindow::send_message(const String& event, const Dictionary& data) {
    if (!is_ready || !webview) return;
    String js = "if(window.godot)window.godot._dispatch("
              + JSON::stringify(event) + ","
              + JSON::stringify(data)  + ");";
    webview->ExecuteScript(to_wstr(js).c_str(), nullptr);
}

void WebViewWindow::show_webview() {
    is_visible_wv = true;
    if (webview_controller) webview_controller->put_IsVisible(TRUE);
}

void WebViewWindow::hide_webview() {
    is_visible_wv = false;
    if (webview_controller) webview_controller->put_IsVisible(FALSE);
}

void WebViewWindow::set_webview_size(const Vector2& size) {
    set_size(size);
    _sync_bounds();
}
