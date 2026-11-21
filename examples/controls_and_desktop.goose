// Interactive Win32 Native Controls, DWM Dark Titlebar, Display Info,
// Drag & Drop, and System Tray demo.
// Run with:   goose run examples/controls_and_desktop.goose
// Build with: goose build examples/controls_and_desktop.goose
import std;
import win32 from "..";

let IDC_EDIT: u32 = 101;
let IDC_BTN_ADD: u32 = 102;
let IDC_BTN_BALLOON: u32 = 103;
let IDC_CHK_DARK: u32 = 104;
let IDC_LIST: u32 = 105;
let IDC_PROGRESS: u32 = 106;
let IDC_STATUS: u32 = 107;
let TRAY_ID: u32 = 1;

fn main() {
    win32::enable_dpi_awareness();
    win32::init_common_controls();

    let win = win32::create_window("win32-goose — Native Controls & Desktop", 560, 420);
    guard win.is_valid() else { abort("create_window failed"); }

    win.set_dark_titlebar(true);
    win.set_corner_preference(win32::DWMWCP_ROUND);
    win.accept_drop_files(true);
    win.tray_add(TRAY_ID, "win32-goose Controls Demo");
    defer win.tray_remove(TRAY_ID);

    win.create_label(100, "Add item or drag & drop files onto this window:", 16, 14, 380, 20);
    let edit = win.create_edit(IDC_EDIT, "Hello Win32 Controls!", 16, 38, 300, 26);
    win.create_button(IDC_BTN_ADD, "Add", 326, 38, 84, 26);
    win.create_button(IDC_BTN_BALLOON, "Notify", 418, 38, 120, 26);
    let chk_dark = win.create_checkbox(IDC_CHK_DARK, "Dark Titlebar (DWM)", 16, 74, 200, 24, true);

    let list = win.create_listbox(IDC_LIST, 16, 108, 522, 224);
    let pb = win.create_progressbar(IDC_PROGRESS, 16, 344, 522, 22, 100);
    let status = win.create_label(IDC_STATUS, "Ready.", 16, 376, 522, 24);

    // Populate display monitors into the ListBox.
    let nmon = win32::monitor_count();
    for i in nmon {
        var mi = win32::MonitorInfo {};
        var name: u8[>..] = [];
        if win32::monitor_info(i, mi, name) {
            var mode = win32::DisplayMode {};
            win32::current_display_mode(name, mode);
            var line: u8[>..] = [];
            format(line, "Monitor ", i, ": ", name, " (", mode.width, "x", mode.height, "@", mode.frequency, "Hz, DPI ", mi.dpi_x, ")");
            list.listbox_add(line);
        }
    }
    var progress: i32 = 25;
    pb.progress_set_pos(progress);

    win.show(win32::SW_SHOW);
    win.update();

    var ev = win32::Event {};
    while win32::wait_event(ev) {
        if ev.msg == win32::WM_COMMAND {
            if ev.command_id == IDC_BTN_ADD {
                let txt = edit.title();
                if txt.len > 0 {
                    list.listbox_add(txt);
                    progress = (progress + 15) % 105;
                    pb.progress_set_pos(progress);
                    status.set_title(str("Added: ", txt));
                }
            } else if ev.command_id == IDC_BTN_BALLOON {
                win.tray_balloon(TRAY_ID, "win32-goose", "Hello from Windows System Tray!", win32::NIIF_INFO);
                status.set_title("Sent tray balloon notification.");
            } else if ev.command_id == IDC_CHK_DARK {
                win.set_dark_titlebar(chk_dark.is_checked());
                status.set_title(if chk_dark.is_checked() { "DWM Dark Titlebar: ON" } else { "DWM Dark Titlebar: OFF" });
            }
        } else if ev.msg == win32::WM_DROPFILES {
            let cnt = win32::dropped_file_count();
            for i in cnt {
                var p: u8[>..] = [];
                if win32::dropped_file_path(i, p) {
                    list.listbox_add(str("[DROP] ", p));
                }
            }
            status.set_title(str("Dropped ", cnt, " file(s)."));
        } else if ev.msg == win32::WM_CLOSE {
            win.destroy();
        } else if ev.msg == win32::WM_DESTROY {
            win32::post_quit_message(0);
        }
    }
}
