// Direct2D + DirectWrite GPU-accelerated 2D vector and text rendering API.
namespace win32;

import types;

// Matches w32_colorf in include/win32_shim.h (16 bytes, 4x f32).
struct ColorF {
    r: f32 = 0.0,
    g: f32 = 0.0,
    b: f32 = 0.0,
    a: f32 = 1.0,
}

fn colorf(r: f32, g: f32, b: f32, a: f32 = 1.0) -> ColorF {
    ColorF { r, g, b, a }
}

fn colorf_rgb(r: u8, g: u8, b: u8, a: f32 = 1.0) -> ColorF {
    ColorF {
        r: r / 255.0,
        g: g / 255.0,
        b: b / 255.0,
        a: a,
    }
}

// DirectWrite text alignment constants.
let D2D_ALIGN_LEADING: i32 = 0;
let D2D_ALIGN_TRAILING: i32 = 1;
let D2D_ALIGN_CENTER: i32 = 2;

let D2D_VALIGN_NEAR: i32 = 0;
let D2D_VALIGN_FAR: i32 = 1;
let D2D_VALIGN_CENTER: i32 = 2;

struct D2dTarget { id: u32 = 0 }
struct D2dFont { id: u32 = 0 }
struct D2dBitmap { id: u32 = 0 }

fn is_valid(t: D2dTarget) -> bool { t.id != 0 }
fn is_valid(f: D2dFont) -> bool { f.id != 0 }
fn is_valid(b: D2dBitmap) -> bool { b.id != 0 }

extern fn shim_d2d_create(hwnd: u64, vsync: bool) -> u32 = "w32_d2d_create";
extern fn shim_d2d_destroy(target_id: u32) = "w32_d2d_destroy";
extern fn shim_d2d_resize(target_id: u32, width: i32, height: i32) -> bool = "w32_d2d_resize";
extern fn shim_d2d_get_pixel_size(target_id: u32, out_w: i32&, out_h: i32&) -> bool = "w32_d2d_get_pixel_size";
extern fn shim_d2d_begin_draw(target_id: u32) = "w32_d2d_begin_draw";
extern fn shim_d2d_end_draw(target_id: u32) -> bool = "w32_d2d_end_draw";
extern fn shim_d2d_clear(target_id: u32, color: ColorF) = "w32_d2d_clear";
extern fn shim_d2d_set_transform(target_id: u32, m11: f32, m12: f32, m21: f32, m22: f32, dx: f32, dy: f32) = "w32_d2d_set_transform";
extern fn shim_d2d_push_clip(target_id: u32, x: f32, y: f32, w: f32, h: f32) = "w32_d2d_push_clip";
extern fn shim_d2d_pop_clip(target_id: u32) = "w32_d2d_pop_clip";
extern fn shim_d2d_draw_line(target_id: u32, x0: f32, y0: f32, x1: f32, y1: f32, color: ColorF, stroke_width: f32) = "w32_d2d_draw_line";
extern fn shim_d2d_draw_rect(target_id: u32, x: f32, y: f32, w: f32, h: f32, color: ColorF, stroke_width: f32) = "w32_d2d_draw_rect";
extern fn shim_d2d_fill_rect(target_id: u32, x: f32, y: f32, w: f32, h: f32, color: ColorF) = "w32_d2d_fill_rect";
extern fn shim_d2d_draw_rounded_rect(target_id: u32, x: f32, y: f32, w: f32, h: f32, rx: f32, ry: f32, color: ColorF, stroke_width: f32) = "w32_d2d_draw_rounded_rect";
extern fn shim_d2d_fill_rounded_rect(target_id: u32, x: f32, y: f32, w: f32, h: f32, rx: f32, ry: f32, color: ColorF) = "w32_d2d_fill_rounded_rect";
extern fn shim_d2d_draw_ellipse(target_id: u32, cx: f32, cy: f32, rx: f32, ry: f32, color: ColorF, stroke_width: f32) = "w32_d2d_draw_ellipse";
extern fn shim_d2d_fill_ellipse(target_id: u32, cx: f32, cy: f32, rx: f32, ry: f32, color: ColorF) = "w32_d2d_fill_ellipse";
extern fn shim_d2d_create_font(target_id: u32, family: const u8[:], size: f32, weight: i32, italic: bool) -> u32 = "w32_d2d_create_font";
extern fn shim_d2d_set_font_align(font_id: u32, h_align: i32, v_align: i32) = "w32_d2d_set_font_align";
extern fn shim_d2d_set_font_word_wrap(font_id: u32, wrap: bool) = "w32_d2d_set_font_word_wrap";
extern fn shim_d2d_destroy_font(font_id: u32) = "w32_d2d_destroy_font";
extern fn shim_d2d_draw_text(target_id: u32, text: const u8[:], font_id: u32, x: f32, y: f32, w: f32, h: f32, color: ColorF) = "w32_d2d_draw_text";
extern fn shim_d2d_measure_text(target_id: u32, text: const u8[:], font_id: u32, out_w: f32&, out_h: f32&) -> bool = "w32_d2d_measure_text";
extern fn shim_d2d_create_bitmap(target_id: u32, w: i32, h: i32, pixels_bgra: const u32[:]) -> u32 = "w32_d2d_create_bitmap";
extern fn shim_d2d_destroy_bitmap(bmp_id: u32) = "w32_d2d_destroy_bitmap";
extern fn shim_d2d_draw_bitmap(target_id: u32, bmp_id: u32, x: f32, y: f32, w: f32, h: f32, opacity: f32) = "w32_d2d_draw_bitmap";

