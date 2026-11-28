// Software DIB pixel buffer helpers (32-bit 0xAARRGGBB / BGRA little-endian).
// Blits any Goose `u32[:]` pixel array directly into an Hdc or Hwnd via
// Win32 StretchDIBits with top-down scanlines.
namespace win32;

import types;

extern fn shim_blit_pixels(
    hdc: u64,
    pixels: const u32[:],
    src_w: i32,
    src_h: i32,
    dst_x: i32,
    dst_y: i32,
    dst_w: i32,
    dst_h: i32
) -> bool = "w32_blit_pixels";

extern fn shim_present_pixels(
    hwnd: u64,
    pixels: const u32[:],
    src_w: i32,
    src_h: i32
) -> bool = "w32_present_pixels";

struct GdiImage { id: u32 = 0 }
fn is_valid(img: GdiImage) -> bool { img.id != 0 }

extern fn shim_gdi_create_image(w: i32, h: i32, pixels_bgra: const u32[:]) -> u32 = "w32_gdi_create_image";
extern fn shim_gdi_destroy_image(img_id: u32) = "w32_gdi_destroy_image";
extern fn shim_gdi_draw_image(
    hdc: u64,
    img_id: u32,
    dst_x: i32,
    dst_y: i32,
    dst_w: i32,
    dst_h: i32,
    opacity: f32
) -> bool = "w32_gdi_draw_image";

fn gdi_create_image(w: i32, h: i32, pixels_bgra: const u32[:]) -> GdiImage {
    return GdiImage { id: shim_gdi_create_image(w, h, pixels_bgra) };
}

fn destroy(img: GdiImage) {
    shim_gdi_destroy_image(img.id);
}

fn draw_image(dc: Hdc, img: GdiImage, dst_x: i32, dst_y: i32, dst_w: i32, dst_h: i32, opacity: f32 = 1.0) -> bool {
    return shim_gdi_draw_image(dc.raw, img.id, dst_x, dst_y, dst_w, dst_h, opacity);
}

// Packs a 32-bit DIB pixel (0xAARRGGBB; note GDI COLORREF uses rgb() = 0x00BBGGRR).
fn argb(r: u8, g: u8, b: u8, a: u8 = 255) -> u32 {
    b | ((g as u32) << 8) | ((r as u32) << 16) | ((a as u32) << 24)
}

fn xrgb(r: u8, g: u8, b: u8) -> u32 {
    argb(r, g, b, 255)
}

fn blit_pixels(
    dc: Hdc,
    pixels: const u32[:],
    src_w: i32,
    src_h: i32,
    dst_x: i32,
    dst_y: i32,
    dst_w: i32,
    dst_h: i32
) -> bool {
    return shim_blit_pixels(dc.raw, pixels, src_w, src_h, dst_x, dst_y, dst_w, dst_h);
}

fn present_pixels(win: Hwnd, pixels: const u32[:], src_w: i32, src_h: i32) -> bool {
    return shim_present_pixels(win.raw, pixels, src_w, src_h);
}

// --- Pure Goose pixel drawing helpers ---------------------------------------

fn clear_pixels(pixels: u32[:], color: u32) {
    for i in pixels.len {
        pixels[i] = color;
    }
}

fn put_pixel(pixels: u32[:], w: i32, h: i32, x: i32, y: i32, color: u32) {
    if x < 0 || y < 0 || x >= w || y >= h { return; }
    let idx: i64 = (y as i64) * w + x;
    if idx >= 0 && idx < pixels.len {
        pixels[idx] = color;
    }
}

fn fill_pixel_rect(pixels: u32[:], w: i32, h: i32, x0: i32, y0: i32, rw: i32, rh: i32, color: u32) {
    if w <= 0 || h <= 0 || rw <= 0 || rh <= 0 { return; }
    let w64: i64 = w;
    let h64: i64 = h;
    let x0_64: i64 = x0;
    let y0_64: i64 = y0;
    let x1_64: i64 = x0_64 + rw;
    let y1_64: i64 = y0_64 + rh;
    let min_x: i64 = if x0_64 < 0 { 0 } else { x0_64 };
    let min_y: i64 = if y0_64 < 0 { 0 } else { y0_64 };
    let max_x: i64 = if x1_64 > w64 { w64 } else { x1_64 };
    let max_y: i64 = if y1_64 > h64 { h64 } else { y1_64 };
    var y = min_y;
    while y < max_y {
        let row = y * w64;
        var x = min_x;
        while x < max_x {
            let idx = row + x;
            if idx < pixels.len {
                pixels[idx] = color;
            }
            x += 1;
        }
        y += 1;
    }
}

fn fill_pixel_circle(pixels: u32[:], w: i32, h: i32, cx: i32, cy: i32, radius: i32, color: u32) {
    if w <= 0 || h <= 0 || radius <= 0 { return; }
    let w64: i64 = w;
    let h64: i64 = h;
    let cx64: i64 = cx;
    let cy64: i64 = cy;
    let r64: i64 = radius;
    let r2: i64 = r64 * r64;
    let min_y: i64 = if cy64 - r64 < 0 { 0 } else { cy64 - r64 };
    let max_y: i64 = if cy64 + r64 >= h64 { h64 - 1 } else { cy64 + r64 };
    let min_x: i64 = if cx64 - r64 < 0 { 0 } else { cx64 - r64 };
    let max_x: i64 = if cx64 + r64 >= w64 { w64 - 1 } else { cx64 + r64 };
    var y = min_y;
    while y <= max_y {
        let dy = y - cy64;
        let dy2 = dy * dy;
        let row = y * w64;
        var x = min_x;
        while x <= max_x {
            let dx = x - cx64;
            if dx * dx + dy2 <= r2 {
                let idx = row + x;
                if idx < pixels.len {
                    pixels[idx] = color;
                }
            }
            x += 1;
        }
        y += 1;
    }
}
