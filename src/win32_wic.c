#define COBJMACROS
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "win32_internal.h"
#include <shellapi.h>
#include <wincodec.h>

/* Inline WIC GUIDs so windowscodecs.lib import is unnecessary (COM loads windowscodecs.dll on demand). */
static const CLSID W32_CLSID_WICImagingFactory =
    { 0xcacaf262, 0x9370, 0x4615, { 0xa1, 0x3b, 0x9f, 0x55, 0x39, 0xda, 0x4c, 0x0a } };
static const IID W32_IID_IWICImagingFactory =
    { 0xec5ec8a9, 0xc395, 0x4314, { 0x9c, 0x77, 0x54, 0xd7, 0xa9, 0x35, 0xff, 0x70 } };
static const GUID W32_GUID_WICPixelFormat32bppBGRA =
    { 0x6fddc324, 0x4e03, 0x4bfe, { 0xb1, 0x85, 0x3d, 0x77, 0x76, 0x8d, 0xc9, 0x0f } };
static const GUID W32_GUID_WICPixelFormat32bppRGBA =
    { 0xf5c7ad2d, 0x6a8d, 0x43dd, { 0xa7, 0xa8, 0xa2, 0x99, 0x35, 0x26, 0x1a, 0xe9 } };
static const GUID W32_GUID_ContainerFormatPng =
    { 0x1b7cfaf4, 0x713f, 0x473c, { 0xbb, 0xcd, 0x61, 0x37, 0x42, 0x5f, 0xae, 0xaf } };
static const GUID W32_GUID_ContainerFormatBmp =
    { 0x0af1d87e, 0xfcfe, 0x4188, { 0xbd, 0xeb, 0xa7, 0x90, 0x64, 0x71, 0xcb, 0xe3 } };

static int g_wic_com_inited = 0;

static int wic_ensure_com(void) {
    if (g_wic_com_inited) return 1;
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE) {
        g_wic_com_inited = 1;
        return 1;
    }
    return 0;
}

static uint8_t wic_decode_source(
    IWICImagingFactory *factory,
    IWICBitmapSource *source,
    REFWICPixelFormatGUID dst_fmt,
    gs_rref out_pixels_u32,
    int32_t *out_w,
    int32_t *out_h
) {
    if (!factory || !source) return 0;
    UINT w = 0, h = 0;
    if (FAILED(IWICBitmapSource_GetSize(source, &w, &h)) || w == 0 || h == 0) return 0;

    IWICFormatConverter *conv = NULL;
    uint8_t ok = 0;
    if (SUCCEEDED(IWICImagingFactory_CreateFormatConverter(factory, &conv)) && conv) {
        HRESULT hr = IWICFormatConverter_Initialize(
            conv, source, dst_fmt,
            WICBitmapDitherTypeNone, NULL, 0.0, WICBitmapPaletteTypeCustom
        );
        if (SUCCEEDED(hr)) {
            uint64_t count = (uint64_t)w * (uint64_t)h;
            if (w <= UINT32_MAX / 4u && count <= UINT32_MAX / 4u) {
                uint32_t *buf = (uint32_t *)malloc((size_t)count * sizeof(uint32_t));
                if (buf) {
                    hr = IWICFormatConverter_CopyPixels(
                        conv, NULL, (UINT)(w * 4u), (UINT)(count * 4u), (BYTE *)buf
                    );
                    if (SUCCEEDED(hr)) {
                        w32_bld_append_u32(out_pixels_u32, buf, (int64_t)count);
                        if (out_w) *out_w = (int32_t)w;
                        if (out_h) *out_h = (int32_t)h;
                        ok = 1;
                    }
                    free(buf);
                }
            }
        }
        IWICFormatConverter_Release(conv);
    }
    return ok;
}

