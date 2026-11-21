// Interactive Win32 Menu Bar, Context Popup Menu, and Common Dialogs demo.
// Run with:   goose run examples/menu_and_dialogs.goose
// Build with: goose build examples/menu_and_dialogs.goose
import std;
import win32 from "..";

let IDM_FILE_OPEN: u32 = 1001;
let IDM_FILE_SAVE: u32 = 1002;
let IDM_FILE_FOLDER: u32 = 1003;
let IDM_FILE_EXIT: u32 = 1004;

let IDM_EDIT_TITLE: u32 = 2001;
let IDM_EDIT_COLOR: u32 = 2002;
let IDM_EDIT_DARK: u32 = 2003;
let IDM_EDIT_COPY: u32 = 2004;
let IDM_EDIT_PASTE: u32 = 2005;

let IDM_HELP_ABOUT: u32 = 3001;

fn build_menu_bar() -> win32::Hmenu {
    let bar = win32::create_menu();

    let file = win32::create_popup_menu();
    file.append_item(IDM_FILE_OPEN, "Open File...\tCtrl+O");
    file.append_item(IDM_FILE_SAVE, "Save File...\tCtrl+S");
    file.append_item(IDM_FILE_FOLDER, "Select Folder...");
    file.append_separator();
    file.append_item(IDM_FILE_EXIT, "Exit\tEsc");
    bar.append_submenu(file, "&File");

    let edit = win32::create_popup_menu();
    edit.append_item(IDM_EDIT_COPY, "&Copy Content");
    edit.append_item(IDM_EDIT_PASTE, "&Paste from Clipboard");
    edit.append_separator();
    edit.append_item(IDM_EDIT_TITLE, "Set Window Title...");
    edit.append_item(IDM_EDIT_COLOR, "Choose Accent Color...");
    edit.append_separator();
    edit.append_item(IDM_EDIT_DARK, "Dark Theme", win32::MF_STRING | win32::MF_CHECKED);
    bar.append_submenu(edit, "&Edit");

    let help = win32::create_popup_menu();
    help.append_item(IDM_HELP_ABOUT, "&About win32-goose...");
    bar.append_submenu(help, "&Help");

    return bar;
}

fn build_context_menu(dark: bool) -> win32::Hmenu {
    let ctx = win32::create_popup_menu();
    ctx.append_item(IDM_EDIT_COLOR, "Choose Accent Color...");
    ctx.append_item(IDM_EDIT_TITLE, "Set Window Title...");
    ctx.append_separator();
    ctx.append_item(IDM_EDIT_DARK, "Dark Theme", win32::MF_STRING | (if dark { win32::MF_CHECKED } else { win32::MF_UNCHECKED }));
    ctx.append_separator();
    ctx.append_item(IDM_FILE_OPEN, "Open File...");
    return ctx;
}

fn render(win: win32::Hwnd, dark: bool, accent: u32, status: const u8[:], last_path: const u8[:]) {
    var rc = win32::Rect {};
    let dc = win.begin_paint(rc);
    guard dc.is_valid();
    defer win.end_paint();

    let bg = if dark { win32::rgb(20, 24, 32) } else { win32::rgb(245, 247, 250) };
    let card_bg = if dark { win32::rgb(32, 39, 52) } else { win32::rgb(255, 255, 255) };
    let fg = if dark { win32::rgb(241, 245, 249) } else { win32::rgb(15, 23, 42) };
    let muted = if dark { win32::rgb(148, 163, 184) } else { win32::rgb(71, 85, 105) };

    dc.fill_rect(rc, bg);

    let pad_x: i32 = 32;
    let pad_y: i32 = 28;
    let card_b = win32::create_solid_brush(card_bg);
    let card_p = win32::create_pen(win32::PS_SOLID, 2, accent);
    let old_b = dc.select_brush(card_b);
    let old_p = dc.select_pen(card_p);
    dc.round_rect(pad_x, pad_y, rc.width() - pad_x, rc.height() - pad_y, 14, 14);
    dc.select_object(old_b);
    dc.select_object(old_p);
    card_b.delete();
    card_p.delete();

    // Accent color swatch.
    let swatch_rc = win32::Rect { left: pad_x + 24, top: pad_y + 24, right: pad_x + 64, bottom: pad_y + 64 };
    dc.fill_rect(swatch_rc, accent);

    let old_font = dc.select_object(win32::stock_object(win32::DEFAULT_GUI_FONT));
    dc.set_bk_mode(win32::TRANSPARENT);
    dc.set_text_color(fg);

    var header_rc = win32::Rect { left: pad_x + 80, top: pad_y + 24, right: rc.width() - pad_x - 24, bottom: pad_y + 64 };
    dc.draw_text("win32-goose: Menus & Common Dialogs", header_rc, win32::DT_LEFT | win32::DT_VCENTER | win32::DT_SINGLELINE);

    dc.set_text_color(muted);
    var status_rc = win32::Rect { left: pad_x + 24, top: pad_y + 84, right: rc.width() - pad_x - 24, bottom: pad_y + 130 };
    dc.draw_text(status, status_rc, win32::DT_LEFT | win32::DT_WORDBREAK);

    dc.set_text_color(fg);
    var path_rc = win32::Rect { left: pad_x + 24, top: pad_y + 144, right: rc.width() - pad_x - 24, bottom: rc.height() - pad_y - 56 };
    dc.draw_text(last_path, path_rc, win32::DT_LEFT | win32::DT_WORDBREAK);

    dc.set_text_color(accent);
    var hint_rc = win32::Rect { left: pad_x + 24, top: rc.height() - pad_y - 42, right: rc.width() - pad_x - 24, bottom: rc.height() - pad_y - 16 };
    dc.draw_text("Use the menu bar above or right-click anywhere for a context menu.", hint_rc, win32::DT_LEFT | win32::DT_SINGLELINE);

    dc.select_object(old_font);
}

