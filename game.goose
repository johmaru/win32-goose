// Win32 Gaming API: XInput gamepad polling & vibration + winmm high-res timer.
namespace win32;

import constants;
import types;

extern "winmm" {
    fn timeBeginPeriod(period_ms: u32) -> u32;
    fn timeEndPeriod(period_ms: u32) -> u32;
    fn timeGetTime() -> u32;
}

extern fn xinput_get_state(user_index: u32, out: GamepadState&) -> bool = "w32_xinput_get_state";
extern fn xinput_set_vibration(user_index: u32, left_motor: u16, right_motor: u16) -> bool = "w32_xinput_set_vibration";

fn is_down(gp: const GamepadState&, mask: u16) -> bool {
    return gp.connected && (gp.buttons & mask) != 0;
}

fn norm_axis(v: i16, deadzone: i32) -> f32 {
    let iv: i32 = v;
    if iv > -deadzone && iv < deadzone { return 0.0; }
    let f = iv / 32767.0;
    if f > 1.0 { return 1.0; }
    if f < -1.0 { return -1.0; }
    return f as f32;
}

fn left_stick_x(gp: const GamepadState&, deadzone: i32 = 7849) -> f32 { norm_axis(gp.thumb_lx, deadzone) }
fn left_stick_y(gp: const GamepadState&, deadzone: i32 = 7849) -> f32 { norm_axis(gp.thumb_ly, deadzone) }
fn right_stick_x(gp: const GamepadState&, deadzone: i32 = 8689) -> f32 { norm_axis(gp.thumb_rx, deadzone) }
fn right_stick_y(gp: const GamepadState&, deadzone: i32 = 8689) -> f32 { norm_axis(gp.thumb_ry, deadzone) }
fn left_trigger_f(gp: const GamepadState&) -> f32 { gp.left_trigger / 255.0 }
fn right_trigger_f(gp: const GamepadState&) -> f32 { gp.right_trigger / 255.0 }