static uint8_t wic_decode_best_frame(
    IWICImagingFactory *factory,
    IWICBitmapDecoder *decoder,
    REFWICPixelFormatGUID dst_fmt,
    gs_rref out_pixels_u32,
    int32_t *out_w,
    int32_t *out_h
) {
    UINT frame_count = 0;
    if (FAILED(IWICBitmapDecoder_GetFrameCount(decoder, &frame_count)) || frame_count == 0) {
        frame_count = 1;
    }

    UINT best_idx = 0;
    if (frame_count > 1) {
        UINT limit = frame_count > 64u ? 64u : frame_count;
        uint64_t best_le256_area = 0;
        uint64_t best_any_area = 0;
        UINT best_le256_idx = 0;
        UINT best_any_idx = 0;
        int has_le256 = 0;

        for (UINT i = 0; i < limit; i++) {
            IWICBitmapFrameDecode *fr = NULL;
            if (SUCCEEDED(IWICBitmapDecoder_GetFrame(decoder, i, &fr)) && fr) {
                UINT fw = 0, fh = 0;
                if (SUCCEEDED(IWICBitmapFrameDecode_GetSize(fr, &fw, &fh)) && fw > 0 && fh > 0) {
                    uint64_t area = (uint64_t)fw * (uint64_t)fh;
                    if (fw <= 256u && fh <= 256u) {
                        if (!has_le256 || area >= best_le256_area) {
                            has_le256 = 1;
                            best_le256_area = area;
                            best_le256_idx = i;
                        }
                    }
                    if (area >= best_any_area) {
                        best_any_area = area;
                        best_any_idx = i;
                    }
                }
                IWICBitmapFrameDecode_Release(fr);
            }
        }
        best_idx = has_le256 ? best_le256_idx : best_any_idx;
    }

    IWICBitmapFrameDecode *frame = NULL;
    uint8_t ok = 0;
    if (SUCCEEDED(IWICBitmapDecoder_GetFrame(decoder, best_idx, &frame)) && frame) {
        ok = wic_decode_source(factory, (IWICBitmapSource *)frame, dst_fmt, out_pixels_u32, out_w, out_h);
        IWICBitmapFrameDecode_Release(frame);
    }
    return ok;
}

static uint8_t wic_load_fmt(
    w32_bytes path,
    REFWICPixelFormatGUID dst_fmt,
    gs_rref out_pixels_u32,
    int32_t *out_w,
    int32_t *out_h
) {
    if (!wic_ensure_com()) return 0;
    w32_wide_buf wpath;
    if (!w32_wide_init(&wpath, path) || wpath.len == 0) {
        w32_wide_free(&wpath);
        return 0;
    }

    IWICImagingFactory *factory = NULL;
    HRESULT hr = CoCreateInstance(
        &W32_CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &W32_IID_IWICImagingFactory, (void **)&factory
    );
    if (FAILED(hr) || !factory) {
        w32_wide_free(&wpath);
        return 0;
    }

    IWICBitmapDecoder *decoder = NULL;
    hr = IWICImagingFactory_CreateDecoderFromFilename(
        factory, wpath.ptr, NULL, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder
    );
    w32_wide_free(&wpath);
    if (FAILED(hr) || !decoder) {
        IWICImagingFactory_Release(factory);
        return 0;
    }

    uint8_t ok = wic_decode_best_frame(factory, decoder, dst_fmt, out_pixels_u32, out_w, out_h);
    IWICBitmapDecoder_Release(decoder);
    IWICImagingFactory_Release(factory);
    return ok;
}

static uint8_t wic_load_mem_fmt(
    w32_bytes data,
    REFWICPixelFormatGUID dst_fmt,
    gs_rref out_pixels_u32,
    int32_t *out_w,
    int32_t *out_h
) {
    if (!wic_ensure_com() || !data.data || data.len <= 0 || data.len > 32 * 1024 * 1024) return 0;

    IWICImagingFactory *factory = NULL;
    HRESULT hr = CoCreateInstance(
        &W32_CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &W32_IID_IWICImagingFactory, (void **)&factory
    );
    if (FAILED(hr) || !factory) return 0;

    IWICStream *stream = NULL;
    IWICBitmapDecoder *decoder = NULL;
    uint8_t ok = 0;

    if (SUCCEEDED(IWICImagingFactory_CreateStream(factory, &stream)) && stream) {
        if (SUCCEEDED(IWICStream_InitializeFromMemory(stream, (BYTE *)data.data, (DWORD)data.len))) {
            if (SUCCEEDED(IWICImagingFactory_CreateDecoderFromStream(
                    factory, (IStream *)stream, NULL, WICDecodeMetadataCacheOnDemand, &decoder)) && decoder) {
                ok = wic_decode_best_frame(factory, decoder, dst_fmt, out_pixels_u32, out_w, out_h);
                IWICBitmapDecoder_Release(decoder);
            }
        }
        IWICStream_Release(stream);
    }

    IWICImagingFactory_Release(factory);
    return ok;
}

