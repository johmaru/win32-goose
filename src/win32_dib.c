#include <string.h>

#include "win32_internal.h"

uint8_t w32_blit_pixels(
    uint64_t hdc_u64,
    w32_u32_slice pixels,
    int32_t src_w,
    int32_t src_h,
    int32_t dst_x,
    int32_t dst_y,
    int32_t dst_w,
    int32_t dst_h
) {
    HDC hdc = (HDC)(uintptr_t)hdc_u64;
    if (!hdc || !pixels.data || src_w <= 0 || src_h <= 0 || dst_w <= 0 || dst_h <= 0) {
        return 0;
    }
    int64_t needed = (int64_t)src_w * (int64_t)src_h;
    if (pixels.len < needed) return 0;

    BITMAPINFO bmi;
    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = (LONG)src_w;
    bmi.bmiHeader.biHeight = -(LONG)src_h; /* negative = top-down scanlines */
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    SetStretchBltMode(hdc, COLORONCOLOR);
    int lines = StretchDIBits(
        hdc,
        (int)dst_x, (int)dst_y, (int)dst_w, (int)dst_h,
        0, 0, (int)src_w, (int)src_h,
        pixels.data,
        &bmi,
        DIB_RGB_COLORS,
        SRCCOPY
    );
    return lines != 0 && lines != GDI_ERROR ? 1 : 0;
}

uint8_t w32_present_pixels(uint64_t hwnd_u64, w32_u32_slice pixels, int32_t src_w, int32_t src_h) {
    HWND hwnd = (HWND)(uintptr_t)hwnd_u64;
    if (!hwnd) return 0;
    RECT rc;
    if (!GetClientRect(hwnd, &rc)) return 0;
    int32_t dst_w = (int32_t)(rc.right - rc.left);
    int32_t dst_h = (int32_t)(rc.bottom - rc.top);
    if (dst_w <= 0 || dst_h <= 0) return 0;

    HDC hdc = GetDC(hwnd);
    if (!hdc) return 0;
    uint8_t ok = w32_blit_pixels((uint64_t)(uintptr_t)hdc, pixels, src_w, src_h, 0, 0, dst_w, dst_h);
    ReleaseDC(hwnd, hdc);
    return ok;
}
