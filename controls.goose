// Win32 Standard UI Controls (Button, Checkbox, Label, Edit, ListBox, ComboBox,
// ProgressBar), Drag & Drop (WM_DROPFILES), and Global Hotkeys (WM_HOTKEY).
namespace win32;

import constants;
import types;
import raw;

extern fn init_common_controls() -> bool = "w32_init_common_controls";

extern fn shim_create_control(
    parent: u64,
    id: u32,
    class_name: const u8[:],
    text: const u8[:],
    x: i32,
    y: i32,
    width: i32,
    height: i32,
    style: u32,
    ex_style: u32
) -> u64 = "w32_create_control";

extern fn shim_listbox_add(hwnd: u64, text: const u8[:]) -> i32 = "w32_listbox_add";
extern fn shim_listbox_item_text(hwnd: u64, index: i32, out: u8[>..]&) -> bool = "w32_listbox_item_text";
extern fn shim_combobox_add(hwnd: u64, text: const u8[:]) -> i32 = "w32_combobox_add";
extern fn shim_combobox_item_text(hwnd: u64, index: i32, out: u8[>..]&) -> bool = "w32_combobox_item_text";

extern fn shim_accept_drop_files(hwnd: u64, enable: bool) = "w32_accept_drop_files";
extern fn dropped_file_count() -> i32 = "w32_dropped_file_count";
extern fn dropped_file_path(index: i32, out: u8[>..]&) -> bool = "w32_dropped_file_path";

extern fn shim_tab_add(hwnd: u64, index: i32, text: const u8[:]) -> i32 = "w32_tab_add";
extern fn shim_statusbar_set_text(hwnd: u64, part: i32, text: const u8[:]) -> bool = "w32_statusbar_set_text";

// --- Control creation methods on parent Hwnd --------------------------------

fn create_button(parent: Hwnd, id: u32, text: const u8[:], x: i32, y: i32, w: i32, h: i32) -> Hwnd {
    return Hwnd { raw: shim_create_control(parent.raw, id, "BUTTON", text, x, y, w, h, BS_PUSHBUTTON | WS_TABSTOP, 0) };
}

fn create_checkbox(parent: Hwnd, id: u32, text: const u8[:], x: i32, y: i32, w: i32, h: i32, checked: bool = false) -> Hwnd {
    let cb = Hwnd { raw: shim_create_control(parent.raw, id, "BUTTON", text, x, y, w, h, BS_AUTOCHECKBOX | WS_TABSTOP, 0) };
    if cb.is_valid() && checked { cb.set_checked(true); }
    return cb;
}

fn create_label(parent: Hwnd, id: u32, text: const u8[:], x: i32, y: i32, w: i32, h: i32) -> Hwnd {
    return Hwnd { raw: shim_create_control(parent.raw, id, "STATIC", text, x, y, w, h, SS_LEFT, 0) };
}

fn create_edit(parent: Hwnd, id: u32, text: const u8[:], x: i32, y: i32, w: i32, h: i32, multiline: bool = false) -> Hwnd {
    let style = if multiline {
        WS_BORDER | WS_TABSTOP | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL
    } else {
        WS_BORDER | WS_TABSTOP | ES_LEFT | ES_AUTOHSCROLL
    };
    return Hwnd { raw: shim_create_control(parent.raw, id, "EDIT", text, x, y, w, h, style, WS_EX_CLIENTEDGE) };
}

fn create_listbox(parent: Hwnd, id: u32, x: i32, y: i32, w: i32, h: i32) -> Hwnd {
    return Hwnd { raw: shim_create_control(parent.raw, id, "LISTBOX", "", x, y, w, h, WS_BORDER | WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY, WS_EX_CLIENTEDGE) };
}

fn create_combobox(parent: Hwnd, id: u32, x: i32, y: i32, w: i32, h: i32) -> Hwnd {
    return Hwnd { raw: shim_create_control(parent.raw, id, "COMBOBOX", "", x, y, w, h, WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, 0) };
}

fn create_progressbar(parent: Hwnd, id: u32, x: i32, y: i32, w: i32, h: i32, max_val: i32 = 100) -> Hwnd {
    init_common_controls();
    let pb = Hwnd { raw: shim_create_control(parent.raw, id, "msctls_progress32", "", x, y, w, h, 0, 0) };
    if pb.is_valid() { pb.progress_set_range(0, max_val); }
    return pb;
}

// --- Control state methods on Hwnd ------------------------------------------

fn enable(w: Hwnd, enabled: bool) -> bool {
    return EnableWindow(w.raw, if enabled { 1 } else { 0 }) != 0;
}

fn is_enabled(w: Hwnd) -> bool {
    return w.is_valid() && IsWindowEnabled(w.raw) != 0;
}

fn is_checked(w: Hwnd) -> bool {
    return SendMessageW(w.raw, BM_GETCHECK, 0, 0) == 1;
}

fn set_checked(w: Hwnd, checked: bool) {
    SendMessageW(w.raw, BM_SETCHECK, if checked { BST_CHECKED } else { BST_UNCHECKED }, 0);
}

fn listbox_add(w: Hwnd, text: const u8[:]) -> i32 { shim_listbox_add(w.raw, text) }
fn listbox_count(w: Hwnd) -> i32 { SendMessageW(w.raw, LB_GETCOUNT, 0, 0) as! i32 }
fn listbox_selected(w: Hwnd) -> i32 { SendMessageW(w.raw, LB_GETCURSEL, 0, 0) as! i32 }
fn listbox_set_selected(w: Hwnd, idx: i32) { SendMessageW(w.raw, LB_SETCURSEL, idx as! u64, 0); }
fn listbox_clear(w: Hwnd) { SendMessageW(w.raw, LB_RESETCONTENT, 0, 0); }

