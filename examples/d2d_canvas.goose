// Interactive Direct2D + DirectWrite GPU 2D vector & text demo.
// Run with:   goose run examples/d2d_canvas.goose
// Build with: goose build examples/d2d_canvas.goose
import std;
import math;
import win32 from "..";

fn main() {
    win32::enable_dpi_awareness();

    let win = win32::create_window("win32-goose — Direct2D + DirectWrite", 820, 520);
    guard win.is_valid() else { abort("create_window failed"); }

    let d2d = win32::d2d_create(win, true);
    guard d2d.is_valid() else { abort("d2d_create failed"); }

    let title_font = d2d.create_font("Segoe UI", 28.0, 600);
    title_font.set_align(win32::D2D_ALIGN_CENTER, win32::D2D_VALIGN_CENTER);
    let body_font = d2d.create_font("Yu Gothic UI", 16.0, 400);
    body_font.set_align(win32::D2D_ALIGN_CENTER, win32::D2D_VALIGN_CENTER);

    win.show(win32::SW_SHOW);
    win.update();

    let t0 = win32::tick_count_ms();
    var mouse_x: f32 = 410.0;
    var mouse_y: f32 = 260.0;
    var running = true;
    var ev = win32::Event {};

    while running {
        while win32::poll_event(ev) {
            if ev.msg == win32::WM_QUIT || ev.msg == win32::WM_CLOSE {
                running = false;
            } else if ev.msg == win32::WM_DESTROY {
                win32::post_quit_message(0);
            } else if ev.msg == win32::WM_SIZE && ev.width > 0 && ev.height > 0 {
                d2d.resize(ev.width, ev.height);
            } else if ev.msg == win32::WM_MOUSEMOVE {
                mouse_x = ev.x;
                mouse_y = ev.y;
            } else if ev.msg == win32::WM_KEYDOWN && ev.key == win32::VK_ESCAPE {
                win.close();
            }
        }
        guard running else { break; }

        let rc = win.client_rect();
        let w = rc.width() as f32;
        let h = rc.height() as f32;
        let t = ((win32::tick_count_ms() - t0) as f64) * 0.002;

        d2d.begin_draw();
        d2d.set_identity();
        d2d.clear(win32::colorf_rgb(15, 23, 42));

        // Animated orbiting antialiased circles.
        for i in 12 {
            let a = t + i * 0.5235987755982988;
            let ox = w * 0.5 + (cos(a) * 190.0) as f32;
            let oy = h * 0.5 + (sin(a) * 130.0) as f32;
            let rad = (14.0 + sin(t * 2.0 + i) * 5.0) as f32;
            let r = (0.35 + 0.55 * (sin(a) * 0.5 + 0.5)) as f32;
            let g = (0.55 + 0.40 * (cos(a) * 0.5 + 0.5)) as f32;
            d2d.fill_ellipse(ox, oy, rad, rad, win32::colorf(r, g, 0.95, 0.75));
        }

        // Translucent frosted center card.
        let cw: f32 = 460.0;
        let ch: f32 = 190.0;
        let cx = (w - cw) * 0.5;
        let cy = (h - ch) * 0.5;
        d2d.fill_rounded_rect(cx, cy, cw, ch, 18.0, 18.0, win32::colorf(0.12, 0.16, 0.25, 0.85));
        d2d.draw_rounded_rect(cx, cy, cw, ch, 18.0, 18.0, win32::colorf(0.38, 0.65, 0.98, 0.9), 2.0);

        d2d.draw_text("Direct2D + DirectWrite", title_font, cx, cy + 28.0, cw, 48.0, win32::colorf_rgb(248, 250, 252));
        d2d.draw_text(
            "GPU アンチエイリアス 2D ベクタ & テキスト描画 (ESC で終了)",
            body_font,
            cx + 20.0,
            cy + 96.0,
            cw - 40.0,
            56.0,
            win32::colorf_rgb(148, 163, 184)
        );

        // Soft pointer halo.
        d2d.fill_ellipse(mouse_x, mouse_y, 20.0, 20.0, win32::colorf(0.98, 0.80, 0.08, 0.25));
        d2d.draw_ellipse(mouse_x, mouse_y, 10.0, 10.0, win32::colorf(0.98, 0.80, 0.08, 0.95), 2.0);

        d2d.end_draw();
    }

    body_font.destroy();
    title_font.destroy();
    d2d.destroy();
    win.destroy();
}
