// Interactive Win32 window demo in Goose.
// Run with:   goose run examples/hello_window.goose
// Build with: goose build examples/hello_window.goose
import std;
import win32 from "..";

fn render(win: win32::Hwnd, mouse_x: i32, mouse_y: i32, clicks: i32) {
    var rc = win32::Rect {};
    let dc = win.begin_paint(rc);
    guard dc.is_valid();
    defer win.end_paint();

    // Background.
    dc.fill_rect(rc, win32::rgb(22, 26, 34));

    // Accent card in the center.
    let card_w: i32 = 460;
    let card_h: i32 = 220;
    let cx = (rc.width() - card_w) / 2;
    let cy = (rc.height() - card_h) / 2;
    let card_brush = win32::create_solid_brush(win32::rgb(34, 41, 54));
    let card_pen = win32::create_pen(win32::PS_SOLID, 2, win32::rgb(96, 165, 250));
    let old_brush = dc.select_brush(card_brush);
    let old_pen = dc.select_pen(card_pen);
    dc.round_rect(cx, cy, cx + card_w, cy + card_h, 16, 16);
    dc.select_object(old_brush);
    dc.select_object(old_pen);
    card_brush.delete();
    card_pen.delete();

    // Pointer indicator circle.
    let dot_brush = win32::create_solid_brush(win32::rgb(250, 204, 21));
    let prev_b = dc.select_brush(dot_brush);
    dc.ellipse(mouse_x - 8, mouse_y - 8, mouse_x + 8, mouse_y + 8);
    dc.select_object(prev_b);
    dot_brush.delete();

    // Text overlay.
    let old_font = dc.select_object(win32::stock_object(win32::DEFAULT_GUI_FONT));
    dc.set_bk_mode(win32::TRANSPARENT);
    dc.set_text_color(win32::rgb(241, 245, 249));

    var title_rc = win32::Rect { left: cx + 20, top: cy + 32, right: cx + card_w - 20, bottom: cy + 80 };
    dc.draw_text("Hello, Win32 from Goose!", title_rc, win32::DT_CENTER | win32::DT_SINGLELINE);

    var info: u8[>..] = [];
    format(info, "Mouse: (", mouse_x, ", ", mouse_y, ")   Clicks: ", clicks, "   DPI: ", win.dpi());
    var info_rc = win32::Rect { left: cx + 20, top: cy + 96, right: cx + card_w - 20, bottom: cy + 140 };
    dc.set_text_color(win32::rgb(148, 163, 184));
    dc.draw_text(info, info_rc, win32::DT_CENTER | win32::DT_SINGLELINE);

    var hint_rc = win32::Rect { left: cx + 20, top: cy + 156, right: cx + card_w - 20, bottom: cy + 196 };
    dc.set_text_color(win32::rgb(96, 165, 250));
    dc.draw_text("Click anywhere to increment  |  Press ESC to quit", hint_rc, win32::DT_CENTER | win32::DT_SINGLELINE);

    dc.select_object(old_font);
}

fn main() {
    win32::enable_dpi_awareness();

    let win = win32::create_window("Goose Win32 Window (日本語OK)", 720, 440);
    guard win.is_valid() else {
        let err = win32::last_error();
        abort(str("CreateWindowExW failed (", err, "): ", win32::format_error(err)));
    }

    win.show(win32::SW_SHOW);
    win.update();

    var mouse_x: i32 = 360;
    var mouse_y: i32 = 220;
    var clicks: i32 = 0;

    var ev = win32::Event {};
    while win32::wait_event(ev) {
        if ev.msg == win32::WM_PAINT {
            render(win, mouse_x, mouse_y, clicks);
        } else if ev.msg == win32::WM_MOUSEMOVE {
            mouse_x = ev.x;
            mouse_y = ev.y;
            win.invalidate();
        } else if ev.msg == win32::WM_LBUTTONDOWN {
            clicks += 1;
            mouse_x = ev.x;
            mouse_y = ev.y;
            win.invalidate();
        } else if ev.msg == win32::WM_KEYDOWN && ev.key == win32::VK_ESCAPE {
            win.close();
        } else if ev.msg == win32::WM_CLOSE {
            win.destroy();
        } else if ev.msg == win32::WM_DESTROY {
            win32::post_quit_message(0);
        }
    }
}
