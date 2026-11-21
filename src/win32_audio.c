#define COBJMACROS
#include <stdlib.h>
#include <string.h>

#include "win32_internal.h"
#include <mmsystem.h>
#include <mmreg.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <propsys.h>

/* Canonical COM GUIDs & PKEY_Device_FriendlyName (no INITGUID needed). */
static const CLSID W32_CLSID_MMDeviceEnumerator =
    { 0xBCDE0395, 0xE52F, 0x467C, { 0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E } };
static const IID W32_IID_IMMDeviceEnumerator =
    { 0xA95664D2, 0x9614, 0x4F35, { 0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6 } };
static const IID W32_IID_IAudioClient =
    { 0x1CB9AD4C, 0xDBFA, 0x4C32, { 0xB1, 0x78, 0xC2, 0xF5, 0x68, 0xA7, 0x03, 0xB2 } };
static const IID W32_IID_IAudioRenderClient =
    { 0xF294ACFC, 0x3146, 0x4483, { 0xA7, 0xBF, 0xAD, 0xDC, 0xA7, 0xC2, 0x60, 0xE2 } };
static const IID W32_IID_IAudioCaptureClient =
    { 0xC8ADBD64, 0xE71E, 0x48a0, { 0xA4, 0xDE, 0x18, 0x5C, 0x39, 0x5C, 0xD3, 0x17 } };
static const GUID W32_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT =
    { 0x00000003, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71 } };
static const PROPERTYKEY W32_PKEY_Device_FriendlyName =
    { { 0xa45c254e, 0xdf1c, 0x4efd, { 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0 } }, 14 };

static int g_audio_com_inited = 0;

static int ensure_com(void) {
    if (g_audio_com_inited) return 1;
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE) {
        g_audio_com_inited = 1;
        return 1;
    }
    return 0;
}

static uint8_t read_device_friendly_name(IMMDevice *dev, gs_rref out) {
    if (!dev) return 0;
    IPropertyStore *props = NULL;
    if (FAILED(IMMDevice_OpenPropertyStore(dev, STGM_READ, &props)) || !props) return 0;

    PROPVARIANT pv;
    PropVariantInit(&pv);
    uint8_t ok = 0;
    if (SUCCEEDED(IPropertyStore_GetValue(props, &W32_PKEY_Device_FriendlyName, &pv))) {
        if (pv.vt == VT_LPWSTR && pv.pwszVal) {
            ok = w32_append_wide_as_utf8(pv.pwszVal, -1, out);
        }
        PropVariantClear(&pv);
    }
    IPropertyStore_Release(props);
    return ok;
}

int32_t w32_audio_output_device_count(void) {
    if (!ensure_com()) return 0;
    IMMDeviceEnumerator *en = NULL;
    if (FAILED(CoCreateInstance(&W32_CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, &W32_IID_IMMDeviceEnumerator, (void **)&en)) || !en) {
        return 0;
    }
    IMMDeviceCollection *col = NULL;
    UINT count = 0;
    if (SUCCEEDED(IMMDeviceEnumerator_EnumAudioEndpoints(en, eRender, DEVICE_STATE_ACTIVE, &col)) && col) {
        IMMDeviceCollection_GetCount(col, &count);
        IMMDeviceCollection_Release(col);
    }
    IMMDeviceEnumerator_Release(en);
    return (int32_t)count;
}

uint8_t w32_audio_output_device_name(int32_t index, gs_rref out) {
    if (index < 0 || !ensure_com()) return 0;
    IMMDeviceEnumerator *en = NULL;
    if (FAILED(CoCreateInstance(&W32_CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, &W32_IID_IMMDeviceEnumerator, (void **)&en)) || !en) {
        return 0;
    }
    IMMDeviceCollection *col = NULL;
    uint8_t ok = 0;
    if (SUCCEEDED(IMMDeviceEnumerator_EnumAudioEndpoints(en, eRender, DEVICE_STATE_ACTIVE, &col)) && col) {
        IMMDevice *dev = NULL;
        if (SUCCEEDED(IMMDeviceCollection_Item(col, (UINT)index, &dev)) && dev) {
            ok = read_device_friendly_name(dev, out);
            IMMDevice_Release(dev);
        }
        IMMDeviceCollection_Release(col);
    }
    IMMDeviceEnumerator_Release(en);
    return ok;
}

