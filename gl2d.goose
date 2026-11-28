// Hardware-accelerated OpenGL 3.3 Core 2D SDF + GPU VRAM atlas renderer.
namespace win32;

import types;
import d2d;

struct Gl2dTarget { id: u32 = 0 }
struct Gl2dFont { id: u32 = 0 }
struct Gl2dImage { id: u32 = 0 }

fn is_valid(t: Gl2dTarget) -> bool { t.id != 0 }
fn is_valid(f: Gl2dFont) -> bool { f.id != 0 }
fn is_valid(img: Gl2dImage) -> bool { img.id != 0 }

let GL2D_ALIGN_START: i32 = 0;
let GL2D_ALIGN_CENTER: i32 = 1;
let GL2D_ALIGN_END: i32 = 2;

extern fn shim_gl2d_create(hwnd: u64, vsync: bool) -> u32 = "w32_gl2d_create";
extern fn shim_gl2d_destroy(target_id: u32) = "w32_gl2d_destroy";
extern fn shim_gl2d_resize(target_id: u32, width: i32, height: i32) -> bool = "w32_gl2d_resize";
extern fn shim_gl2d_begin_draw(target_id: u32, clear_color: ColorF) = "w32_gl2d_begin_draw";
extern fn shim_gl2d_end_draw(target_id: u32) -> bool = "w32_gl2d_end_draw";
extern fn shim_gl2d_push_clip(target_id: u32, x: f32, y: f32, w: f32, h: f32) = "w32_gl2d_push_clip";
extern fn shim_gl2d_pop_clip(target_id: u32) = "w32_gl2d_pop_clip";
extern fn shim_gl2d_clear_text_cache(target_id: u32) = "w32_gl2d_clear_text_cache";
extern fn shim_gl2d_draw_line(target_id: u32, x0: f32, y0: f32, x1: f32, y1: f32, color: ColorF, stroke_width: f32) = "w32_gl2d_draw_line";
extern fn shim_gl2d_draw_rect(target_id: u32, x: f32, y: f32, w: f32, h: f32, color: ColorF, stroke_width: f32) = "w32_gl2d_draw_rect";
extern fn shim_gl2d_fill_rect(target_id: u32, x: f32, y: f32, w: f32, h: f32, color: ColorF) = "w32_gl2d_fill_rect";
extern fn shim_gl2d_draw_rounded_rect(target_id: u32, x: f32, y: f32, w: f32, h: f32, r: f32, color: ColorF, stroke_width: f32) = "w32_gl2d_draw_rounded_rect";
extern fn shim_gl2d_fill_rounded_rect(target_id: u32, x: f32, y: f32, w: f32, h: f32, r: f32, color: ColorF) = "w32_gl2d_fill_rounded_rect";
extern fn shim_gl2d_draw_ellipse(target_id: u32, cx: f32, cy: f32, rx: f32, ry: f32, color: ColorF, stroke_width: f32) = "w32_gl2d_draw_ellipse";
extern fn shim_gl2d_fill_ellipse(target_id: u32, cx: f32, cy: f32, rx: f32, ry: f32, color: ColorF) = "w32_gl2d_fill_ellipse";
extern fn shim_gl2d_create_font(target_id: u32, family: const u8[:], size: f32, weight: i32, italic: bool) -> u32 = "w32_gl2d_create_font";
extern fn shim_gl2d_destroy_font(font_id: u32) = "w32_gl2d_destroy_font";
extern fn shim_gl2d_draw_text(
    target_id: u32,
    text: const u8[:],
    font_id: u32,
    x: f32, y: f32, w: f32, h: f32,
    h_align: i32,
    v_align: i32,
    color: ColorF
) = "w32_gl2d_draw_text";
extern fn shim_gl2d_readback_pixel(target_id: u32, x: i32, y: i32, out_rgba: u32&) -> bool = "w32_gl2d_readback_pixel";
extern fn shim_gl2d_create_image(target_id: u32, w: i32, h: i32, pixels_bgra: const u32[:]) -> u32 = "w32_gl2d_create_image";
extern fn shim_gl2d_destroy_image(img_id: u32) = "w32_gl2d_destroy_image";
extern fn shim_gl2d_draw_image(target_id: u32, img_id: u32, x: f32, y: f32, w: f32, h: f32, opacity: f32) = "w32_gl2d_draw_image";

