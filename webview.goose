// Microsoft Edge WebView2 bindings (src/win32_webview.c).
// Loads WebView2Loader.dll if present, or loads EmbeddedBrowserWebView.dll
// directly from the installed Edge WebView2 Runtime with zero external DLLs.
namespace win32;

import types;

// WebView2 event kinds produced by WebView.poll_event / webview_poll_event.
let WV_EV_NONE: u32 = 0;
let WV_EV_READY: u32 = 1;
let WV_EV_INIT_FAILED: u32 = 2;
let WV_EV_NAV_STARTING: u32 = 3;
let WV_EV_NAV_COMPLETED: u32 = 4;
let WV_EV_SOURCE_CHANGED: u32 = 5;
let WV_EV_TITLE_CHANGED: u32 = 6;
let WV_EV_HISTORY_CHANGED: u32 = 7;
let WV_EV_WEB_MESSAGE: u32 = 8;
let WV_EV_SCRIPT_RESULT: u32 = 9;
let WV_EV_NEW_WINDOW: u32 = 10;
let WV_EV_FOCUS_CHANGED: u32 = 11;

// NewWindowRequested handling policies.
let WV_NEWWIN_BLOCK: i32 = 0;
let WV_NEWWIN_NAVIGATE_SELF: i32 = 1;
let WV_NEWWIN_OPEN_EXTERNAL: i32 = 2;
let WV_NEWWIN_ALLOW_POPUP: i32 = 3;

// Preferred color scheme constants (COREWEBVIEW2_PREFERRED_COLOR_SCHEME).
let WV_SCHEME_AUTO: i32 = 0;
let WV_SCHEME_LIGHT: i32 = 1;
let WV_SCHEME_DARK: i32 = 2;

// Virtual host folder access constants (COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND).
let WV_HOST_DENY: i32 = 0;
let WV_HOST_ALLOW: i32 = 1;
let WV_HOST_DENY_CORS: i32 = 2;

struct WebView {
    id: u32 = 0,
}

fn is_valid(wv: WebView) -> bool { wv.id != 0 }

// Matches w32_webview_event in include/win32_shim.h (packed).
struct WebViewEvent {
    webview_id: u32 = 0,
    kind: u32 = 0,
    req_id: u64 = 0,
    int_val: i32 = 0,
    flag_a: bool = false,
    flag_b: bool = false,
}

