#pragma once

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/rect2.hpp>

#include <windows.h>
#include <wrl.h>
#include <wil/com.h>
#include "WebView2.h"

using namespace godot;
using namespace Microsoft::WRL;

class WebViewWindow : public Control {
    GDCLASS(WebViewWindow, Control)

private:
    // WebView2 core objects
    wil::com_ptr<ICoreWebView2Environment>  webview_env;
    wil::com_ptr<ICoreWebView2Controller>   webview_controller;
    wil::com_ptr<ICoreWebView2>             webview;

    // State
    bool    is_ready       = false;
    bool    is_visible_wv  = false;
    String  pending_url    = "";
    String  pending_file   = "";
    HWND    godot_hwnd     = nullptr;

    // Internal helpers
    void    _sync_bounds();
    void    _init_webview2();
    void    _inject_bridge();
    String  _res_to_abs(const String& res_path);

    // WebView2 callbacks (static → instance dispatch)
    static void _on_env_created(WebViewWindow* self,
                                HRESULT hr,
                                ICoreWebView2Environment* env);
    static void _on_controller_created(WebViewWindow* self,
                                       HRESULT hr,
                                       ICoreWebView2Controller* ctrl);

protected:
    static void _bind_methods();

public:
    WebViewWindow();
    ~WebViewWindow();

    // ── GDScript API ──────────────────────────────────────
    void    load_url(const String& url);
    void    load_file(const String& res_path);
    void    send_message(const String& event, const Dictionary& data);
    void    show_webview();
    void    hide_webview();
    void    set_webview_size(const Vector2& size);
    bool    get_is_ready() const { return is_ready; }

    // Godot lifecycle
    void    _ready()                          override;
    void    _process(double delta)            override;
    void    _notification(int what);
};
