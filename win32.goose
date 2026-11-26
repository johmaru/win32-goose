// win32: A thin-shim + direct-DLL Win32 API package for Goose.
//
// Pure Win32 calls (kernel32, user32, gdi32) live in raw.goose using dynamic
// `extern "dll"` blocks. Only the pieces that require a C WndProc callback,
// pointers inside structs (WNDCLASSEXW), double-buffered painting, or builder
// writes live in src/win32_shim.c.
namespace win32;

import std;
import constants;
import types;
import utf16;
import raw;
import menu;
import dialog;
import clipboard;
import file;
import sync;
import dib;
import d3d11;
import d2d;
import gl;
import gl2d;
import system;
import net;
import display;
import desktop;
import controls;
import game;
import audio;
import video;
import wic;

// --- Thin C shim bindings (src/win32_shim.c) --------------------------------

extern fn enable_dpi_awareness() -> bool = "w32_enable_dpi_awareness";

extern fn shim_register_class(class_name: const u8[:], style: u32) -> bool = "w32_register_class";

extern fn shim_create_window(
    class_name: const u8[:],
    title: const u8[:],
    x: i32,
    y: i32,
    width: i32,
    height: i32,
    style: u32,
    ex_style: u32,
    parent: u64
) -> u64 = "w32_create_window";

extern fn poll_event(out: Event&) -> bool = "w32_poll_event";
extern fn wait_event(out: Event&) -> bool = "w32_wait_event";
extern fn shim_set_window_min_size(hwnd: u64, min_w: i32, min_h: i32) -> bool = "w32_set_window_min_size";

extern fn shim_begin_paint(hwnd: u64, rect: Rect&) -> u64 = "w32_begin_paint";
extern fn shim_end_paint(hwnd: u64) = "w32_end_paint";

extern fn shim_set_window_text(hwnd: u64, text: const u8[:]) -> bool = "w32_set_window_text";
extern fn shim_message_box(hwnd: u64, text: const u8[:], caption: const u8[:], flags: u32) -> i32 = "w32_message_box";
extern fn shim_draw_text(hdc: u64, text: const u8[:], rect: Rect&, format: u32) -> i32 = "w32_draw_text";
extern fn shim_text_out(hdc: u64, x: i32, y: i32, text: const u8[:]) -> bool = "w32_text_out";

extern fn shim_get_window_text(hwnd: u64, out: u8[>..]&) -> bool = "w32_get_window_text";
extern fn format_error(error_code: u32, out: u8[>..]&) -> bool = "w32_format_error";

extern fn set_system_cursor(idc: u32) -> bool = "w32_set_system_cursor";
extern fn shim_clip_cursor_window(hwnd: u64) -> bool = "w32_clip_cursor_window";
extern fn unclip_cursor() -> bool = "w32_unclip_cursor";
extern fn shim_set_borderless_fullscreen(hwnd: u64, enable: bool) -> bool = "w32_set_borderless_fullscreen";
extern fn shim_enable_raw_mouse(hwnd: u64, enable: bool) -> bool = "w32_enable_raw_mouse";

// --- Convenience functions --------------------------------------------------

let DEFAULT_CLASS_NAME: const u8[:] = "Win32GooseWindow";

fn format_error(error_code: u32) -> u8[>..] {
    var out: u8[>..] = [];
    format_error(error_code, out);
    return out;
}

fn last_error() -> u32 { GetLastError() }

fn register_class(class_name: const u8[:] = DEFAULT_CLASS_NAME, style: u32 = CS_HREDRAW | CS_VREDRAW | CS_OWNDC) -> bool {
    return shim_register_class(class_name, style);
}

