#define COBJMACROS
#include <string.h>

#include "win32_internal.h"
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <propvarutil.h>

static int g_mf_started = 0;

uint8_t w32_mf_startup(void) {
    if (g_mf_started) return 1;
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET))) return 0;
    g_mf_started = 1;
    return 1;
}

void w32_mf_shutdown(void) {
    if (!g_mf_started) return;
    MFShutdown();
    g_mf_started = 0;
}

#define W32_MAX_VIDEOS 8

typedef struct {
    int used;
    IMFSourceReader *reader;
    uint32_t width;
    uint32_t height;
    int32_t stride;
    uint32_t fps_num;
    uint32_t fps_den;
    int64_t duration_100ns;
} w32_video_stream;

static w32_video_stream g_videos[W32_MAX_VIDEOS];

static w32_video_stream *get_video(uint32_t id) {
    if (id == 0 || id > W32_MAX_VIDEOS) return NULL;
    return g_videos[id - 1].used ? &g_videos[id - 1] : NULL;
}

uint32_t w32_video_open(w32_bytes path, w32_video_info *out_info) {
    if (!w32_mf_startup()) return 0;
    int slot = -1;
    for (int i = 0; i < W32_MAX_VIDEOS; i++) {
        if (!g_videos[i].used) { slot = i; break; }
    }
    if (slot < 0) return 0;

    w32_wide_buf wpath;
    if (!w32_wide_init(&wpath, path) || wpath.len == 0) {
        w32_wide_free(&wpath);
        return 0;
    }

    IMFAttributes *attrs = NULL;
    MFCreateAttributes(&attrs, 1);
    if (attrs) {
        IMFAttributes_SetUINT32(attrs, &MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
    }

    IMFSourceReader *reader = NULL;
    HRESULT hr = MFCreateSourceReaderFromURL(wpath.ptr, attrs, &reader);
    if (attrs) IMFAttributes_Release(attrs);
    w32_wide_free(&wpath);
    if (FAILED(hr) || !reader) return 0;

    /* Ask Media Foundation to convert decoded video frames to RGB32 (matching dib.goose u32[:]). */
    IMFMediaType *out_type = NULL;
    hr = MFCreateMediaType(&out_type);
    if (SUCCEEDED(hr) && out_type) {
        IMFMediaType_SetGUID(out_type, &MF_MT_MAJOR_TYPE, &MFMediaType_Video);
        IMFMediaType_SetGUID(out_type, &MF_MT_SUBTYPE, &MFVideoFormat_RGB32);
        hr = IMFSourceReader_SetCurrentMediaType(reader, (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, out_type);
        IMFMediaType_Release(out_type);
    } else if (SUCCEEDED(hr)) {
        hr = E_OUTOFMEMORY;
    }
    if (FAILED(hr)) {
        IMFSourceReader_Release(reader);
        return 0;
    }

    uint32_t width = 0, height = 0, fps_num = 0, fps_den = 1;
    int32_t stride = 0;
    IMFMediaType *cur = NULL;
    if (SUCCEEDED(IMFSourceReader_GetCurrentMediaType(reader, (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, &cur)) && cur) {
        UINT64 packed_sz = 0;
        if (SUCCEEDED(IMFMediaType_GetUINT64(cur, &MF_MT_FRAME_SIZE, &packed_sz))) {
            width = (uint32_t)(packed_sz >> 32);
            height = (uint32_t)(packed_sz & 0xFFFFFFFFu);
        }
        UINT32 raw_stride = 0;
        if (SUCCEEDED(IMFMediaType_GetUINT32(cur, &MF_MT_DEFAULT_STRIDE, &raw_stride))) {
            stride = (int32_t)raw_stride;
        }
        UINT64 packed_fps = 0;
        if (SUCCEEDED(IMFMediaType_GetUINT64(cur, &MF_MT_FRAME_RATE, &packed_fps))) {
            fps_num = (uint32_t)(packed_fps >> 32);
            fps_den = (uint32_t)(packed_fps & 0xFFFFFFFFu);
        }
        IMFMediaType_Release(cur);
    }
    if (stride == 0 && width > 0) {
        LONG calc_stride = 0;
        if (SUCCEEDED(MFGetStrideForBitmapInfoHeader(BI_RGB, width, &calc_stride)) && calc_stride != 0) {
            stride = (int32_t)calc_stride;
        } else {
            stride = (int32_t)(width * 4u);
        }
    }

    int64_t duration = 0;
    PROPVARIANT pv;
    PropVariantInit(&pv);
    if (SUCCEEDED(IMFSourceReader_GetPresentationAttribute(
            reader, (DWORD)MF_SOURCE_READER_MEDIASOURCE, &MF_PD_DURATION, &pv))) {
        if (pv.vt == VT_UI8) duration = (int64_t)pv.uhVal.QuadPart;
        else if (pv.vt == VT_I8) duration = (int64_t)pv.hVal.QuadPart;
        PropVariantClear(&pv);
    }

    w32_video_stream *v = &g_videos[slot];
    v->used = 1;
    v->reader = reader;
    v->width = width;
    v->height = height;
    v->stride = stride;
    v->fps_num = fps_num;
    v->fps_den = fps_den ? fps_den : 1;
    v->duration_100ns = duration;

    if (out_info) {
        out_info->width = v->width;
        out_info->height = v->height;
        out_info->fps_num = v->fps_num;
        out_info->fps_den = v->fps_den;
        out_info->duration_100ns = v->duration_100ns;
    }
    return (uint32_t)(slot + 1);
}

uint8_t w32_video_read_frame_rgb32(uint32_t vid_id, w32_mut_u32_slice out_pixels, int64_t *out_ts_100ns) {
    w32_video_stream *v = get_video(vid_id);
    if (!v || !out_pixels.data) return 0;
    int64_t needed = (int64_t)v->width * (int64_t)v->height;
    if (needed <= 0 || out_pixels.len < needed) return 0;

    for (;;) {
        DWORD flags = 0;
        LONGLONG ts = 0;
        IMFSample *sample = NULL;
        HRESULT hr = IMFSourceReader_ReadSample(
            v->reader, (DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, NULL, &flags, &ts, &sample
        );
        if (FAILED(hr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM)) {
            if (sample) IMFSample_Release(sample);
            return 0;
        }
        if (!sample) continue;

        IMFMediaBuffer *buf = NULL;
        uint8_t ok = 0;
        if (SUCCEEDED(IMFSample_ConvertToContiguousBuffer(sample, &buf)) && buf) {
            size_t row_bytes = (size_t)v->width * 4u;
            uint8_t *dst_base = (uint8_t *)out_pixels.data;

            IMF2DBuffer *buf2d = NULL;
            if (SUCCEEDED(IMFMediaBuffer_QueryInterface(buf, &IID_IMF2DBuffer, (void **)&buf2d)) && buf2d) {
                BYTE *scanline0 = NULL;
                LONG pitch = 0;
                if (SUCCEEDED(IMF2DBuffer_Lock2D(buf2d, &scanline0, &pitch)) && scanline0) {
                    /* Lock2D always points scanline0 to the top row; pitch may be negative (bottom-up). */
                    for (uint32_t y = 0; y < v->height; y++) {
                        const BYTE *src_row = scanline0 + (ptrdiff_t)y * (ptrdiff_t)pitch;
                        memcpy(dst_base + (size_t)y * row_bytes, src_row, row_bytes);
                    }
                    IMF2DBuffer_Unlock2D(buf2d);
                    if (out_ts_100ns) *out_ts_100ns = (int64_t)ts;
                    ok = 1;
                }
                IMF2DBuffer_Release(buf2d);
            } else {
                BYTE *src = NULL;
                DWORD cur_len = 0;
                if (SUCCEEDED(IMFMediaBuffer_Lock(buf, &src, NULL, &cur_len)) && src) {
                    int32_t st = v->stride != 0 ? v->stride : (int32_t)row_bytes;
                    size_t abs_stride = st < 0 ? (size_t)(-(int64_t)st) : (size_t)st;
                    if (abs_stride >= row_bytes &&
                        (uint64_t)abs_stride * (uint64_t)v->height <= (uint64_t)cur_len) {
                        for (uint32_t y = 0; y < v->height; y++) {
                            size_t src_y = st < 0 ? (size_t)(v->height - 1u - y) : (size_t)y;
                            memcpy(dst_base + (size_t)y * row_bytes, src + src_y * abs_stride, row_bytes);
                        }
                        if (out_ts_100ns) *out_ts_100ns = (int64_t)ts;
                        ok = 1;
                    } else if (cur_len >= (DWORD)(row_bytes * v->height)) {
                        memcpy(dst_base, src, row_bytes * v->height);
                        if (out_ts_100ns) *out_ts_100ns = (int64_t)ts;
                        ok = 1;
                    }
                    IMFMediaBuffer_Unlock(buf);
                }
            }
            IMFMediaBuffer_Release(buf);
        }
        IMFSample_Release(sample);
        return ok;
    }
}

uint8_t w32_video_seek_100ns(uint32_t vid_id, int64_t pos_100ns) {
    w32_video_stream *v = get_video(vid_id);
    if (!v) return 0;
    PROPVARIANT pv;
    PropVariantInit(&pv);
    pv.vt = VT_I8;
    pv.hVal.QuadPart = (LONGLONG)pos_100ns;
    HRESULT hr = IMFSourceReader_SetCurrentPosition(v->reader, &GUID_NULL, &pv);
    PropVariantClear(&pv);
    return SUCCEEDED(hr) ? 1 : 0;
}

void w32_video_close(uint32_t vid_id) {
    w32_video_stream *v = get_video(vid_id);
    if (!v) return;
    if (v->reader) IMFSourceReader_Release(v->reader);
    memset(v, 0, sizeof(*v));
}
