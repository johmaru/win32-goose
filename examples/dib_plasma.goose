// Interactive software DIB pixel buffer demo (plasma + cursor glow via StretchDIBits).
// Run with:   goose run examples/dib_plasma.goose
// Build with: goose build examples/dib_plasma.goose
import std;
import math;
import win32 from "..";

let FB_W: i32 = 400;
let FB_H: i32 = 260;

fn main() {
    win32::enable_dpi_awareness();

    let win = win32::create_window("win32-goose — DIB Software Framebuffer", 800, 520);
    guard win.is_valid() else { abort("create_window failed"); }
    win.show(win32::SW_SHOW);
    win.update();

    var pixels: u32[FB_W * FB_H] = [0; FB_W * FB_H];
    let t0 = win32::tick_count_ms();
    var mouse_x: i32 = FB_W / 2;
    var mouse_y: i32 = FB_H / 2;
    var running = true;
    var ev = win32::Event {};

    while running {
        while win32::poll_event(ev) {
            if ev.msg == win32::WM_QUIT {
                running = false;
            } else if ev.msg == win32::WM_CLOSE {
                win.destroy();
            } else if ev.msg == win32::WM_DESTROY {
                win32::post_quit_message(0);
            } else if ev.msg == win32::WM_KEYDOWN && ev.key == win32::VK_ESCAPE {
                win.close();
            } else if ev.msg == win32::WM_MOUSEMOVE {
                let rc = win.client_rect();
                if rc.width() > 0 && rc.height() > 0 {
                    mouse_x = (ev.x * FB_W) / rc.width();
                    mouse_y = (ev.y * FB_H) / rc.height();
                }
            }
        }
        guard running else { break; }

        let t = ((win32::tick_count_ms() - t0) as f64) * 0.003;
        var y: i32 = 0;
        while y < FB_H {
            let fy = (y as f64) * 0.045;
            let row: i64 = (y as i64) * FB_W;
            var x: i32 = 0;
            while x < FB_W {
                let fx = (x as f64) * 0.045;
                let v = sin(fx + t)
                      + sin(fy - t * 0.8)
                      + sin((fx + fy) * 0.7 + t * 0.6);
                let r = ((sin(v * 1.5 + 0.0) * 0.5 + 0.5) * 255.0) as! u8;
                let g = ((sin(v * 1.5 + 2.1) * 0.5 + 0.5) * 255.0) as! u8;
                let b = ((sin(v * 1.5 + 4.2) * 0.5 + 0.5) * 255.0) as! u8;
                pixels[row + x] = win32::xrgb(r, g, b);
                x += 1;
            }
            y += 1;
        }

        win32::fill_pixel_circle(pixels, FB_W, FB_H, mouse_x, mouse_y, 12, win32::xrgb(255, 255, 255));
        win32::present_pixels(win, pixels, FB_W, FB_H);
        win32::Sleep(16);
    }
}