extern fn shim_webview_is_available() -> bool = "w32_webview_is_available";
extern fn shim_webview_runtime_version(out: u8[>..]&) -> bool = "w32_webview_runtime_version";
extern fn shim_webview_create(
    parent_hwnd: u64,
    x: i32,
    y: i32,
    w: i32,
    h: i32,
    user_data_dir: const u8[:],
    additional_args: const u8[:]
) -> u32 = "w32_webview_create";
extern fn shim_webview_destroy(wv_id: u32) = "w32_webview_destroy";
extern fn shim_webview_is_ready(wv_id: u32) -> bool = "w32_webview_is_ready";
extern fn shim_webview_host_hwnd(wv_id: u32) -> u64 = "w32_webview_host_hwnd";
extern fn shim_webview_set_bounds(wv_id: u32, x: i32, y: i32, w: i32, h: i32) = "w32_webview_set_bounds";
extern fn shim_webview_set_bounds_clipped(
    wv_id: u32,
    x: i32,
    y: i32,
    w: i32,
    h: i32,
    clip_x: i32,
    clip_y: i32,
    clip_w: i32,
    clip_h: i32
) = "w32_webview_set_bounds_clipped";
extern fn shim_webview_set_visible(wv_id: u32, visible: bool) = "w32_webview_set_visible";
extern fn shim_webview_focus(wv_id: u32) = "w32_webview_focus";
extern fn shim_webview_notify_parent_pos_changed(wv_id: u32) = "w32_webview_notify_parent_pos_changed";
extern fn shim_webview_navigate(wv_id: u32, url: const u8[:]) -> bool = "w32_webview_navigate";
extern fn shim_webview_set_html(wv_id: u32, html: const u8[:]) -> bool = "w32_webview_set_html";
extern fn shim_webview_reload(wv_id: u32) -> bool = "w32_webview_reload";
extern fn shim_webview_stop(wv_id: u32) -> bool = "w32_webview_stop";
extern fn shim_webview_go_back(wv_id: u32) -> bool = "w32_webview_go_back";
extern fn shim_webview_go_forward(wv_id: u32) -> bool = "w32_webview_go_forward";
extern fn shim_webview_can_go_back(wv_id: u32) -> bool = "w32_webview_can_go_back";
extern fn shim_webview_can_go_forward(wv_id: u32) -> bool = "w32_webview_can_go_forward";
extern fn shim_webview_get_url(wv_id: u32, out: u8[>..]&) -> bool = "w32_webview_get_url";
extern fn shim_webview_get_title(wv_id: u32, out: u8[>..]&) -> bool = "w32_webview_get_title";
extern fn shim_webview_eval_js(wv_id: u32, script: const u8[:]) -> u64 = "w32_webview_eval_js";
extern fn shim_webview_add_init_script(wv_id: u32, script: const u8[:]) -> bool = "w32_webview_add_init_script";
extern fn shim_webview_post_message_json(wv_id: u32, json: const u8[:]) -> bool = "w32_webview_post_message_json";
extern fn shim_webview_post_message_string(wv_id: u32, text: const u8[:]) -> bool = "w32_webview_post_message_string";
extern fn shim_webview_set_background_color(wv_id: u32, r: u8, g: u8, b: u8, a: u8) -> bool = "w32_webview_set_background_color";
extern fn shim_webview_set_color_scheme(wv_id: u32, scheme: i32) -> bool = "w32_webview_set_color_scheme";
extern fn shim_webview_set_zoom(wv_id: u32, factor: f64) -> bool = "w32_webview_set_zoom";
extern fn shim_webview_get_zoom(wv_id: u32) -> f64 = "w32_webview_get_zoom";
extern fn shim_webview_set_muted(wv_id: u32, muted: bool) -> bool = "w32_webview_set_muted";
extern fn shim_webview_is_muted(wv_id: u32) -> bool = "w32_webview_is_muted";
extern fn shim_webview_is_playing_audio(wv_id: u32) -> bool = "w32_webview_is_playing_audio";
extern fn shim_webview_set_devtools_enabled(wv_id: u32, enabled: bool) -> bool = "w32_webview_set_devtools_enabled";
extern fn shim_webview_open_devtools(wv_id: u32) -> bool = "w32_webview_open_devtools";
extern fn shim_webview_set_context_menu_enabled(wv_id: u32, enabled: bool) -> bool = "w32_webview_set_context_menu_enabled";
extern fn shim_webview_set_status_bar_enabled(wv_id: u32, enabled: bool) -> bool = "w32_webview_set_status_bar_enabled";
extern fn shim_webview_set_zoom_control_enabled(wv_id: u32, enabled: bool) -> bool = "w32_webview_set_zoom_control_enabled";
extern fn shim_webview_set_accelerator_keys_enabled(wv_id: u32, enabled: bool) -> bool = "w32_webview_set_accelerator_keys_enabled";
extern fn shim_webview_set_user_agent(wv_id: u32, ua: const u8[:]) -> bool = "w32_webview_set_user_agent";
extern fn shim_webview_set_new_window_policy(wv_id: u32, policy: i32) -> bool = "w32_webview_set_new_window_policy";
extern fn shim_webview_set_virtual_host_mapping(
    wv_id: u32,
    host_name: const u8[:],
    folder_path: const u8[:],
    access_kind: i32
) -> bool = "w32_webview_set_virtual_host_mapping";
extern fn shim_webview_clear_virtual_host_mapping(wv_id: u32, host_name: const u8[:]) -> bool = "w32_webview_clear_virtual_host_mapping";
extern fn shim_webview_poll_event(out_meta: WebViewEvent&, out_str1: u8[>..]&, out_str2: u8[>..]&) -> bool = "w32_webview_poll_event";

// --- High-level Goose API ---------------------------------------------------

fn webview_is_available() -> bool {
    return shim_webview_is_available();
}

fn webview_runtime_version() -> u8[>..] {
    var out: u8[>..] = [];
    shim_webview_runtime_version(out);
    return out;
}

fn webview_create(
    parent: Hwnd,
    x: i32 = 0,
    y: i32 = 0,
    w: i32 = 320,
    h: i32 = 240,
    user_data_dir: const u8[:] = "",
    additional_args: const u8[:] = ""
) -> WebView {
    return WebView {
        id: shim_webview_create(parent.raw, x, y, w, h, user_data_dir, additional_args),
    };
}

fn webview_poll_event(out_meta: WebViewEvent&, out_str1: u8[>..]&, out_str2: u8[>..]&) -> bool {
    out_str1.clear();
    out_str2.clear();
    return shim_webview_poll_event(out_meta, out_str1, out_str2);
}

fn destroy(wv: WebView) { shim_webview_destroy(wv.id); }
fn is_ready(wv: WebView) -> bool { return shim_webview_is_ready(wv.id); }
fn host_hwnd(wv: WebView) -> Hwnd { return Hwnd { raw: shim_webview_host_hwnd(wv.id) }; }

fn set_bounds(wv: WebView, x: i32, y: i32, w: i32, h: i32) {
    shim_webview_set_bounds(wv.id, x, y, w, h);
}