uint8_t w32_image_load_bgra(w32_bytes path, gs_rref out_pixels_u32, int32_t *out_w, int32_t *out_h) {
    return wic_load_fmt(path, &W32_GUID_WICPixelFormat32bppBGRA, out_pixels_u32, out_w, out_h);
}

uint8_t w32_image_load_rgba(w32_bytes path, gs_rref out_pixels_u32, int32_t *out_w, int32_t *out_h) {
    return wic_load_fmt(path, &W32_GUID_WICPixelFormat32bppRGBA, out_pixels_u32, out_w, out_h);
}

uint8_t w32_image_load_bgra_mem(w32_bytes data, gs_rref out_pixels_u32, int32_t *out_w, int32_t *out_h) {
    return wic_load_mem_fmt(data, &W32_GUID_WICPixelFormat32bppBGRA, out_pixels_u32, out_w, out_h);
}

uint8_t w32_image_load_rgba_mem(w32_bytes data, gs_rref out_pixels_u32, int32_t *out_w, int32_t *out_h) {
    return wic_load_mem_fmt(data, &W32_GUID_WICPixelFormat32bppRGBA, out_pixels_u32, out_w, out_h);
}

uint8_t w32_image_resize_bgra(
    w32_u32_slice src_pixels,
    int32_t src_w,
    int32_t src_h,
    int32_t dst_w,
    int32_t dst_h,
    gs_rref out_pixels_u32
) {
    if (!wic_ensure_com() || !src_pixels.data || src_w <= 0 || src_h <= 0 || dst_w <= 0 || dst_h <= 0) {
        return 0;
    }
    uint64_t src_count = (uint64_t)src_w * (uint64_t)src_h;
    uint64_t dst_count = (uint64_t)dst_w * (uint64_t)dst_h;
    if (src_pixels.len < (int64_t)src_count || src_count > UINT32_MAX / 4u || dst_count > UINT32_MAX / 4u) {
        return 0;
    }
    if (src_w == dst_w && src_h == dst_h) {
        w32_bld_append_u32(out_pixels_u32, src_pixels.data, (int64_t)src_count);
        return 1;
    }

    IWICImagingFactory *factory = NULL;
    HRESULT hr = CoCreateInstance(
        &W32_CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &W32_IID_IWICImagingFactory, (void **)&factory
    );
    if (FAILED(hr) || !factory) return 0;

    IWICBitmap *bmp = NULL;
    IWICBitmapScaler *scaler = NULL;
    uint8_t ok = 0;

    hr = IWICImagingFactory_CreateBitmapFromMemory(
        factory,
        (UINT)src_w,
        (UINT)src_h,
        &W32_GUID_WICPixelFormat32bppBGRA,
        (UINT)((uint64_t)src_w * 4u),
        (UINT)(src_count * 4u),
        (BYTE *)src_pixels.data,
        &bmp
    );
    if (SUCCEEDED(hr) && bmp) {
        if (SUCCEEDED(IWICImagingFactory_CreateBitmapScaler(factory, &scaler)) && scaler) {
            hr = IWICBitmapScaler_Initialize(
                scaler,
                (IWICBitmapSource *)bmp,
                (UINT)dst_w,
                (UINT)dst_h,
                WICBitmapInterpolationModeFant
            );
            if (SUCCEEDED(hr)) {
                ok = wic_decode_source(
                    factory, (IWICBitmapSource *)scaler, &W32_GUID_WICPixelFormat32bppBGRA,
                    out_pixels_u32, NULL, NULL
                );
            }
            IWICBitmapScaler_Release(scaler);
        }
        IWICBitmap_Release(bmp);
    }

    IWICImagingFactory_Release(factory);
    return ok;
}

/* ---- Windows Executable / Shortcut / Shell Icon Extraction --------------- */

static const GUID W32_IID_IShellItemImageFactory =
    { 0xbcc18b79, 0xba16, 0x442f, { 0x80, 0xc4, 0x8a, 0x59, 0xc3, 0x0c, 0x46, 0x3b } };

typedef UINT (WINAPI *pfn_w32_PrivateExtractIconsW)(
    LPCWSTR, int, int, int, HICON *, UINT *, UINT, UINT
);
typedef HRESULT (WINAPI *pfn_w32_SHCreateItemFromParsingName)(
    LPCWSTR, void *, REFIID, void **
);

