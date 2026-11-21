// Interactive WASAPI real-time sine-wave synth + XInput gamepad visualizer (rendered with Direct2D).
// Hold SPACE (or Gamepad A) to play a soft 440Hz tone via WASAPI; ESC to quit.
// Run with:   goose run examples/audio_and_gamepad.goose
// Build with: goose build examples/audio_and_gamepad.goose
import std;
import math;
import win32 from "..";

fn main() {
    win32::enable_dpi_awareness();
    win32::timeBeginPeriod(1);
    defer win32::timeEndPeriod(1);

    let win = win32::create_window("win32-goose — WASAPI Audio & XInput Gamepad", 720, 400);
    guard win.is_valid() else { abort("create_window failed"); }
    win.set_dark_titlebar(true);

    let d2d = win32::d2d_create(win, true);
    guard d2d.is_valid() else { abort("d2d_create failed"); }
    defer d2d.destroy();

    let title_font = d2d.create_font("Segoe UI", 24.0, 600);
    title_font.set_align(win32::D2D_ALIGN_CENTER, win32::D2D_VALIGN_CENTER);
    defer title_font.destroy();
    let body_font = d2d.create_font("Segoe UI", 16.0, 400);
    body_font.set_align(win32::D2D_ALIGN_CENTER, win32::D2D_VALIGN_CENTER);
    defer body_font.destroy();

    var fmt = win32::AudioFormat {};
    let audio = win32::wasapi_open_default(fmt);
    defer audio.close();

    let out_name = win32::audio_default_output_name();
    print("WASAPI Output: ", out_name, " (", fmt.sample_rate, " Hz, ", fmt.channels, " ch)");

    win.show(win32::SW_SHOW);
    win.update();

    var phase: f64 = 0.0;
    var space_down = false;
    var running = true;
    var ev = win32::Event {};
    var pcm: f32[>..] = [];

    while running {
        while win32::poll_event(ev) {
            if ev.msg == win32::WM_QUIT {
                running = false;
            } else if ev.msg == win32::WM_CLOSE {
                win.destroy();
            } else if ev.msg == win32::WM_DESTROY {
                win32::post_quit_message(0);
            } else if ev.msg == win32::WM_SIZE && ev.width > 0 && ev.height > 0 {
                d2d.resize(ev.width, ev.height);
            } else if ev.msg == win32::WM_KEYDOWN {
                if ev.key == win32::VK_ESCAPE { win.close(); }
                if ev.key == win32::VK_SPACE { space_down = true; }
            } else if ev.msg == win32::WM_KEYUP {
                if ev.key == win32::VK_SPACE { space_down = false; }
            }
        }
        guard running else { break; }

        var gp = win32::GamepadState {};
        win32::xinput_get_state(0, gp);
        let playing = space_down || gp.is_down(win32::XINPUT_A);
        let freq = 440.0 + gp.left_stick_x() * 180.0;

        // Feed WASAPI shared-mode render buffer.
        if audio.is_valid() && fmt.channels > 0 && fmt.sample_rate > 0 {
            let avail = audio.available_frames();
            let frames = if avail > 1024 { 1024 } else { avail };
            if frames > 0 {
                pcm.clear();
                let step = 6.283185307179586 * freq / fmt.sample_rate;
                for f in frames {
                    let s: f32 = if playing { (sin(phase) * 0.15) as f32 } else { 0.0 };
                    phase += step;
                    if phase > 6.283185307179586 { phase -= 6.283185307179586; }
                    for ch in fmt.channels {
                        pcm.push(s);
                    }
                }
                audio.write_f32(pcm);
            }
        }

        // Render live status with Direct2D.
        let rc = win.client_rect();
        let w = rc.width() as f32;
        let h = rc.height() as f32;

        d2d.begin_draw();
        d2d.clear(win32::colorf_rgb(15, 23, 42));

        let accent = if playing { win32::colorf_rgb(34, 197, 94) } else { win32::colorf_rgb(96, 165, 250) };
        d2d.draw_text("WASAPI Audio + XInput Gamepad", title_font, 0.0, 24.0, w, 44.0, win32::colorf_rgb(248, 250, 252));

        var info: u8[>..] = [];
        format(info, "Output: ", out_name, " (", fmt.sample_rate, " Hz, ", fmt.channels, " ch)");
        d2d.draw_text(info, body_font, 20.0, 74.0, w - 40.0, 32.0, win32::colorf_rgb(148, 163, 184));

        // Left stick circle indicator.
        let cx = w * 0.5;
        let cy = h * 0.56;
        d2d.draw_ellipse(cx, cy, 64.0, 64.0, win32::colorf(0.4, 0.5, 0.7, 0.6), 2.0);
        let sx = cx + gp.left_stick_x() * 52.0;
        let sy = cy - gp.left_stick_y() * 52.0;
        d2d.fill_ellipse(sx, sy, 14.0, 14.0, accent);

        var hint: u8[>..] = [];
        if gp.connected {
            format(hint, "Gamepad #0 Connected  |  Hold SPACE or A for tone (", freq as! i32, " Hz)");
        } else {
            format(hint, "No XInput Gamepad  |  Hold SPACE to play 440 Hz WASAPI tone  |  ESC to quit");
        }
        d2d.draw_text(hint, body_font, 20.0, h - 54.0, w - 40.0, 36.0, accent);
        d2d.end_draw();
    }
}