// Registers the class if needed and creates a window sized so its *client* area
// matches (client_width, client_height).
fn create_window(
    title: const u8[:],
    client_width: i32 = 800,
    client_height: i32 = 600,
    style: u32 = WS_OVERLAPPEDWINDOW,
    ex_style: u32 = 0,
    x: i32 = CW_USEDEFAULT,
    y: i32 = CW_USEDEFAULT,
    class_name: const u8[:] = DEFAULT_CLASS_NAME,
    parent: Hwnd = Hwnd {}
) -> Hwnd {
    guard shim_register_class(class_name, CS_HREDRAW | CS_VREDRAW | CS_OWNDC) else {
        return Hwnd {};
    }
    var win_w = client_width;
    var win_h = client_height;
    if client_width != CW_USEDEFAULT || client_height != CW_USEDEFAULT {
        var rc = Rect {
            left: 0,
            top: 0,
            right: if client_width == CW_USEDEFAULT { 0 } else { client_width },
            bottom: if client_height == CW_USEDEFAULT { 0 } else { client_height },
        };
        AdjustWindowRectEx(rc, style, 0, ex_style);
        if client_width != CW_USEDEFAULT { win_w = rc.right - rc.left; }
        if client_height != CW_USEDEFAULT { win_h = rc.bottom - rc.top; }
    }
    let raw_hwnd = shim_create_window(
        class_name,
        title,
        x,
        y,
        win_w,
        win_h,
        style,
        ex_style,
        parent.raw
    );
    return Hwnd { raw: raw_hwnd };
}

fn message_box(text: const u8[:], caption: const u8[:] = "Goose", flags: u32 = MB_OK, owner: Hwnd = Hwnd {}) -> i32 {
    return shim_message_box(owner.raw, text, caption, flags);
}

fn post_quit_message(exit_code: i32 = 0) {
    PostQuitMessage(exit_code);
}

fn tick_count_ms() -> u64 { GetTickCount64() }

fn perf_counter() -> i64 {
    var c: i64 = 0;
    QueryPerformanceCounter(c);
    return c;
}

fn perf_frequency() -> i64 {
    var f: i64 = 0;
    QueryPerformanceFrequency(f);
    return f;
}

// --- Hwnd methods (UFCS) ----------------------------------------------------

fn show(w: Hwnd, cmd: i32 = SW_SHOW) -> bool {
    return ShowWindow(w.raw, cmd) != 0;
}

fn update(w: Hwnd) -> bool {
    return UpdateWindow(w.raw) != 0;
}

fn destroy(w: Hwnd) -> bool {
    return DestroyWindow(w.raw) != 0;
}

fn exists(w: Hwnd) -> bool {
    return w.raw != 0 && IsWindow(w.raw) != 0;
}

fn is_visible(w: Hwnd) -> bool {
    return w.raw != 0 && IsWindowVisible(w.raw) != 0;
}

fn post_message(w: Hwnd, msg: u32, wparam: u64 = 0, lparam: i64 = 0) -> bool {
    return PostMessageW(w.raw, msg, wparam, lparam) != 0;
}

fn close(w: Hwnd) -> bool {
    return PostMessageW(w.raw, WM_CLOSE, 0, 0) != 0;
}

fn invalidate(w: Hwnd, erase: bool = false) -> bool {
    return InvalidateRectAll(w.raw, 0, if erase { 1 } else { 0 }) != 0;
}

fn set_title(w: Hwnd, title: const u8[:]) -> bool {
    return shim_set_window_text(w.raw, title);
}

fn title(w: Hwnd) -> u8[>..] {
    var out: u8[>..] = [];
    shim_get_window_text(w.raw, out);
    return out;
}

fn client_rect(w: Hwnd) -> Rect {
    var rc = Rect {};
    GetClientRect(w.raw, rc);
    return rc;
}

fn window_rect(w: Hwnd) -> Rect {
    var rc = Rect {};
    GetWindowRect(w.raw, rc);
    return rc;
}

fn move(w: Hwnd, x: i32, y: i32, width: i32, height: i32, repaint: bool = true) -> bool {
    return MoveWindow(w.raw, x, y, width, height, if repaint { 1 } else { 0 }) != 0;
}

fn set_min_size(w: Hwnd, min_client_w: i32, min_client_h: i32) -> bool {
    return shim_set_window_min_size(w.raw, min_client_w, min_client_h);
}

fn dpi(w: Hwnd) -> u32 {
    return GetDpiForWindow(w.raw);
}

fn set_timer(w: Hwnd, id: u64, elapse_ms: u32) -> u64 {
    return SetTimer(w.raw, id, elapse_ms, 0);
}

fn kill_timer(w: Hwnd, id: u64) -> bool {
    return KillTimer(w.raw, id) != 0;
}

fn begin_paint(w: Hwnd, out_rect: Rect&) -> Hdc {
    return Hdc { raw: shim_begin_paint(w.raw, out_rect) };
}

fn end_paint(w: Hwnd) {
    shim_end_paint(w.raw);
}

