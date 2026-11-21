#define COBJMACROS
#include <stdlib.h>
#include <string.h>

#include "win32_internal.h"
#include <wincodec.h>

static int wic_ensure_com(void) {
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    return SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE;
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
        &CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory, (void **)&factory
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
                    size_t count = (size_t)w * (size_t)h;
                    uint32_t *buf = (uint32_t *)malloc(count * sizeof(uint32_t));
                    if (buf) {
                        hr = IWICFormatConverter_CopyPixels(
                            conv, NULL, (UINT)(w * 4), (UINT)(count * 4), (BYTE *)buf
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
    return wic_load_fmt(path, &GUID_WICPixelFormat32bppBGRA, out_pixels_u32, out_w, out_h);
}

uint8_t w32_image_load_rgba(w32_bytes path, gs_rref out_pixels_u32, int32_t *out_w, int32_t *out_h) {
    return wic_load_fmt(path, &GUID_WICPixelFormat32bppRGBA, out_pixels_u32, out_w, out_h);
}

static uint8_t wic_save_fmt(
    w32_bytes path,
    REFGUID container_format,
    w32_u32_slice pixels_bgra,
    int32_t w,
    int32_t h
) {
    if (!wic_ensure_com() || !pixels_bgra.data || w <= 0 || h <= 0) return 0;
    if (pixels_bgra.len < (int64_t)w * (int64_t)h) return 0;

    w32_wide_buf wpath;
    if (!w32_wide_init(&wpath, path) || wpath.len == 0) {
        w32_wide_free(&wpath);
        return 0;
    }

    IWICImagingFactory *factory = NULL;
    HRESULT hr = CoCreateInstance(
        &CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory, (void **)&factory
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
                        WICPixelFormatGUID pf = GUID_WICPixelFormat32bppBGRA;
                        if (SUCCEEDED(IWICBitmapFrameEncode_Initialize(frame, NULL)) &&
                            SUCCEEDED(IWICBitmapFrameEncode_SetSize(frame, (UINT)w, (UINT)h)) &&
                            SUCCEEDED(IWICBitmapFrameEncode_SetPixelFormat(frame, &pf)) &&
                            SUCCEEDED(IWICBitmapFrameEncode_WritePixels(
                                frame, (UINT)h, (UINT)(w * 4), (UINT)((size_t)w * (size_t)h * 4),
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
    return wic_save_fmt(path, &GUID_ContainerFormatPng, pixels_bgra, w, h);
}

uint8_t w32_image_save_bmp(w32_bytes path, w32_u32_slice pixels_bgra, int32_t w, int32_t h) {
    return wic_save_fmt(path, &GUID_ContainerFormatBmp, pixels_bgra, w, h);
}