uint8_t w32_audio_default_output_name(gs_rref out) {
    if (!ensure_com()) return 0;
    IMMDeviceEnumerator *en = NULL;
    if (FAILED(CoCreateInstance(&W32_CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, &W32_IID_IMMDeviceEnumerator, (void **)&en)) || !en) {
        return 0;
    }
    IMMDevice *dev = NULL;
    uint8_t ok = 0;
    if (SUCCEEDED(IMMDeviceEnumerator_GetDefaultAudioEndpoint(en, eRender, eConsole, &dev)) && dev) {
        ok = read_device_friendly_name(dev, out);
        IMMDevice_Release(dev);
    }
    IMMDeviceEnumerator_Release(en);
    return ok;
}

/* ---- WASAPI shared-mode render stream ------------------------------------ */

#define W32_MAX_AUDIO 8

typedef struct {
    int used;
    IMMDevice *dev;
    IAudioClient *client;
    IAudioRenderClient *render;
    uint32_t sample_rate;
    uint16_t channels;
    uint16_t bits_per_sample;
    uint8_t is_float;
    uint32_t buffer_frames;
} w32_wasapi_stream;

static w32_wasapi_stream g_audio[W32_MAX_AUDIO];

static w32_wasapi_stream *get_audio(uint32_t id) {
    if (id == 0 || id > W32_MAX_AUDIO) return NULL;
    return g_audio[id - 1].used ? &g_audio[id - 1] : NULL;
}

uint32_t w32_wasapi_open_default(w32_audio_format *out_fmt) {
    if (!ensure_com()) return 0;
    int slot = -1;
    for (int i = 0; i < W32_MAX_AUDIO; i++) {
        if (!g_audio[i].used) { slot = i; break; }
    }
    if (slot < 0) return 0;

    IMMDeviceEnumerator *en = NULL;
    if (FAILED(CoCreateInstance(&W32_CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, &W32_IID_IMMDeviceEnumerator, (void **)&en)) || !en) {
        return 0;
    }

    IMMDevice *dev = NULL;
    HRESULT hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(en, eRender, eConsole, &dev);
    IMMDeviceEnumerator_Release(en);
    if (FAILED(hr) || !dev) return 0;

    IAudioClient *client = NULL;
    hr = IMMDevice_Activate(dev, &W32_IID_IAudioClient, CLSCTX_ALL, NULL, (void **)&client);
    if (FAILED(hr) || !client) {
        IMMDevice_Release(dev);
        return 0;
    }

    WAVEFORMATEX *wfx = NULL;
    hr = IAudioClient_GetMixFormat(client, &wfx);
    if (FAILED(hr) || !wfx) {
        IAudioClient_Release(client);
        IMMDevice_Release(dev);
        return 0;
    }

    uint8_t is_float = 0;
    if (wfx->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
        is_float = 1;
    } else if (wfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE && wfx->cbSize >= 22) {
        const WAVEFORMATEXTENSIBLE *ext = (const WAVEFORMATEXTENSIBLE *)wfx;
        if (memcmp(&ext->SubFormat, &W32_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, sizeof(GUID)) == 0) {
            is_float = 1;
        }
    }

    /* 100ms shared buffer (1,000,000 * 100ns units). */
    REFERENCE_TIME hns_dur = 1000000;
    hr = IAudioClient_Initialize(client, AUDCLNT_SHAREMODE_SHARED, 0, hns_dur, 0, wfx, NULL);
    uint32_t sample_rate = (uint32_t)wfx->nSamplesPerSec;
    uint16_t channels = (uint16_t)wfx->nChannels;
    uint16_t bits = (uint16_t)wfx->wBitsPerSample;
    CoTaskMemFree(wfx);
    if (FAILED(hr)) {
        IAudioClient_Release(client);
        IMMDevice_Release(dev);
        return 0;
    }

    UINT32 buf_frames = 0;
    hr = IAudioClient_GetBufferSize(client, &buf_frames);
    if (FAILED(hr) || buf_frames == 0) {
        IAudioClient_Release(client);
        IMMDevice_Release(dev);
        return 0;
    }

    IAudioRenderClient *render = NULL;
    hr = IAudioClient_GetService(client, &W32_IID_IAudioRenderClient, (void **)&render);
    if (FAILED(hr) || !render) {
        IAudioClient_Release(client);
        IMMDevice_Release(dev);
        return 0;
    }

    if (FAILED(IAudioClient_Start(client))) {
        IAudioRenderClient_Release(render);
        IAudioClient_Release(client);
        IMMDevice_Release(dev);
        return 0;
    }

    w32_wasapi_stream *s = &g_audio[slot];
    s->used = 1;
    s->dev = dev;
    s->client = client;
    s->render = render;
    s->sample_rate = sample_rate;
    s->channels = channels;
    s->bits_per_sample = bits;
    s->is_float = is_float;
    s->buffer_frames = (uint32_t)buf_frames;

    if (out_fmt) {
        out_fmt->sample_rate = sample_rate;
        out_fmt->channels = channels;
        out_fmt->bits_per_sample = bits;
        out_fmt->is_float = is_float;
        out_fmt->buffer_frames = (uint32_t)buf_frames;
    }
    return (uint32_t)(slot + 1);
}

