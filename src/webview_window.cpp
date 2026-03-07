#include "webview_window.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/json.hpp>

#include <shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "ole32.lib")

// ── Helpers ──────────────────────────────────────────────────────────────────

static std::wstring to_wstr(const godot::String& s) {
    const char* utf8 = s.utf8().get_data();
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
    std::wstring out(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, out.data(), len);
    return out;
}

static godot::String from_wstr(const std::wstring& ws) {
    int len = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string out(len, 0);
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, out.data(), len, nullptr, nullptr);
    return godot::String(out.c_str());
}

// ── _bind_methods ─────────────────────────────────────────────────────────────

void WebViewWindow::_bind_methods() {
    ClassDB::bind_method(D_METHOD("load_url",          "url"),        &WebViewWindow::load_url);
    ClassDB::bind_method(D_METHOD("load_file",         "res_path"),   &WebViewWindow::load_file);
    ClassDB::bind_method(D_METHOD("send_message",      "event","data"),&WebViewWindow::send_message);
    ClassDB::bind_method(D_METHOD("show_webview"),                    &WebViewWindow::show_webview);
    ClassDB::bind_method(D_METHOD("hide_webview"),                    &WebViewWindow::hide_webview);
    ClassDB::bind_method(D_METHOD("set_webview_size",  "size"),       &WebViewWindow::set_webview_size);
    ClassDB::bind_method(D_METHOD("get_is_ready"),                    &WebViewWindow::get_is_ready);

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
    }
}

// ── Godot Lifecycle ───────────────────────────────────────────────────────────

void WebViewWindow::_ready() {
    if (Engine::get_singleton()->is_editor_hint()) return;

    // Get the native HWND from Godot's DisplayServer
    godot_hwnd = (HWND)(INT_PTR)DisplayServer::get_singleton()->window_get_native_handle(
        DisplayServer::HANDLE_TYPE_WINDOW_HANDLE, 0);

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
        if (is_ready && webview_controller) {
            _sync_bounds();
        }
    }
    if (what == NOTIFICATION_PREDELETE) {
        if (webview_controller) {
            webview_controller->Close();
        }
    }
}

// ── WebView2 Init ─────────────────────────────────────────────────────────────

void WebViewWindow::_init_webview2() {
    // User data folder next to the executable
    std::wstring user_data = to_wstr(OS::get_singleton()->get_executable_path().get_base_dir() + "/webview2_data");

    CreateCoreWebView2EnvironmentWithOptions(
        nullptr,
        user_data.c_str(),
        nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this](HRESULT hr, ICoreWebView2Environment* env) -> HRESULT {
                _on_env_created(this, hr, env);
                return S_OK;
            }).Get()
    );
}

void WebViewWindow::_on_env_created(WebViewWindow* self, HRESULT hr, ICoreWebView2Environment* env) {
    if (FAILED(hr) || !env) {
        UtilityFunctions::printerr("[WebView] Environment creation failed: ", (int)hr);
        self->emit_signal("page_error", "WebView2 environment creation failed");
        return;
    }

    self->webview_env = env;

    env->CreateCoreWebView2Controller(
        self->godot_hwnd,
        Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
            [self](HRESULT hr, ICoreWebView2Controller* ctrl) -> HRESULT {
                _on_controller_created(self, hr, ctrl);
                return S_OK;
            }).Get()
    );
}