fn handle_command(
    cmd: u32,
    win: win32::Hwnd,
    bar: win32::Hmenu,
    dark: bool&,
    accent: u32&,
    status: u8[>..]&,
    last_path: u8[>..]&
) {
    if cmd == IDM_FILE_OPEN {
        var path: u8[>..] = [];
        if win32::open_file_dialog(path, win, "Open a File", "Text / Goose (*.goose;*.txt;*.md)|*.goose;*.txt;*.md|All Files (*.*)|*.*") {
            var content: u8[>..] = [];
            if win32::read_file_bytes(path, content) {
                status.clear();
                format(status, "Opened (", content.len, " bytes): ", path);
                last_path.clear();
                let preview_len = if content.len > 400 { 400 } else { content.len };
                last_path.append(content[..preview_len]);
            } else {
                status.clear();
                format(status, "Failed to read file: ", path);
            }
        } else {
            status.clear();
            status.append("Open File dialog canceled.");
        }
        win.invalidate();
    } else if cmd == IDM_FILE_SAVE {
        var path: u8[>..] = [];
        if win32::save_file_dialog(path, win, "Save Content As", "Text Files (*.txt)|*.txt|All Files (*.*)|*.*", "txt", "untitled.txt") {
            if win32::write_file_bytes(path, last_path) {
                status.clear();
                format(status, "Saved (", last_path.len, " bytes) to: ", path);
            } else {
                status.clear();
                format(status, "Failed to save: ", path);
            }
        } else {
            status.clear();
            status.append("Save File dialog canceled.");
        }
        win.invalidate();
    } else if cmd == IDM_FILE_FOLDER {
        var path: u8[>..] = [];
        if win32::browse_folder_dialog(path, win, "Select a Folder") {
            status.clear();
            status.append("Selected folder via IFileOpenDialog (FOS_PICKFOLDERS):");
            last_path.clear();
            last_path.append(path);
        } else {
            status.clear();
            status.append("Folder picker canceled.");
        }
        win.invalidate();
    } else if cmd == IDM_FILE_EXIT {
        win.close();
    } else if cmd == IDM_EDIT_TITLE {
        var text: u8[>..] = [];
        let cur = win.title();
        if win32::input_box(text, win, "Set Window Title", "New window title (UTF-8 / 日本語 OK):", cur) {
            win.set_title(text);
            status.clear();
            status.append("Updated title via modal DLGTEMPLATE input_box.");
            last_path.clear();
            format(last_path, "Title: ", text);
            win.invalidate();
        }
    } else if cmd == IDM_EDIT_COLOR {
        if win32::choose_color_dialog(accent, win) {
            status.clear();
            status.append("Updated accent color via ChooseColorW.");
            last_path.clear();
            format(last_path, "COLORREF = 0x", accent);
            win.invalidate();
        }
    } else if cmd == IDM_EDIT_COPY {
        if win32::clipboard_set_text(last_path, win) {
            status.clear();
            format(status, "Copied ", last_path.len, " bytes to clipboard.");
            win.invalidate();
        }
    } else if cmd == IDM_EDIT_PASTE {
        var clip: u8[>..] = [];
        if win32::clipboard_get_text(clip, win) {
            status.clear();
            format(status, "Pasted ", clip.len, " bytes from clipboard.");
            last_path.clear();
            last_path.append(clip);
            win.invalidate();
        } else {
            status.clear();
            status.append("Clipboard has no text.");
            win.invalidate();
        }
    } else if cmd == IDM_EDIT_DARK {
        dark = !dark;
        bar.set_checked(IDM_EDIT_DARK, dark);
        status.clear();
        status.append(if dark { "Theme: Dark" } else { "Theme: Light" });
        win.invalidate();
    } else if cmd == IDM_HELP_ABOUT {
        win32::message_box(
            "win32-goose\nWin32 Menus, Context Popups, and Common Dialogs in Goose!",
            "About win32-goose",
            win32::MB_OK | win32::MB_ICONINFORMATION,
            win
        );
    }
}

fn main() {
    win32::enable_dpi_awareness();

    let win = win32::create_window("win32-goose — Menus & Dialogs", 760, 420);
    guard win.is_valid() else {
        abort("create_window failed");
    }

    let bar = build_menu_bar();
    win.set_menu(bar);
    win.draw_menu_bar();
    win.show(win32::SW_SHOW);
    win.update();

    var dark = true;
    var accent = win32::rgb(96, 165, 250);
    var status: u8[>..] = [];
    status.append("Ready. Try File -> Open / Save / Folder, or Edit -> Title / Color.");
    var last_path: u8[>..] = [];
    last_path.append("(No selection yet)");

    var ev = win32::Event {};
    while win32::wait_event(ev) {
        if ev.msg == win32::WM_PAINT {
            render(win, dark, accent, status, last_path);
        } else if ev.msg == win32::WM_COMMAND {
            handle_command(ev.command_id, win, bar, dark, accent, status, last_path);
        } else if ev.msg == win32::WM_RBUTTONUP {
            let ctx = build_context_menu(dark);
            let picked = ctx.track_popup_client(win, ev.x, ev.y);
            ctx.destroy();
            if picked != 0 {
                handle_command(picked, win, bar, dark, accent, status, last_path);
            }
        } else if ev.msg == win32::WM_KEYDOWN && ev.key == win32::VK_ESCAPE {
            win.close();
        } else if ev.msg == win32::WM_CLOSE {
            win.destroy();
        } else if ev.msg == win32::WM_DESTROY {
            win32::post_quit_message(0);
        }
    }
}