fn listbox_item_text(w: Hwnd, idx: i32) -> u8[>..] {
    var out: u8[>..] = [];
    shim_listbox_item_text(w.raw, idx, out);
    return out;
}

fn combobox_add(w: Hwnd, text: const u8[:]) -> i32 { shim_combobox_add(w.raw, text) }
fn combobox_count(w: Hwnd) -> i32 { SendMessageW(w.raw, CB_GETCOUNT, 0, 0) as! i32 }
fn combobox_selected(w: Hwnd) -> i32 { SendMessageW(w.raw, CB_GETCURSEL, 0, 0) as! i32 }
fn combobox_set_selected(w: Hwnd, idx: i32) { SendMessageW(w.raw, CB_SETCURSEL, idx as! u64, 0); }
fn combobox_clear(w: Hwnd) { SendMessageW(w.raw, CB_RESETCONTENT, 0, 0); }

fn combobox_item_text(w: Hwnd, idx: i32) -> u8[>..] {
    var out: u8[>..] = [];
    shim_combobox_item_text(w.raw, idx, out);
    return out;
}

fn progress_set_range(w: Hwnd, min_val: i32, max_val: i32) {
    SendMessageW(w.raw, PBM_SETRANGE32, min_val as! u64, max_val);
}

fn progress_set_pos(w: Hwnd, pos: i32) -> i32 {
    return SendMessageW(w.raw, PBM_SETPOS, pos as! u64, 0) as! i32;
}

fn progress_pos(w: Hwnd) -> i32 {
    return SendMessageW(w.raw, PBM_GETPOS, 0, 0) as! i32;
}

// --- Drag & Drop and Global Hotkeys -----------------------------------------

fn accept_drop_files(w: Hwnd, enable: bool) {
    shim_accept_drop_files(w.raw, enable);
}

fn register_hotkey(w: Hwnd, id: i32, modifiers: u32, vk: u32) -> bool {
    return RegisterHotKey(w.raw, id, modifiers, vk) != 0;
}

fn unregister_hotkey(w: Hwnd, id: i32) -> bool {
    return UnregisterHotKey(w.raw, id) != 0;
}

// --- Trackbar (Slider), Tab Control & Status Bar ----------------------------

fn create_trackbar(
    parent: Hwnd,
    id: u32,
    x: i32,
    y: i32,
    w: i32,
    h: i32,
    min_val: i32 = 0,
    max_val: i32 = 100,
    vertical: bool = false
) -> Hwnd {
    init_common_controls();
    let style = WS_TABSTOP | TBS_AUTOTICKS | (if vertical { TBS_VERT } else { TBS_HORZ });
    let tb = Hwnd { raw: shim_create_control(parent.raw, id, "msctls_trackbar32", "", x, y, w, h, style, 0) };
    if tb.is_valid() {
        tb.trackbar_set_range(min_val, max_val);
    }
    return tb;
}

fn trackbar_set_range(w: Hwnd, min_val: i32, max_val: i32) {
    SendMessageW(w.raw, TBM_SETRANGEMIN, 0, min_val);
    SendMessageW(w.raw, TBM_SETRANGEMAX, 1, max_val);
}

fn trackbar_set_pos(w: Hwnd, pos: i32) {
    SendMessageW(w.raw, TBM_SETPOS, 1, pos);
}

fn trackbar_pos(w: Hwnd) -> i32 {
    return SendMessageW(w.raw, TBM_GETPOS, 0, 0) as! i32;
}

fn create_tabcontrol(parent: Hwnd, id: u32, x: i32, y: i32, w: i32, h: i32) -> Hwnd {
    init_common_controls();
    return Hwnd { raw: shim_create_control(parent.raw, id, "SysTabControl32", "", x, y, w, h, WS_TABSTOP | WS_CLIPSIBLINGS, 0) };
}

fn tab_add(w: Hwnd, text: const u8[:]) -> i32 {
    let idx = w.tab_count();
    return shim_tab_add(w.raw, idx, text);
}

fn tab_count(w: Hwnd) -> i32 {
    return SendMessageW(w.raw, TCM_GETITEMCOUNT, 0, 0) as! i32;
}

fn tab_selected(w: Hwnd) -> i32 {
    return SendMessageW(w.raw, TCM_GETCURSEL, 0, 0) as! i32;
}

fn tab_set_selected(w: Hwnd, index: i32) -> i32 {
    return SendMessageW(w.raw, TCM_SETCURSEL, index as! u64, 0) as! i32;
}

fn tab_clear(w: Hwnd) {
    SendMessageW(w.raw, TCM_DELETEALLITEMS, 0, 0);
}

fn create_statusbar(parent: Hwnd, id: u32, text: const u8[:] = "", size_grip: bool = true) -> Hwnd {
    init_common_controls();
    let style = if size_grip { SBARS_SIZEGRIP } else { 0 };
    return Hwnd { raw: shim_create_control(parent.raw, id, "msctls_statusbar32", text, 0, 0, 0, 0, style, 0) };
}

fn statusbar_set_text(w: Hwnd, text: const u8[:], part: i32 = 0) -> bool {
    return shim_statusbar_set_text(w.raw, part, text);
}

fn statusbar_auto_size(w: Hwnd) {
    SendMessageW(w.raw, WM_SIZE, 0, 0);
}