uint32_t w32_wasapi_available_frames(uint32_t audio_id) {
    w32_wasapi_stream *s = get_audio(audio_id);
    if (!s) return 0;
    UINT32 pad = 0;
    if (FAILED(IAudioClient_GetCurrentPadding(s->client, &pad))) return 0;
    return pad < s->buffer_frames ? (s->buffer_frames - (uint32_t)pad) : 0;
}

uint32_t w32_wasapi_write_f32(uint32_t audio_id, w32_f32_slice samples) {
    w32_wasapi_stream *s = get_audio(audio_id);
    if (!s || !samples.data || samples.len <= 0 || s->channels == 0) return 0;

    uint32_t req_frames = (uint32_t)(samples.len / (int64_t)s->channels);
    uint32_t avail = w32_wasapi_available_frames(audio_id);
    uint32_t frames = req_frames < avail ? req_frames : avail;
    if (frames == 0) return 0;

    BYTE *dst = NULL;
    if (FAILED(IAudioRenderClient_GetBuffer(s->render, (UINT32)frames, &dst)) || !dst) {
        return 0;
    }

    size_t total_samples = (size_t)frames * (size_t)s->channels;
    if (s->is_float && s->bits_per_sample == 32) {
        memcpy(dst, samples.data, total_samples * sizeof(float));
    } else if (s->bits_per_sample == 16) {
        int16_t *out16 = (int16_t *)dst;
        for (size_t i = 0; i < total_samples; i++) {
            float v = samples.data[i];
            if (v > 1.0f) v = 1.0f;
            if (v < -1.0f) v = -1.0f;
            out16[i] = (int16_t)(v * 32767.0f);
        }
    } else {
        memset(dst, 0, (size_t)frames * (size_t)s->channels * (size_t)(s->bits_per_sample / 8));
    }

    IAudioRenderClient_ReleaseBuffer(s->render, (UINT32)frames, 0);
    return frames;
}

void w32_wasapi_start(uint32_t audio_id) {
    w32_wasapi_stream *s = get_audio(audio_id);
    if (s) IAudioClient_Start(s->client);
}

void w32_wasapi_stop(uint32_t audio_id) {
    w32_wasapi_stream *s = get_audio(audio_id);
    if (s) IAudioClient_Stop(s->client);
}

void w32_wasapi_close(uint32_t audio_id) {
    w32_wasapi_stream *s = get_audio(audio_id);
    if (!s) return;
    if (s->client) IAudioClient_Stop(s->client);
    if (s->render) IAudioRenderClient_Release(s->render);
    if (s->client) IAudioClient_Release(s->client);
    if (s->dev) IMMDevice_Release(s->dev);
    memset(s, 0, sizeof(*s));
}

/* ---- winmm PlaySound helpers --------------------------------------------- */

static uint8_t *g_async_wav_buf = NULL;

uint8_t w32_play_sound_file(w32_bytes path, uint8_t async_play) {
    w32_wide_buf wb;
    if (!w32_wide_init(&wb, path) || wb.len == 0) {
        w32_wide_free(&wb);
        return 0;
    }
    PlaySoundW(NULL, NULL, 0);
    free(g_async_wav_buf);
    g_async_wav_buf = NULL;

    DWORD flags = SND_FILENAME | (async_play ? SND_ASYNC : SND_SYNC);
    BOOL ok = PlaySoundW(wb.ptr, NULL, flags);
    w32_wide_free(&wb);
    return ok ? 1 : 0;
}

