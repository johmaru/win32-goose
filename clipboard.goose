// Win32 Clipboard API (Unicode text, raw custom formats, and sequence tracking).
namespace win32;

import constants;
import types;
import raw;

extern fn shim_clipboard_set_text(owner: u64, text: const u8[:]) -> bool = "w32_clipboard_set_text";
extern fn shim_clipboard_get_text(owner: u64, out: u8[>..]&) -> bool = "w32_clipboard_get_text";
extern fn shim_clipboard_set_bytes(owner: u64, format: u32, data: const u8[:]) -> bool = "w32_clipboard_set_bytes";
extern fn shim_clipboard_get_bytes(owner: u64, format: u32, out: u8[>..]&) -> bool = "w32_clipboard_get_bytes";
extern fn register_clipboard_format(name: const u8[:]) -> u32 = "w32_register_clipboard_format";

fn clipboard_has_text() -> bool {
    return IsClipboardFormatAvailable(CF_UNICODETEXT) != 0;
}

fn clipboard_has_files() -> bool {
    return IsClipboardFormatAvailable(CF_HDROP) != 0;
}

fn clipboard_has_format(format: u32) -> bool {
    return IsClipboardFormatAvailable(format) != 0;
}

fn clipboard_sequence_number() -> u32 {
    return GetClipboardSequenceNumber();
}

fn clipboard_clear(owner: Hwnd = Hwnd {}) -> bool {
    var opened = false;
    for attempt in 10 {
        if OpenClipboard(owner.raw) != 0 {
            opened = true;
            break;
        }
        Sleep(5);
    }
    guard opened else { return false; }
    let ok = EmptyClipboard() != 0;
    CloseClipboard();
    return ok;
}

fn clipboard_set_text(text: const u8[:], owner: Hwnd = Hwnd {}) -> bool {
    return shim_clipboard_set_text(owner.raw, text);
}

fn clipboard_get_text(out: u8[>..]&, owner: Hwnd = Hwnd {}) -> bool {
    return shim_clipboard_get_text(owner.raw, out);
}

fn clipboard_get_text(owner: Hwnd = Hwnd {}) -> u8[>..] {
    var out: u8[>..] = [];
    shim_clipboard_get_text(owner.raw, out);
    return out;
}

fn clipboard_set_bytes(format: u32, data: const u8[:], owner: Hwnd = Hwnd {}) -> bool {
    return shim_clipboard_set_bytes(owner.raw, format, data);
}

fn clipboard_get_bytes(format: u32, out: u8[>..]&, owner: Hwnd = Hwnd {}) -> bool {
    return shim_clipboard_get_bytes(owner.raw, format, out);
}