static uint8_t wic_extract_from_hicon(
    IWICImagingFactory *factory,
    HICON hicon,
    gs_rref out_pixels_u32,
    int32_t *out_w,
    int32_t *out_h
) {
    if (!factory || !hicon) return 0;
    IWICBitmap *wic_bmp = NULL;
    if (FAILED(IWICImagingFactory_CreateBitmapFromHICON(factory, hicon, &wic_bmp)) || !wic_bmp) {
        return 0;
    }
    uint8_t ok = wic_decode_source(
        factory, (IWICBitmapSource *)wic_bmp, &W32_GUID_WICPixelFormat32bppBGRA,
        out_pixels_u32, out_w, out_h
    );
    IWICBitmap_Release(wic_bmp);
    return ok;
}

static uint8_t wic_extract_shell_item_image(
    const wchar_t *resolved,
    int32_t desired_size,
    gs_rref out_pixels_u32,
    int32_t *out_w,
    int32_t *out_h
) {
    HMODULE shell32 = GetModuleHandleW(L"shell32.dll");
    if (!shell32) shell32 = LoadLibraryW(L"shell32.dll");
    if (!shell32) return 0;
    pfn_w32_SHCreateItemFromParsingName fn_create =
        (pfn_w32_SHCreateItemFromParsingName)(uintptr_t)GetProcAddress(shell32, "SHCreateItemFromParsingName");
    if (!fn_create) return 0;

    void *siif = NULL;
    HRESULT hr = fn_create(resolved, NULL, &W32_IID_IShellItemImageFactory, &siif);
    if (FAILED(hr) || !siif) return 0;

    typedef HRESULT (STDMETHODCALLTYPE *pfn_GetImage)(void *, SIZE, int, HBITMAP *);
    typedef ULONG   (STDMETHODCALLTYPE *pfn_Release)(void *);
    void **vtbl = *(void ***)siif;
    SIZE req_sz = { (LONG)desired_size, (LONG)desired_size };
    HBITMAP hbm = NULL;
    /* SIIGBF_ICONONLY (0x4) | SIIGBF_BIGGERSIZEOK (0x1) */
    hr = ((pfn_GetImage)vtbl[3])(siif, req_sz, 0x05, &hbm);
    ((pfn_Release)vtbl[2])(siif);
    if (FAILED(hr) || !hbm) return 0;

    BITMAP bm;
    memset(&bm, 0, sizeof(bm));
    uint8_t ok = 0;
    if (GetObjectW(hbm, sizeof(bm), &bm) == sizeof(bm) && bm.bmWidth > 0 && bm.bmHeight > 0 &&
        bm.bmWidth <= 1024 && bm.bmHeight <= 1024) {
        int32_t bw = (int32_t)bm.bmWidth;
        int32_t bh = (int32_t)bm.bmHeight;
        uint64_t count = (uint64_t)bw * (uint64_t)bh;
        uint32_t *buf = (uint32_t *)malloc((size_t)count * sizeof(uint32_t));
        if (buf) {
            BITMAPINFO bmi;
            memset(&bmi, 0, sizeof(bmi));
            bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bmi.bmiHeader.biWidth = (LONG)bw;
            bmi.bmiHeader.biHeight = -(LONG)bh; /* top-down */
            bmi.bmiHeader.biPlanes = 1;
            bmi.bmiHeader.biBitCount = 32;
            bmi.bmiHeader.biCompression = BI_RGB;

            HDC hdc = GetDC(NULL);
            if (hdc) {
                int lines = GetDIBits(hdc, hbm, 0, (UINT)bh, buf, &bmi, DIB_RGB_COLORS);
                ReleaseDC(NULL, hdc);
                if (lines > 0) {
                    int has_alpha = 0;
                    for (uint64_t i = 0; i < count; i++) {
                        if ((buf[i] >> 24) != 0u) {
                            has_alpha = 1;
                            break;
                        }
                    }
                    if (has_alpha) {
                        /* Un-premultiply ARGB from IShellItemImageFactory into straight 32bpp BGRA. */
                        for (uint64_t i = 0; i < count; i++) {
                            uint32_t px = buf[i];
                            uint32_t a = (px >> 24) & 0xFFu;
                            if (a == 0u) {
                                buf[i] = 0u;
                            } else if (a < 255u) {
                                uint32_t r = ((px >> 16) & 0xFFu) * 255u / a;
                                uint32_t g = ((px >> 8) & 0xFFu) * 255u / a;
                                uint32_t b = (px & 0xFFu) * 255u / a;
                                if (r > 255u) r = 255u;
                                if (g > 255u) g = 255u;
                                if (b > 255u) b = 255u;
                                buf[i] = (a << 24) | (r << 16) | (g << 8) | b;
                            }
                        }
                        w32_bld_append_u32(out_pixels_u32, buf, (int64_t)count);
                        if (out_w) *out_w = bw;
                        if (out_h) *out_h = bh;
                        ok = 1;
                    }
                }
            }
            free(buf);
        }
    }
    DeleteObject(hbm);
    return ok;
}

