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
    let min_x = if x0 < 0 { 0 } else { x0 };
    let min_y = if y0 < 0 { 0 } else { y0 };
    let max_x = if x0 + rw > w { w } else { x0 + rw };
    let max_y = if y0 + rh > h { h } else { y0 + rh };
    var y = min_y;
    while y < max_y {
        let row: i64 = (y as i64) * w;
        var x = min_x;
        while x < max_x {
            pixels[row + x] = color;
            x += 1;
        }
        y += 1;
    }
}

fn fill_pixel_circle(pixels: u32[:], w: i32, h: i32, cx: i32, cy: i32, radius: i32, color: u32) {
    if radius <= 0 { return; }
    let r2 = radius * radius;
    let min_y = if cy - radius < 0 { 0 } else { cy - radius };
    let max_y = if cy + radius >= h { h - 1 } else { cy + radius };
    let min_x = if cx - radius < 0 { 0 } else { cx - radius };
    let max_x = if cx + radius >= w { w - 1 } else { cx + radius };
    var y = min_y;
    while y <= max_y {
        let dy = y - cy;
        let row: i64 = (y as i64) * w;
        var x = min_x;
        while x <= max_x {
            let dx = x - cx;
            if dx * dx + dy * dy <= r2 {
                pixels[row + x] = color;
            }
            x += 1;
        }
        y += 1;
    }
}