fn gl2d_create(win: Hwnd, vsync: bool = true) -> Gl2dTarget {
    return Gl2dTarget { id: shim_gl2d_create(win.raw, vsync) };
}

fn destroy(t: Gl2dTarget) {
    shim_gl2d_destroy(t.id);
}

fn resize(t: Gl2dTarget, width: i32, height: i32) -> bool {
    return shim_gl2d_resize(t.id, width, height);
}

fn begin_draw(t: Gl2dTarget, clear_color: ColorF) {
    shim_gl2d_begin_draw(t.id, clear_color);
}

fn end_draw(t: Gl2dTarget) -> bool {
    return shim_gl2d_end_draw(t.id);
}

fn push_clip(t: Gl2dTarget, x: f32, y: f32, w: f32, h: f32) {
    shim_gl2d_push_clip(t.id, x, y, w, h);
}

fn pop_clip(t: Gl2dTarget) {
    shim_gl2d_pop_clip(t.id);
}

fn clear_text_cache(t: Gl2dTarget) {
    shim_gl2d_clear_text_cache(t.id);
}

fn draw_line(t: Gl2dTarget, x0: f32, y0: f32, x1: f32, y1: f32, color: ColorF, stroke_width: f32 = 1.0) {
    shim_gl2d_draw_line(t.id, x0, y0, x1, y1, color, stroke_width);
}

fn draw_rect(t: Gl2dTarget, x: f32, y: f32, w: f32, h: f32, color: ColorF, stroke_width: f32 = 1.0) {
    shim_gl2d_draw_rect(t.id, x, y, w, h, color, stroke_width);
}

fn fill_rect(t: Gl2dTarget, x: f32, y: f32, w: f32, h: f32, color: ColorF) {
    shim_gl2d_fill_rect(t.id, x, y, w, h, color);
}

fn draw_rounded_rect(t: Gl2dTarget, x: f32, y: f32, w: f32, h: f32, r: f32, color: ColorF, stroke_width: f32 = 1.0) {
    shim_gl2d_draw_rounded_rect(t.id, x, y, w, h, r, color, stroke_width);
}

fn fill_rounded_rect(t: Gl2dTarget, x: f32, y: f32, w: f32, h: f32, r: f32, color: ColorF) {
    shim_gl2d_fill_rounded_rect(t.id, x, y, w, h, r, color);
}

fn draw_ellipse(t: Gl2dTarget, cx: f32, cy: f32, rx: f32, ry: f32, color: ColorF, stroke_width: f32 = 1.0) {
    shim_gl2d_draw_ellipse(t.id, cx, cy, rx, ry, color, stroke_width);
}

fn fill_ellipse(t: Gl2dTarget, cx: f32, cy: f32, rx: f32, ry: f32, color: ColorF) {
    shim_gl2d_fill_ellipse(t.id, cx, cy, rx, ry, color);
}

fn create_font(t: Gl2dTarget, family: const u8[:] = "Segoe UI", size: f32 = 18.0, weight: i32 = 400, italic: bool = false) -> Gl2dFont {
    return Gl2dFont { id: shim_gl2d_create_font(t.id, family, size, weight, italic) };
}

fn destroy(f: Gl2dFont) {
    shim_gl2d_destroy_font(f.id);
}

fn draw_text(
    t: Gl2dTarget,
    text: const u8[:],
    font: Gl2dFont,
    x: f32, y: f32, w: f32, h: f32,
    h_align: i32,
    v_align: i32,
    color: ColorF
) {
    shim_gl2d_draw_text(t.id, text, font.id, x, y, w, h, h_align, v_align, color);
}

fn readback_pixel(t: Gl2dTarget, x: i32, y: i32, out_rgba: u32&) -> bool {
    return shim_gl2d_readback_pixel(t.id, x, y, out_rgba);
}

fn create_image(t: Gl2dTarget, w: i32, h: i32, pixels_bgra: const u32[:]) -> Gl2dImage {
    return Gl2dImage { id: shim_gl2d_create_image(t.id, w, h, pixels_bgra) };
}

fn destroy(img: Gl2dImage) {
    shim_gl2d_destroy_image(img.id);
}

fn draw_image(t: Gl2dTarget, img: Gl2dImage, x: f32, y: f32, w: f32, h: f32, opacity: f32 = 1.0) {
    shim_gl2d_draw_image(t.id, img.id, x, y, w, h, opacity);
}
