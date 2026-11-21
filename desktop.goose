// Win32 Desktop, Shell, DWM (dark titlebar / rounded corners / Mica),
// Known Folders, ShellExecute, and System Tray Icon API.
namespace win32;

import constants;
import types;

extern fn shim_dwm_set_dark_titlebar(hwnd: u64, enabled: bool) -> bool = "w32_dwm_set_dark_titlebar";
extern fn shim_dwm_set_corner_preference(hwnd: u64, pref: u32) -> bool = "w32_dwm_set_corner_preference";
extern fn shim_dwm_set_backdrop(hwnd: u64, backdrop: u32) -> bool = "w32_dwm_set_backdrop";
extern fn dwm_flush() -> bool = "w32_dwm_flush";

extern fn known_folder_path(kind: i32, out: u8[>..]&) -> bool = "w32_known_folder_path";
extern fn shell_open(
    target: const u8[:],
    params: const u8[:] = "",
    verb: const u8[:] = "open",
    show_cmd: i32 = SW_SHOWNORMAL
) -> bool = "w32_shell_open";

extern fn shim_tray_add(hwnd: u64, id: u32, tooltip: const u8[:]) -> bool = "w32_tray_add";
extern fn shim_tray_set_tooltip(hwnd: u64, id: u32, tooltip: const u8[:]) -> bool = "w32_tray_set_tooltip";
extern fn shim_tray_balloon(hwnd: u64, id: u32, title: const u8[:], message: const u8[:], info_flags: u32) -> bool = "w32_tray_balloon";
extern fn shim_tray_remove(hwnd: u64, id: u32) -> bool = "w32_tray_remove";

fn known_folder_path(kind: i32) -> u8[>..] {
    var out: u8[>..] = [];
    known_folder_path(kind, out);
    return out;
}

// --- Hwnd DWM & System Tray methods -----------------------------------------

fn set_dark_titlebar(w: Hwnd, enabled: bool) -> bool {
    return w.is_valid() && shim_dwm_set_dark_titlebar(w.raw, enabled);
}

fn set_corner_preference(w: Hwnd, pref: u32) -> bool {
    return w.is_valid() && shim_dwm_set_corner_preference(w.raw, pref);
}

fn set_backdrop(w: Hwnd, backdrop: u32) -> bool {
    return w.is_valid() && shim_dwm_set_backdrop(w.raw, backdrop);
}

fn tray_add(w: Hwnd, id: u32, tooltip: const u8[:]) -> bool {
    return w.is_valid() && shim_tray_add(w.raw, id, tooltip);
}

fn tray_set_tooltip(w: Hwnd, id: u32, tooltip: const u8[:]) -> bool {
    return w.is_valid() && shim_tray_set_tooltip(w.raw, id, tooltip);
}

fn tray_balloon(w: Hwnd, id: u32, title: const u8[:], message: const u8[:], info_flags: u32 = NIIF_INFO) -> bool {
    return w.is_valid() && shim_tray_balloon(w.raw, id, title, message, info_flags);
}

fn tray_remove(w: Hwnd, id: u32) -> bool {
    return w.is_valid() && shim_tray_remove(w.raw, id);
}