fn d2d_create(win: Hwnd, vsync: bool = true) -> D2dTarget {
    return D2dTarget { id: shim_d2d_create(win.raw, vsync) };
}

fn destroy(t: D2dTarget) {
    shim_d2d_destroy(t.id);
}

fn resize(t: D2dTarget, width: i32, height: i32) -> bool {
    return shim_d2d_resize(t.id, width, height);
}

fn pixel_size(t: D2dTarget) -> i32, i32 {
    var w: i32 = 0;
    var h: i32 = 0;
    if !shim_d2d_get_pixel_size(t.id, w, h) {
        return 0, 0;
    }
    return w, h;
}

fn begin_draw(t: D2dTarget) {
    shim_d2d_begin_draw(t.id);
}

fn end_draw(t: D2dTarget) -> bool {
    return shim_d2d_end_draw(t.id);
}

fn clear(t: D2dTarget, color: ColorF) {
    shim_d2d_clear(t.id, color);
}

fn set_transform(t: D2dTarget, m11: f32, m12: f32, m21: f32, m22: f32, dx: f32, dy: f32) {
    shim_d2d_set_transform(t.id, m11, m12, m21, m22, dx, dy);
}

fn set_identity(t: D2dTarget) {
    shim_d2d_set_transform(t.id, 1.0, 0.0, 0.0, 1.0, 0.0, 0.0);
}

fn push_clip(t: D2dTarget, x: f32, y: f32, w: f32, h: f32) {
    shim_d2d_push_clip(t.id, x, y, w, h);
}

fn pop_clip(t: D2dTarget) {
    shim_d2d_pop_clip(t.id);
}

fn draw_line(t: D2dTarget, x0: f32, y0: f32, x1: f32, y1: f32, color: ColorF, stroke_width: f32 = 1.0) {
    shim_d2d_draw_line(t.id, x0, y0, x1, y1, color, stroke_width);
}

fn draw_rect(t: D2dTarget, x: f32, y: f32, w: f32, h: f32, color: ColorF, stroke_width: f32 = 1.0) {
    shim_d2d_draw_rect(t.id, x, y, w, h, color, stroke_width);
}

fn fill_rect(t: D2dTarget, x: f32, y: f32, w: f32, h: f32, color: ColorF) {
    shim_d2d_fill_rect(t.id, x, y, w, h, color);
}

fn draw_rounded_rect(t: D2dTarget, x: f32, y: f32, w: f32, h: f32, rx: f32, ry: f32, color: ColorF, stroke_width: f32 = 1.0) {
    shim_d2d_draw_rounded_rect(t.id, x, y, w, h, rx, ry, color, stroke_width);
}

fn fill_rounded_rect(t: D2dTarget, x: f32, y: f32, w: f32, h: f32, rx: f32, ry: f32, color: ColorF) {
    shim_d2d_fill_rounded_rect(t.id, x, y, w, h, rx, ry, color);
}

fn draw_ellipse(t: D2dTarget, cx: f32, cy: f32, rx: f32, ry: f32, color: ColorF, stroke_width: f32 = 1.0) {
    shim_d2d_draw_ellipse(t.id, cx, cy, rx, ry, color, stroke_width);
}

fn fill_ellipse(t: D2dTarget, cx: f32, cy: f32, rx: f32, ry: f32, color: ColorF) {
    shim_d2d_fill_ellipse(t.id, cx, cy, rx, ry, color);
}

fn create_font(t: D2dTarget, family: const u8[:] = "Segoe UI", size: f32 = 18.0, weight: i32 = 400, italic: bool = false) -> D2dFont {
    return D2dFont { id: shim_d2d_create_font(t.id, family, size, weight, italic) };
}

fn set_align(f: D2dFont, h_align: i32, v_align: i32 = D2D_VALIGN_NEAR) {
    shim_d2d_set_font_align(f.id, h_align, v_align);
}

fn set_word_wrap(f: D2dFont, wrap: bool) {
    shim_d2d_set_font_word_wrap(f.id, wrap);
}

fn destroy(f: D2dFont) {
    shim_d2d_destroy_font(f.id);
}

fn draw_text(t: D2dTarget, text: const u8[:], font: D2dFont, x: f32, y: f32, w: f32, h: f32, color: ColorF) {
    shim_d2d_draw_text(t.id, text, font.id, x, y, w, h, color);
}

fn measure_text(t: D2dTarget, text: const u8[:], font: D2dFont) -> f32, f32 {
    var w: f32 = 0.0;
    var h: f32 = 0.0;
    if !shim_d2d_measure_text(t.id, text, font.id, w, h) {
        return 0.0, 0.0;
    }
    return w, h;
}

fn create_bitmap(t: D2dTarget, w: i32, h: i32, pixels_bgra: const u32[:]) -> D2dBitmap {
    return D2dBitmap { id: shim_d2d_create_bitmap(t.id, w, h, pixels_bgra) };
}

fn destroy(b: D2dBitmap) {
    shim_d2d_destroy_bitmap(b.id);
}

fn draw_bitmap(t: D2dTarget, bmp: D2dBitmap, x: f32, y: f32, w: f32, h: f32, opacity: f32 = 1.0) {
    shim_d2d_draw_bitmap(t.id, bmp.id, x, y, w, h, opacity);
}