static int wic_has_direct_icon_ext(const wchar_t *p) {
    size_t n = wcslen(p);
    if (n < 4) return 0;
    const wchar_t *ext = p + n - 4;
    return (_wcsicmp(ext, L".exe") == 0 || _wcsicmp(ext, L".dll") == 0 || _wcsicmp(ext, L".ico") == 0) ? 1 : 0;
}

uint8_t w32_extract_file_icon_bgra(
    w32_bytes path,
    int32_t desired_size,
    gs_rref out_pixels_u32,
    int32_t *out_w,
    int32_t *out_h
) {
    if (!wic_ensure_com() || !path.data || path.len <= 0) return 0;
    int32_t sz = desired_size;
    if (sz < 16) sz = 16;
    if (sz > 256) sz = 256;

    /* Trim ASCII whitespace and outer double quotes. */
    int64_t lo = 0, hi = path.len;
    while (lo < hi && (path.data[lo] == ' ' || path.data[lo] == '\t' || path.data[lo] == '\r' || path.data[lo] == '\n')) lo++;
    while (hi > lo && (path.data[hi - 1] == ' ' || path.data[hi - 1] == '\t' || path.data[hi - 1] == '\r' || path.data[hi - 1] == '\n')) hi--;
    if (hi - lo >= 2 && path.data[lo] == '"' && path.data[hi - 1] == '"') {
        lo++;
        hi--;
    }
    if (hi <= lo) return 0;

    w32_bytes clean_bytes = { path.data + lo, hi - lo };
    w32_wide_buf wpath;
    if (!w32_wide_init(&wpath, clean_bytes) || wpath.len == 0) {
        w32_wide_free(&wpath);
        return 0;
    }

    wchar_t resolved_buf[MAX_PATH * 4];
    const wchar_t *resolved = wpath.ptr;
    int has_sep = (wcschr(wpath.ptr, L'\\') != NULL || wcschr(wpath.ptr, L'/') != NULL) ? 1 : 0;
    if (!has_sep) {
        DWORD found = SearchPathW(NULL, wpath.ptr, L".exe", MAX_PATH * 4, resolved_buf, NULL);
        if (found == 0 || found >= MAX_PATH * 4) {
            found = SearchPathW(NULL, wpath.ptr, NULL, MAX_PATH * 4, resolved_buf, NULL);
        }
        if (found > 0 && found < MAX_PATH * 4) {
            resolved = resolved_buf;
        }
    }

    DWORD attrs = GetFileAttributesW(resolved);
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        w32_wide_free(&wpath);
        return 0;
    }

    IWICImagingFactory *factory = NULL;
    HRESULT hr = CoCreateInstance(
        &W32_CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &W32_IID_IWICImagingFactory, (void **)&factory
    );
    if (FAILED(hr) || !factory) {
        w32_wide_free(&wpath);
        return 0;
    }

    uint8_t ok = 0;

    /* 1. Direct high-res icon extraction for .exe / .dll / .ico via PrivateExtractIconsW. */
    if (wic_has_direct_icon_ext(resolved)) {
        HMODULE user32 = GetModuleHandleW(L"user32.dll");
        pfn_w32_PrivateExtractIconsW fn_pei = user32
            ? (pfn_w32_PrivateExtractIconsW)(uintptr_t)GetProcAddress(user32, "PrivateExtractIconsW")
            : NULL;
        if (fn_pei) {
            HICON hicon = NULL;
            UINT icon_id = 0;
            UINT extracted = fn_pei(resolved, 0, (int)sz, (int)sz, &hicon, &icon_id, 1, 0);
            if (extracted > 0 && extracted != 0xFFFFFFFFu && hicon) {
                ok = wic_extract_from_hicon(factory, hicon, out_pixels_u32, out_w, out_h);
                DestroyIcon(hicon);
            }
        }
    }

    /* 2. High-res Shell icon via IShellItemImageFactory (handles .lnk shortcuts & modern apps). */
    if (!ok) {
        ok = wic_extract_shell_item_image(resolved, sz, out_pixels_u32, out_w, out_h);
    }

    /* 3. Final fallback via SHGetFileInfoW (properly converts legacy AND/XOR masks via WIC). */
    if (!ok) {
        SHFILEINFOW sfi;
        memset(&sfi, 0, sizeof(sfi));
        if (SHGetFileInfoW(resolved, 0, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_LARGEICON) && sfi.hIcon) {
            ok = wic_extract_from_hicon(factory, sfi.hIcon, out_pixels_u32, out_w, out_h);
            DestroyIcon(sfi.hIcon);
        }
    }

    IWICImagingFactory_Release(factory);
    w32_wide_free(&wpath);
    return ok;
}