fn set_bounds_clipped(
    wv: WebView,
    x: i32,
    y: i32,
    w: i32,
    h: i32,
    clip_x: i32,
    clip_y: i32,
    clip_w: i32,
    clip_h: i32
) {
    shim_webview_set_bounds_clipped(wv.id, x, y, w, h, clip_x, clip_y, clip_w, clip_h);
}

fn set_visible(wv: WebView, visible: bool) { shim_webview_set_visible(wv.id, visible); }
fn focus(wv: WebView) { shim_webview_focus(wv.id); }
fn notify_parent_pos_changed(wv: WebView) { shim_webview_notify_parent_pos_changed(wv.id); }

fn navigate(wv: WebView, url: const u8[:]) -> bool { return shim_webview_navigate(wv.id, url); }
fn set_html(wv: WebView, html: const u8[:]) -> bool { return shim_webview_set_html(wv.id, html); }
fn reload(wv: WebView) -> bool { return shim_webview_reload(wv.id); }
fn stop(wv: WebView) -> bool { return shim_webview_stop(wv.id); }
fn go_back(wv: WebView) -> bool { return shim_webview_go_back(wv.id); }
fn go_forward(wv: WebView) -> bool { return shim_webview_go_forward(wv.id); }
fn can_go_back(wv: WebView) -> bool { return shim_webview_can_go_back(wv.id); }
fn can_go_forward(wv: WebView) -> bool { return shim_webview_can_go_forward(wv.id); }

fn url(wv: WebView) -> u8[>..] {
    var out: u8[>..] = [];
    shim_webview_get_url(wv.id, out);
    return out;
}

fn title(wv: WebView) -> u8[>..] {
    var out: u8[>..] = [];
    shim_webview_get_title(wv.id, out);
    return out;
}

fn eval_js(wv: WebView, script: const u8[:]) -> u64 { return shim_webview_eval_js(wv.id, script); }
fn add_init_script(wv: WebView, script: const u8[:]) -> bool { return shim_webview_add_init_script(wv.id, script); }
fn post_message_json(wv: WebView, json: const u8[:]) -> bool { return shim_webview_post_message_json(wv.id, json); }
fn post_message_string(wv: WebView, text: const u8[:]) -> bool { return shim_webview_post_message_string(wv.id, text); }

fn set_background_color(wv: WebView, r: u8, g: u8, b: u8, a: u8 = 255) -> bool {
    return shim_webview_set_background_color(wv.id, r, g, b, a);
}

fn set_color_scheme(wv: WebView, scheme: i32) -> bool { return shim_webview_set_color_scheme(wv.id, scheme); }
fn set_zoom(wv: WebView, factor: f64) -> bool { return shim_webview_set_zoom(wv.id, factor); }
fn zoom(wv: WebView) -> f64 { return shim_webview_get_zoom(wv.id); }
fn set_muted(wv: WebView, muted: bool) -> bool { return shim_webview_set_muted(wv.id, muted); }
fn is_muted(wv: WebView) -> bool { return shim_webview_is_muted(wv.id); }
fn is_playing_audio(wv: WebView) -> bool { return shim_webview_is_playing_audio(wv.id); }

fn set_devtools_enabled(wv: WebView, enabled: bool) -> bool { return shim_webview_set_devtools_enabled(wv.id, enabled); }
fn open_devtools(wv: WebView) -> bool { return shim_webview_open_devtools(wv.id); }
fn set_context_menu_enabled(wv: WebView, enabled: bool) -> bool { return shim_webview_set_context_menu_enabled(wv.id, enabled); }
fn set_status_bar_enabled(wv: WebView, enabled: bool) -> bool { return shim_webview_set_status_bar_enabled(wv.id, enabled); }
fn set_zoom_control_enabled(wv: WebView, enabled: bool) -> bool { return shim_webview_set_zoom_control_enabled(wv.id, enabled); }
fn set_accelerator_keys_enabled(wv: WebView, enabled: bool) -> bool { return shim_webview_set_accelerator_keys_enabled(wv.id, enabled); }
fn set_user_agent(wv: WebView, ua: const u8[:]) -> bool { return shim_webview_set_user_agent(wv.id, ua); }
fn set_new_window_policy(wv: WebView, policy: i32) -> bool { return shim_webview_set_new_window_policy(wv.id, policy); }

fn set_virtual_host_mapping(
    wv: WebView,
    host_name: const u8[:],
    folder_path: const u8[:],
    access_kind: i32 = WV_HOST_ALLOW
) -> bool {
    return shim_webview_set_virtual_host_mapping(wv.id, host_name, folder_path, access_kind);
}

fn clear_virtual_host_mapping(wv: WebView, host_name: const u8[:]) -> bool {
    return shim_webview_clear_virtual_host_mapping(wv.id, host_name);
}
