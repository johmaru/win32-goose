// Win32 common dialogs (Open/Save file, Folder picker, Color picker, Input box).
namespace win32;

import types;

extern fn shim_open_file_dialog(
    owner: u64,
    title: const u8[:],
    filter: const u8[:],
    default_ext: const u8[:],
    out_path: u8[>..]&
) -> bool = "w32_open_file_dialog";

extern fn shim_save_file_dialog(
    owner: u64,
    title: const u8[:],
    filter: const u8[:],
    default_ext: const u8[:],
    initial_name: const u8[:],
    out_path: u8[>..]&
) -> bool = "w32_save_file_dialog";

extern fn shim_browse_folder_dialog(
    owner: u64,
    title: const u8[:],
    out_path: u8[>..]&
) -> bool = "w32_browse_folder_dialog";

extern fn shim_choose_color_dialog(
    owner: u64,
    initial_color: u32,
    out_color: u32&
) -> bool = "w32_choose_color_dialog";

extern fn shim_input_dialog(
    owner: u64,
    title: const u8[:],
    prompt: const u8[:],
    initial_text: const u8[:],
    out_text: u8[>..]&
) -> bool = "w32_input_dialog";

// Opens the Win32 File Open dialog.
// `filter` uses pipe separators: "Goose (*.goose)|*.goose|All Files (*.*)|*.*".
// Appends the selected UTF-8 path into `out_path` and returns true if accepted.
fn open_file_dialog(
    out_path: u8[>..]&,
    owner: Hwnd = Hwnd {},
    title: const u8[:] = "Open File",
    filter: const u8[:] = "All Files (*.*)|*.*",
    default_ext: const u8[:] = ""
) -> bool {
    return shim_open_file_dialog(owner.raw, title, filter, default_ext, out_path);
}

// Opens the Win32 File Save dialog.
fn save_file_dialog(
    out_path: u8[>..]&,
    owner: Hwnd = Hwnd {},
    title: const u8[:] = "Save File",
    filter: const u8[:] = "All Files (*.*)|*.*",
    default_ext: const u8[:] = "",
    initial_name: const u8[:] = ""
) -> bool {
    return shim_save_file_dialog(owner.raw, title, filter, default_ext, initial_name, out_path);
}

// Opens the modern Windows folder picker (IFileOpenDialog + FOS_PICKFOLDERS).
fn browse_folder_dialog(
    out_path: u8[>..]&,
    owner: Hwnd = Hwnd {},
    title: const u8[:] = "Select Folder"
) -> bool {
    return shim_browse_folder_dialog(owner.raw, title, out_path);
}

// Opens the Win32 ChooseColor dialog. Updates `color` and returns true if accepted.
fn choose_color_dialog(
    color: u32&,
    owner: Hwnd = Hwnd {}
) -> bool {
    var picked: u32 = color;
    guard shim_choose_color_dialog(owner.raw, color, picked) else { return false; }
    color = picked;
    return true;
}

// Opens a native Win32 modal text input dialog (in-memory DLGTEMPLATE, no .rc file).
fn input_box(
    out_text: u8[>..]&,
    owner: Hwnd = Hwnd {},
    title: const u8[:] = "Input",
    prompt: const u8[:] = "Enter text:",
    initial_text: const u8[:] = ""
) -> bool {
    return shim_input_dialog(owner.raw, title, prompt, initial_text, out_text);
}