// --- Cursor, Capture, Foreground, Fullscreen & Raw Input --------------------

fn show_cursor(visible: bool) -> i32 {
    return ShowCursor(if visible { 1 } else { 0 });
}

fn clip_cursor(w: Hwnd) -> bool {
    return shim_clip_cursor_window(w.raw);
}

fn set_capture(w: Hwnd) -> Hwnd {
    return Hwnd { raw: SetCapture(w.raw) };
}

fn release_capture() -> bool {
    return ReleaseCapture() != 0;
}

fn foreground_window() -> Hwnd {
    return Hwnd { raw: GetForegroundWindow() };
}

fn set_foreground(w: Hwnd) -> bool {
    return SetForegroundWindow(w.raw) != 0;
}

fn set_borderless_fullscreen(w: Hwnd, enable: bool) -> bool {
    return shim_set_borderless_fullscreen(w.raw, enable);
}

fn enable_raw_mouse(w: Hwnd, enable: bool) -> bool {
    return shim_enable_raw_mouse(w.raw, enable);
}

// --- GDI helpers ------------------------------------------------------------

fn create_solid_brush(color: u32) -> Hbrush {
    return Hbrush { raw: CreateSolidBrush(color) };
}

fn delete(b: Hbrush) -> bool {
    return b.raw != 0 && DeleteObject(b.raw) != 0;
}

fn create_pen(style: i32, width: i32, color: u32) -> Hpen {
    return Hpen { raw: CreatePen(style, width, color) };
}

fn delete(p: Hpen) -> bool {
    return p.raw != 0 && DeleteObject(p.raw) != 0;
}

fn stock_object(index: i32) -> u64 {
    return GetStockObject(index);
}

fn fill_rect(dc: Hdc, r: const Rect&, brush: Hbrush) -> bool {
    return FillRect(dc.raw, r, brush.raw) != 0;
}

fn fill_rect(dc: Hdc, r: const Rect&, color: u32) -> bool {
    let b = create_solid_brush(color);
    guard b.is_valid() else { return false; }
    defer b.delete();
    return FillRect(dc.raw, r, b.raw) != 0;
}

fn frame_rect(dc: Hdc, r: const Rect&, brush: Hbrush) -> bool {
    return FrameRect(dc.raw, r, brush.raw) != 0;
}

fn select_object(dc: Hdc, obj: u64) -> u64 {
    return SelectObject(dc.raw, obj);
}

fn select_brush(dc: Hdc, brush: Hbrush) -> u64 {
    return SelectObject(dc.raw, brush.raw);
}

fn select_pen(dc: Hdc, pen: Hpen) -> u64 {
    return SelectObject(dc.raw, pen.raw);
}

fn set_text_color(dc: Hdc, color: u32) -> u32 {
    return SetTextColor(dc.raw, color);
}

fn set_bk_color(dc: Hdc, color: u32) -> u32 {
    return SetBkColor(dc.raw, color);
}

fn set_bk_mode(dc: Hdc, mode: i32) -> i32 {
    return SetBkMode(dc.raw, mode);
}

fn rectangle(dc: Hdc, left: i32, top: i32, right: i32, bottom: i32) -> bool {
    return Rectangle(dc.raw, left, top, right, bottom) != 0;
}

fn ellipse(dc: Hdc, left: i32, top: i32, right: i32, bottom: i32) -> bool {
    return Ellipse(dc.raw, left, top, right, bottom) != 0;
}

fn round_rect(dc: Hdc, left: i32, top: i32, right: i32, bottom: i32, rx: i32, ry: i32) -> bool {
    return RoundRect(dc.raw, left, top, right, bottom, rx, ry) != 0;
}

fn move_to(dc: Hdc, x: i32, y: i32) -> bool {
    return MoveToEx(dc.raw, x, y, 0) != 0;
}

fn line_to(dc: Hdc, x: i32, y: i32) -> bool {
    return LineTo(dc.raw, x, y) != 0;
}

fn draw_text(dc: Hdc, text: const u8[:], rect: Rect&, format: u32 = DT_LEFT | DT_TOP) -> i32 {
    return shim_draw_text(dc.raw, text, rect, format);
}

fn text_out(dc: Hdc, x: i32, y: i32, text: const u8[:]) -> bool {
    return shim_text_out(dc.raw, x, y, text);
}