void w32_play_sound_stop(void) {
    PlaySoundW(NULL, NULL, 0);
    free(g_async_wav_buf);
    g_async_wav_buf = NULL;
}

uint8_t w32_play_wav_bytes(w32_bytes wav_bytes, uint8_t async_play) {
    if (!wav_bytes.data || wav_bytes.len < 44) return 0;
    /* Basic RIFF/WAVE header & size validation so PlaySoundW never reads past the slice. */
    if (memcmp(wav_bytes.data, "RIFF", 4) != 0 || memcmp(wav_bytes.data + 8, "WAVE", 4) != 0) {
        return 0;
    }
    uint32_t riff_size = 0;
    memcpy(&riff_size, wav_bytes.data + 4, sizeof(riff_size));
    if ((uint64_t)riff_size + 8ull > (uint64_t)wav_bytes.len) {
        return 0;
    }

    PlaySoundW(NULL, NULL, 0);
    free(g_async_wav_buf);
    g_async_wav_buf = NULL;

    if (async_play) {
        /* PlaySoundW(SND_MEMORY | SND_ASYNC) keeps reading the buffer until playback finishes. */
        uint8_t *copy = (uint8_t *)malloc((size_t)wav_bytes.len);
        if (!copy) return 0;
        memcpy(copy, wav_bytes.data, (size_t)wav_bytes.len);
        if (!PlaySoundW((LPCWSTR)copy, NULL, SND_MEMORY | SND_ASYNC)) {
            free(copy);
            return 0;
        }
        g_async_wav_buf = copy;
        return 1;
    }

    return PlaySoundW((LPCWSTR)wav_bytes.data, NULL, SND_MEMORY | SND_SYNC) ? 1 : 0;
}

/* ---- WASAPI input enumeration & capture/loopback stream ------------------ */

int32_t w32_audio_input_device_count(void) {
    if (!ensure_com()) return 0;
    IMMDeviceEnumerator *en = NULL;
    if (FAILED(CoCreateInstance(&W32_CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, &W32_IID_IMMDeviceEnumerator, (void **)&en)) || !en) {
        return 0;
    }
    IMMDeviceCollection *col = NULL;
    UINT count = 0;
    if (SUCCEEDED(IMMDeviceEnumerator_EnumAudioEndpoints(en, eCapture, DEVICE_STATE_ACTIVE, &col)) && col) {
        IMMDeviceCollection_GetCount(col, &count);
        IMMDeviceCollection_Release(col);
    }
    IMMDeviceEnumerator_Release(en);
    return (int32_t)count;
}

uint8_t w32_audio_input_device_name(int32_t index, gs_rref out) {
    if (index < 0 || !ensure_com()) return 0;
    IMMDeviceEnumerator *en = NULL;
    if (FAILED(CoCreateInstance(&W32_CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, &W32_IID_IMMDeviceEnumerator, (void **)&en)) || !en) {
        return 0;
    }
    IMMDeviceCollection *col = NULL;
    uint8_t ok = 0;
    if (SUCCEEDED(IMMDeviceEnumerator_EnumAudioEndpoints(en, eCapture, DEVICE_STATE_ACTIVE, &col)) && col) {
        IMMDevice *dev = NULL;
        if (SUCCEEDED(IMMDeviceCollection_Item(col, (UINT)index, &dev)) && dev) {
            ok = read_device_friendly_name(dev, out);
            IMMDevice_Release(dev);
        }
        IMMDeviceCollection_Release(col);
    }
    IMMDeviceEnumerator_Release(en);
    return ok;
}

uint8_t w32_audio_default_input_name(gs_rref out) {
    if (!ensure_com()) return 0;
    IMMDeviceEnumerator *en = NULL;
    if (FAILED(CoCreateInstance(&W32_CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, &W32_IID_IMMDeviceEnumerator, (void **)&en)) || !en) {
        return 0;
    }
    IMMDevice *dev = NULL;
    uint8_t ok = 0;
    if (SUCCEEDED(IMMDeviceEnumerator_GetDefaultAudioEndpoint(en, eCapture, eConsole, &dev)) && dev) {
        ok = read_device_friendly_name(dev, out);
        IMMDevice_Release(dev);
    }
    IMMDeviceEnumerator_Release(en);
    return ok;
}

#define W32_MAX_CAPTURE 8

