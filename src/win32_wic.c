#define COBJMACROS
#include <stdlib.h>
#include <string.h>

#include "win32_internal.h"
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

    IWICBitmapFrameDecode *frame = NULL;
    IWICFormatConverter *conv = NULL;
    uint8_t ok = 0;

    if (SUCCEEDED(IWICBitmapDecoder_GetFrame(decoder, 0, &frame)) && frame) {
        UINT w = 0, h = 0;
        if (SUCCEEDED(IWICBitmapFrameDecode_GetSize(frame, &w, &h)) && w > 0 && h > 0) {
            if (SUCCEEDED(IWICImagingFactory_CreateFormatConverter(factory, &conv)) && conv) {
                hr = IWICFormatConverter_Initialize(
                    conv, (IWICBitmapSource *)frame, dst_fmt,
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
        }
        IWICBitmapFrameDecode_Release(frame);
    }

    IWICBitmapDecoder_Release(decoder);
    IWICImagingFactory_Release(factory);
    return ok;
}

uint8_t w32_image_load_bgra(w32_bytes path, gs_rref out_pixels_u32, int32_t *out_w, int32_t *out_h) {
    return wic_load_fmt(path, &W32_GUID_WICPixelFormat32bppBGRA, out_pixels_u32, out_w, out_h);
}

uint8_t w32_image_load_rgba(w32_bytes path, gs_rref out_pixels_u32, int32_t *out_w, int32_t *out_h) {
    return wic_load_fmt(path, &W32_GUID_WICPixelFormat32bppRGBA, out_pixels_u32, out_w, out_h);
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
