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
    return lines != 0 && lines != (int)GDI_ERROR ? 1 : 0;
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

/* ---- Retained 32bpp Premultiplied GDI Alpha Images ----------------------- */

#define W32_MAX_GDI_IMAGES 512

typedef BOOL (WINAPI *pfn_w32_AlphaBlend)(
    HDC, int, int, int, int, HDC, int, int, int, int, BLENDFUNCTION
);

typedef struct {
    int used;
    HBITMAP hbmp;
    int32_t w;
    int32_t h;
} w32_gdi_image;

static w32_gdi_image g_gdi_images[W32_MAX_GDI_IMAGES];
static struct {
    int tried;
    pfn_w32_AlphaBlend alpha_blend;
} g_gdi_ab;

static pfn_w32_AlphaBlend ensure_alpha_blend(void) {
    if (g_gdi_ab.alpha_blend) return g_gdi_ab.alpha_blend;
    if (g_gdi_ab.tried) return NULL;
    g_gdi_ab.tried = 1;
    HMODULE gdi = GetModuleHandleW(L"gdi32.dll");
    if (gdi) {
        g_gdi_ab.alpha_blend = (pfn_w32_AlphaBlend)(uintptr_t)GetProcAddress(gdi, "GdiAlphaBlend");
    }
    if (!g_gdi_ab.alpha_blend) {
        HMODULE msimg = LoadLibraryW(L"msimg32.dll");
        if (msimg) {
            g_gdi_ab.alpha_blend = (pfn_w32_AlphaBlend)(uintptr_t)GetProcAddress(msimg, "AlphaBlend");
        }
    }
    return g_gdi_ab.alpha_blend;
}

static uint32_t premul_bgra_px(uint32_t px) {
    uint32_t a = (px >> 24) & 0xFFu;
    if (a == 255u) return px;
    if (a == 0u) return 0u;
    uint32_t r = (((px >> 16) & 0xFFu) * a + 127u) / 255u;
    uint32_t g = (((px >> 8) & 0xFFu) * a + 127u) / 255u;
    uint32_t b = ((px & 0xFFu) * a + 127u) / 255u;
    return (a << 24) | (r << 16) | (g << 8) | b;
}

uint32_t w32_gdi_create_image(int32_t w, int32_t h, w32_u32_slice pixels_bgra) {
    if (!pixels_bgra.data || w <= 0 || h <= 0) return 0;
    int64_t count = (int64_t)w * (int64_t)h;
    if (pixels_bgra.len < count || count > 64LL * 1024LL * 1024LL) return 0;

    int slot = -1;
    for (int i = 0; i < W32_MAX_GDI_IMAGES; i++) {
        if (!g_gdi_images[i].used) { slot = i; break; }
    }
    if (slot < 0) return 0;

    BITMAPINFO bmi;
    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = (LONG)w;
    bmi.bmiHeader.biHeight = -(LONG)h; /* top-down */
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void *bits = NULL;
    HBITMAP hbmp = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!hbmp || !bits) {
        if (hbmp) DeleteObject(hbmp);
        return 0;
    }

    uint32_t *dst = (uint32_t *)bits;
    for (int64_t i = 0; i < count; i++) {
        dst[i] = premul_bgra_px(pixels_bgra.data[i]);
    }

    g_gdi_images[slot].used = 1;
    g_gdi_images[slot].hbmp = hbmp;
    g_gdi_images[slot].w = w;
    g_gdi_images[slot].h = h;
    return (uint32_t)(slot + 1);
}

void w32_gdi_destroy_image(uint32_t img_id) {
    if (img_id == 0 || img_id > W32_MAX_GDI_IMAGES) return;
    w32_gdi_image *img = &g_gdi_images[img_id - 1];
    if (!img->used) return;
    if (img->hbmp) DeleteObject(img->hbmp);
    memset(img, 0, sizeof(*img));
}

uint8_t w32_gdi_draw_image(
    uint64_t hdc_u64,
    uint32_t img_id,
    int32_t dst_x,
    int32_t dst_y,
    int32_t dst_w,
    int32_t dst_h,
    float opacity
) {
    HDC hdc = (HDC)(uintptr_t)hdc_u64;
    if (!hdc || img_id == 0 || img_id > W32_MAX_GDI_IMAGES || dst_w <= 0 || dst_h <= 0 || opacity <= 0.0f) {
        return 0;
    }
    w32_gdi_image *img = &g_gdi_images[img_id - 1];
    if (!img->used || !img->hbmp) return 0;

    HDC mem_dc = CreateCompatibleDC(hdc);
    if (!mem_dc) return 0;
    HGDIOBJ old_bmp = SelectObject(mem_dc, img->hbmp);

    float op = opacity > 1.0f ? 1.0f : opacity;
    BYTE alpha_byte = (BYTE)(op * 255.0f + 0.5f);
    BOOL ok = FALSE;

    pfn_w32_AlphaBlend fn_ab = ensure_alpha_blend();
    if (fn_ab) {
        BLENDFUNCTION bf;
        bf.BlendOp = AC_SRC_OVER;
        bf.BlendFlags = 0;
        bf.SourceConstantAlpha = alpha_byte;
        bf.AlphaFormat = AC_SRC_ALPHA;
        ok = fn_ab(hdc, (int)dst_x, (int)dst_y, (int)dst_w, (int)dst_h, mem_dc, 0, 0, (int)img->w, (int)img->h, bf);
    } else {
        SetStretchBltMode(hdc, HALFTONE);
        ok = StretchBlt(hdc, (int)dst_x, (int)dst_y, (int)dst_w, (int)dst_h, mem_dc, 0, 0, (int)img->w, (int)img->h, SRCCOPY);
    }

    SelectObject(mem_dc, old_bmp);
    DeleteDC(mem_dc);
    return ok ? 1 : 0;
}