static uint8_t wic_save_fmt(
    w32_bytes path,
    REFGUID container_format,
    w32_u32_slice pixels_bgra,
    int32_t w,
    int32_t h
) {
    if (!wic_ensure_com() || !pixels_bgra.data || w <= 0 || h <= 0) return 0;
    uint64_t pixel_count = (uint64_t)w * (uint64_t)h;
    if ((uint64_t)w > UINT32_MAX / 4u || pixel_count > UINT32_MAX / 4u) return 0;
    if (pixels_bgra.len < (int64_t)pixel_count) return 0;

    w32_wide_buf wpath;
    if (!w32_wide_init(&wpath, path) || wpath.len == 0) {
        w32_wide_free(&wpath);
        return 0;
    }

    IWICImagingFactory *factory = NULL;
    HRESULT hr = CoCreateInstance(
        &W32_CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &W32_IID_IWICImagingFactory, (void **)&factory
    );
    if (FAILED(hr) || !factory) {
        w32_wide_free(&wpath);
        return 0;
    }

    IWICStream *stream = NULL;
    IWICBitmapEncoder *encoder = NULL;
    IWICBitmapFrameEncode *frame = NULL;
    uint8_t ok = 0;

    if (SUCCEEDED(IWICImagingFactory_CreateStream(factory, &stream)) && stream) {
        if (SUCCEEDED(IWICStream_InitializeFromFilename(stream, wpath.ptr, GENERIC_WRITE))) {
            if (SUCCEEDED(IWICImagingFactory_CreateEncoder(factory, container_format, NULL, &encoder)) && encoder) {
                if (SUCCEEDED(IWICBitmapEncoder_Initialize(encoder, (IStream *)stream, WICBitmapEncoderNoCache))) {
                    if (SUCCEEDED(IWICBitmapEncoder_CreateNewFrame(encoder, &frame, NULL)) && frame) {
                        WICPixelFormatGUID pf = W32_GUID_WICPixelFormat32bppBGRA;
                        if (SUCCEEDED(IWICBitmapFrameEncode_Initialize(frame, NULL)) &&
                            SUCCEEDED(IWICBitmapFrameEncode_SetSize(frame, (UINT)w, (UINT)h)) &&
                            SUCCEEDED(IWICBitmapFrameEncode_SetPixelFormat(frame, &pf)) &&
                            IsEqualGUID(&pf, &W32_GUID_WICPixelFormat32bppBGRA) &&
                            SUCCEEDED(IWICBitmapFrameEncode_WritePixels(
                                frame, (UINT)h, (UINT)((uint64_t)w * 4u), (UINT)(pixel_count * 4u),
                                (BYTE *)pixels_bgra.data)) &&
                            SUCCEEDED(IWICBitmapFrameEncode_Commit(frame)) &&
                            SUCCEEDED(IWICBitmapEncoder_Commit(encoder))) {
                            ok = 1;
                        }
                        IWICBitmapFrameEncode_Release(frame);
                    }
                }
                IWICBitmapEncoder_Release(encoder);
            }
        }
        IWICStream_Release(stream);
    }

    w32_wide_free(&wpath);
    IWICImagingFactory_Release(factory);
    return ok;
}

uint8_t w32_image_save_png(w32_bytes path, w32_u32_slice pixels_bgra, int32_t w, int32_t h) {
    return wic_save_fmt(path, &W32_GUID_ContainerFormatPng, pixels_bgra, w, h);
}

uint8_t w32_image_save_bmp(w32_bytes path, w32_u32_slice pixels_bgra, int32_t w, int32_t h) {
    return wic_save_fmt(path, &W32_GUID_ContainerFormatBmp, pixels_bgra, w, h);
}