typedef struct {
    int used;
    IMMDevice *dev;
    IAudioClient *client;
    IAudioCaptureClient *capture;
    uint32_t sample_rate;
    uint16_t channels;
    uint16_t bits_per_sample;
    uint8_t is_float;
    uint32_t buffer_frames;
} w32_wasapi_capture;

static w32_wasapi_capture g_capture[W32_MAX_CAPTURE];

static w32_wasapi_capture *get_capture(uint32_t id) {
    if (id == 0 || id > W32_MAX_CAPTURE) return NULL;
    return g_capture[id - 1].used ? &g_capture[id - 1] : NULL;
}

uint32_t w32_wasapi_capture_open(uint8_t loopback, w32_audio_format *out_fmt) {
    if (!ensure_com()) return 0;
    int slot = -1;
    for (int i = 0; i < W32_MAX_CAPTURE; i++) {
        if (!g_capture[i].used) { slot = i; break; }
    }
    if (slot < 0) return 0;

    IMMDeviceEnumerator *en = NULL;
    if (FAILED(CoCreateInstance(&W32_CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, &W32_IID_IMMDeviceEnumerator, (void **)&en)) || !en) {
        return 0;
    }

    IMMDevice *dev = NULL;
    EDataFlow flow = loopback ? eRender : eCapture;
    HRESULT hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(en, flow, eConsole, &dev);
    IMMDeviceEnumerator_Release(en);
    if (FAILED(hr) || !dev) return 0;

    IAudioClient *client = NULL;
    hr = IMMDevice_Activate(dev, &W32_IID_IAudioClient, CLSCTX_ALL, NULL, (void **)&client);
    if (FAILED(hr) || !client) {
        IMMDevice_Release(dev);
        return 0;
    }

    WAVEFORMATEX *wfx = NULL;
    hr = IAudioClient_GetMixFormat(client, &wfx);
    if (FAILED(hr) || !wfx) {
        IAudioClient_Release(client);
        IMMDevice_Release(dev);
        return 0;
    }

    uint8_t is_float = 0;
    if (wfx->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
        is_float = 1;
    } else if (wfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE && wfx->cbSize >= 22) {
        const WAVEFORMATEXTENSIBLE *ext = (const WAVEFORMATEXTENSIBLE *)wfx;
        if (memcmp(&ext->SubFormat, &W32_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, sizeof(GUID)) == 0) {
            is_float = 1;
        }
    }

    REFERENCE_TIME hns_dur = 1000000;
    DWORD stream_flags = loopback ? AUDCLNT_STREAMFLAGS_LOOPBACK : 0;
    hr = IAudioClient_Initialize(client, AUDCLNT_SHAREMODE_SHARED, stream_flags, hns_dur, 0, wfx, NULL);
    uint32_t sample_rate = (uint32_t)wfx->nSamplesPerSec;
    uint16_t channels = (uint16_t)wfx->nChannels;
    uint16_t bits = (uint16_t)wfx->wBitsPerSample;
    CoTaskMemFree(wfx);
    if (FAILED(hr)) {
        IAudioClient_Release(client);
        IMMDevice_Release(dev);
        return 0;
    }

    UINT32 buf_frames = 0;
    hr = IAudioClient_GetBufferSize(client, &buf_frames);
    if (FAILED(hr) || buf_frames == 0) {
        IAudioClient_Release(client);
        IMMDevice_Release(dev);
        return 0;
    }

    IAudioCaptureClient *capture = NULL;
    hr = IAudioClient_GetService(client, &W32_IID_IAudioCaptureClient, (void **)&capture);
    if (FAILED(hr) || !capture) {
        IAudioClient_Release(client);
        IMMDevice_Release(dev);
        return 0;
    }

    if (FAILED(IAudioClient_Start(client))) {
        IAudioCaptureClient_Release(capture);
        IAudioClient_Release(client);
        IMMDevice_Release(dev);
        return 0;
    }

    w32_wasapi_capture *c = &g_capture[slot];
    c->used = 1;
    c->dev = dev;
    c->client = client;
    c->capture = capture;
    c->sample_rate = sample_rate;
    c->channels = channels;
    c->bits_per_sample = bits;
    c->is_float = is_float;
    c->buffer_frames = (uint32_t)buf_frames;

    if (out_fmt) {
        out_fmt->sample_rate = sample_rate;
        out_fmt->channels = channels;
        out_fmt->bits_per_sample = bits;
        out_fmt->is_float = is_float;
        out_fmt->buffer_frames = (uint32_t)buf_frames;
    }
    return (uint32_t)(slot + 1);
}