void WebViewWindow::_on_controller_created(WebViewWindow* self, HRESULT hr, ICoreWebView2Controller* ctrl) {
    if (FAILED(hr) || !ctrl) {
        UtilityFunctions::printerr("[WebView] Controller creation failed: ", (int)hr);
        self->emit_signal("page_error", "WebView2 controller creation failed");
        return;
    }

    self->webview_controller = ctrl;
    ctrl->get_CoreWebView2(&self->webview);

    // ── Settings ──
    wil::com_ptr<ICoreWebView2Settings> settings;
    self->webview->get_Settings(&settings);
    settings->put_IsScriptEnabled(TRUE);
    settings->put_AreDefaultScriptDialogsEnabled(TRUE);
    settings->put_IsWebMessageEnabled(TRUE);
    settings->put_IsStatusBarEnabled(FALSE);
    settings->put_AreDevToolsEnabled(TRUE); // set FALSE for release

    // ── Mic / Camera permissions ──
    wil::com_ptr<ICoreWebView2_11> webview11;
    if (SUCCEEDED(self->webview->QueryInterface(IID_PPV_ARGS(&webview11)))) {
        webview11->add_PermissionRequested(
            Callback<ICoreWebView2PermissionRequestedEventHandler>(
                [](ICoreWebView2* sender, ICoreWebView2PermissionRequestedEventArgs* args) -> HRESULT {
                    COREWEBVIEW2_PERMISSION_KIND kind;
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

    // ── Web Message (JS → Godot bridge) ──
    self->webview->add_WebMessageReceived(
        Callback<ICoreWebView2WebMessageReceivedEventHandler>(
            [self](ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                LPWSTR raw = nullptr;
                args->TryGetWebMessageAsString(&raw);
                if (!raw) return S_OK;

                godot::String json_str = from_wstr(raw);
                CoTaskMemFree(raw);

                // Parse JSON → { event, data }
                godot::Ref<godot::JSON> json;
                json.instantiate();
                godot::Error err = json->parse(json_str);
                if (err != godot::OK) return S_OK;

                godot::Variant parsed = json->get_data();
                if (parsed.get_type() != godot::Variant::DICTIONARY) return S_OK;

                godot::Dictionary msg = parsed;
                godot::String event   = msg.get("event", "");
                godot::Dictionary data = msg.get("data", godot::Dictionary());

                self->emit_signal("message_received", event, data);
                return S_OK;
            }).Get(),
        nullptr
    );

    // ── Navigation completed ──
    self->webview->add_NavigationCompleted(
        Callback<ICoreWebView2NavigationCompletedEventHandler>(
            [self](ICoreWebView2* sender, ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT {
                BOOL success;
                args->get_IsSuccess(&success);
                if (success) {
                    self->_inject_bridge();
                    self->emit_signal("page_loaded");
                } else {
                    COREWEBVIEW2_WEB_ERROR_STATUS status;
                    args->get_WebErrorStatus(&status);
                    self->emit_signal("page_error", godot::String("Navigation failed: ") + godot::String::num_int64(status));
                }
                return S_OK;
            }).Get(),
        nullptr
    );

    // ── Initial bounds & visibility ──
    self->_sync_bounds();
    self->webview_controller->put_IsVisible(self->is_visible_wv);

    self->is_ready = true;
    self->emit_signal("webview_ready");

    // Load any pending URL/file
    if (!self->pending_url.is_empty()) {
        self->load_url(self->pending_url);
        self->pending_url = "";
    } else if (!self->pending_file.is_empty()) {
        self->load_file(self->pending_file);
        self->pending_file = "";
    }
}

// ── Sync Bounds (maps Control rect → WebView2 bounds) ────────────────────────

void WebViewWindow::_sync_bounds() {
    if (!webview_controller) return;

    godot::Rect2 rect = get_global_rect();
    RECT r;
    r.left   = (LONG)rect.position.x;
    r.top    = (LONG)rect.position.y;
    r.right  = (LONG)(rect.position.x + rect.size.x);
    r.bottom = (LONG)(rect.position.y + rect.size.y);

    webview_controller->put_Bounds(r);
}

// ── Inject JS Bridge ──────────────────────────────────────────────────────────

void WebViewWindow::_inject_bridge() {
    // Inject window.godot.send() and window.godot.on() into every page
    const char* bridge_js = R"JS(
        (function() {
            if (window.__godot_bridge_ready) return;
            window.__godot_bridge_ready = true;

            const _listeners = {};

            window.godot = {
                // Webpage → Godot
                send: function(event, data) {
                    const msg = JSON.stringify({ event: event, data: data || {} });
                    window.chrome.webview.postMessage(msg);
                },
                // Godot → Webpage (internal dispatch)
                _dispatch: function(event, data) {
                    const cbs = _listeners[event] || [];
                    cbs.forEach(cb => { try { cb(data); } catch(e) {} });
                },
                // Webpage registers listener for Godot messages
                on: function(event, callback) {
                    if (!_listeners[event]) _listeners[event] = [];
                    _listeners[event].push(callback);
                },
                off: function(event, callback) {
                    if (!_listeners[event]) return;
                    _listeners[event] = _listeners[event].filter(cb => cb !== callback);
                }
            };

            console.log('[GodotBridge] Ready');
        })();
    )JS";

    webview->ExecuteScript(to_wstr(godot::String(bridge_js)).c_str(), nullptr);
}

// ── Public API ────────────────────────────────────────────────────────────────

void WebViewWindow::load_url(const godot::String& url) {
    if (!is_ready) { pending_url = url; return; }
    webview->Navigate(to_wstr(url).c_str());
}

void WebViewWindow::load_file(const godot::String& res_path) {
    if (!is_ready) { pending_file = res_path; return; }
    godot::String abs = _res_to_abs(res_path);
    // Convert to file:/// URL
    godot::String file_url = "file:///" + abs.replace("\\", "/");
    webview->Navigate(to_wstr(file_url).c_str());
}

void WebViewWindow::send_message(const godot::String& event, const godot::Dictionary& data) {
    if (!is_ready || !webview) return;

    godot::Dictionary msg;
    msg["event"] = event;
    msg["data"]  = data;

    godot::String json_str = godot::JSON::stringify(msg);
    godot::String js = "if(window.godot) window.godot._dispatch(" +
                       godot::JSON::stringify(event) + "," +
                       godot::JSON::stringify(data)  + ");";

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

void WebViewWindow::set_webview_size(const godot::Vector2& size) {
    set_size(size);
    _sync_bounds();
}

godot::String WebViewWindow::_res_to_abs(const godot::String& res_path) {
    return godot::ProjectSettings::get_singleton()->globalize_path(res_path);
}
