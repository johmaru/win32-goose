// Win32 menu bar and context popup menu API.
namespace win32;

import constants;
import types;
import raw;

extern fn shim_append_menu(hmenu: u64, flags: u32, id_or_submenu: u64, text: const u8[:]) -> bool = "w32_append_menu";
extern fn shim_get_menu_string(hmenu: u64, id: u32, flags: u32, out: u8[>..]&) -> bool = "w32_get_menu_string";

fn create_menu() -> Hmenu {
    return Hmenu { raw: CreateMenu() };
}

fn create_popup_menu() -> Hmenu {
    return Hmenu { raw: CreatePopupMenu() };
}

fn destroy(m: Hmenu) -> bool {
    return m.raw != 0 && DestroyMenu(m.raw) != 0;
}

fn append_item(m: Hmenu, id: u32, text: const u8[:], flags: u32 = MF_STRING) -> bool {
    return shim_append_menu(m.raw, flags, id, text);
}

fn append_separator(m: Hmenu) -> bool {
    return shim_append_menu(m.raw, MF_SEPARATOR, 0, "");
}

fn append_submenu(m: Hmenu, submenu: Hmenu, text: const u8[:], flags: u32 = MF_STRING) -> bool {
    return shim_append_menu(m.raw, flags | MF_POPUP, submenu.raw, text);
}

fn item_count(m: Hmenu) -> i32 {
    return GetMenuItemCount(m.raw);
}

fn item_text(m: Hmenu, id: u32, flags: u32 = MF_BYCOMMAND) -> u8[>..] {
    var out: u8[>..] = [];
    shim_get_menu_string(m.raw, id, flags, out);
    return out;
}

fn set_enabled(m: Hmenu, id: u32, enabled: bool) -> bool {
    let f = MF_BYCOMMAND | (if enabled { MF_ENABLED } else { MF_GRAYED });
    return EnableMenuItem(m.raw, id, f) != -1;
}

fn is_enabled(m: Hmenu, id: u32) -> bool {
    let st = GetMenuState(m.raw, id, MF_BYCOMMAND);
    if st == 0xFFFFFFFF { return false; }
    return (st & (MF_GRAYED | MF_DISABLED)) == 0;
}

fn set_checked(m: Hmenu, id: u32, checked: bool) -> bool {
    let f = MF_BYCOMMAND | (if checked { MF_CHECKED } else { MF_UNCHECKED });
    return CheckMenuItem(m.raw, id, f) != 0xFFFFFFFF;
}

fn is_checked(m: Hmenu, id: u32) -> bool {
    let st = GetMenuState(m.raw, id, MF_BYCOMMAND);
    if st == 0xFFFFFFFF { return false; }
    return (st & MF_CHECKED) != 0;
}

// Displays a context popup menu at screen coordinates (screen_x, screen_y).
// With TPM_RETURNCMD (default), returns the selected command ID or 0 if canceled.
fn track_popup(
    m: Hmenu,
    owner: Hwnd,
    screen_x: i32,
    screen_y: i32,
    flags: u32 = TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD
) -> u32 {
    let r = TrackPopupMenu(m.raw, flags, screen_x, screen_y, 0, owner.raw, 0);
    return if r > 0 { r as! u32 } else { 0 };
}

// Convenience: track popup at client coordinates of `owner`.
fn track_popup_client(
    m: Hmenu,
    owner: Hwnd,
    client_x: i32,
    client_y: i32,
    flags: u32 = TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD
) -> u32 {
    var pt = Point { x: client_x, y: client_y };
    ClientToScreen(owner.raw, pt);
    return m.track_popup(owner, pt.x, pt.y, flags);
}

// --- Hwnd menu methods ------------------------------------------------------

fn set_menu(w: Hwnd, m: Hmenu) -> bool {
    return SetMenu(w.raw, m.raw) != 0;
}

fn menu(w: Hwnd) -> Hmenu {
    return Hmenu { raw: GetMenu(w.raw) };
}

fn draw_menu_bar(w: Hwnd) -> bool {
    return DrawMenuBar(w.raw) != 0;
}