uint32_t w32_wasapi_capture_read_f32(uint32_t cap_id, gs_rref out_f32) {
    w32_wasapi_capture *c = get_capture(cap_id);
    if (!c || c->channels == 0) return 0;

    uint32_t total_frames = 0;
    UINT32 pkt = 0;
    while (SUCCEEDED(IAudioCaptureClient_GetNextPacketSize(c->capture, &pkt)) && pkt > 0) {
        BYTE *data = NULL;
        UINT32 frames = 0;
        DWORD flags = 0;
        if (FAILED(IAudioCaptureClient_GetBuffer(c->capture, &data, &frames, &flags, NULL, NULL))) {
            break;
        }
        int64_t nsamples = (int64_t)frames * (int64_t)c->channels;
        int wrote = 0;
        if (nsamples > 0) {
            if ((flags & AUDCLNT_BUFFERFLAGS_SILENT) || !data) {
                float zeros[256];
                memset(zeros, 0, sizeof(zeros));
                int64_t rem = nsamples;
                while (rem > 0) {
                    int64_t chunk = rem > 256 ? 256 : rem;
                    w32_bld_append_f32(out_f32, zeros, chunk);
                    rem -= chunk;
                }
                wrote = 1;
            } else if (c->is_float && c->bits_per_sample == 32) {
                w32_bld_append_f32(out_f32, (const float *)data, nsamples);
                wrote = 1;
            } else if (c->bits_per_sample == 16) {
                const int16_t *in16 = (const int16_t *)data;
                float tmp[256];
                int64_t i = 0;
                while (i < nsamples) {
                    int64_t chunk = (nsamples - i) > 256 ? 256 : (nsamples - i);
                    for (int64_t j = 0; j < chunk; j++) {
                        tmp[j] = (float)in16[i + j] / 32768.0f;
                    }
                    w32_bld_append_f32(out_f32, tmp, chunk);
                    i += chunk;
                }
                wrote = 1;
            } else if (!c->is_float && c->bits_per_sample == 32) {
                const int32_t *in32 = (const int32_t *)data;
                float tmp[256];
                int64_t i = 0;
                while (i < nsamples) {
                    int64_t chunk = (nsamples - i) > 256 ? 256 : (nsamples - i);
                    for (int64_t j = 0; j < chunk; j++) {
                        tmp[j] = (float)((double)in32[i + j] / 2147483648.0);
                    }
                    w32_bld_append_f32(out_f32, tmp, chunk);
                    i += chunk;
                }
                wrote = 1;
            } else if (!c->is_float && c->bits_per_sample == 24) {
                float tmp[256];
                int64_t i = 0;
                while (i < nsamples) {
                    int64_t chunk = (nsamples - i) > 256 ? 256 : (nsamples - i);
                    for (int64_t j = 0; j < chunk; j++) {
                        const uint8_t *p = data + (size_t)(i + j) * 3u;
                        int32_t v = ((int32_t)p[0]) | ((int32_t)p[1] << 8) | ((int32_t)(int8_t)p[2] << 16);
                        tmp[j] = (float)v / 8388608.0f;
                    }
                    w32_bld_append_f32(out_f32, tmp, chunk);
                    i += chunk;
                }
                wrote = 1;
            }
        }
        IAudioCaptureClient_ReleaseBuffer(c->capture, frames);
        if (wrote) {
            total_frames += (uint32_t)frames;
        } else if (nsamples > 0) {
            break;
        }
    }
    return total_frames;
}

void w32_wasapi_capture_start(uint32_t cap_id) {
    w32_wasapi_capture *c = get_capture(cap_id);
    if (c) IAudioClient_Start(c->client);
}

void w32_wasapi_capture_stop(uint32_t cap_id) {
    w32_wasapi_capture *c = get_capture(cap_id);
    if (c) IAudioClient_Stop(c->client);
}

void w32_wasapi_capture_close(uint32_t cap_id) {
    w32_wasapi_capture *c = get_capture(cap_id);
    if (!c) return;
    if (c->client) IAudioClient_Stop(c->client);
    if (c->capture) IAudioCaptureClient_Release(c->capture);
    if (c->client) IAudioClient_Release(c->client);
    if (c->dev) IMMDevice_Release(c->dev);
    memset(c, 0, sizeof(*c));
}
