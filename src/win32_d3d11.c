#define COBJMACROS
#include <stdlib.h>
#include <string.h>

#include "win32_internal.h"
#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h>

/* ---- Internal slot tables ------------------------------------------------ */

#define W32_MAX_D3D_DEVS   8
#define W32_MAX_D3D_PIPES  64
#define W32_MAX_D3D_BUFS   128

typedef struct {
    int used;
    HWND hwnd;
    uint8_t vsync;
    int32_t width;
    int32_t height;
    ID3D11Device *device;
    ID3D11DeviceContext *ctx;
    IDXGISwapChain *swapchain;
    ID3D11RenderTargetView *rtv;
    ID3D11Texture2D *depth_tex;
    ID3D11DepthStencilView *dsv;
} w32_d3d_dev;

typedef struct {
    int used;
    ID3D11VertexShader *vs;
    ID3D11PixelShader *ps;
    ID3D11InputLayout *layout;
    ID3D11RasterizerState *rast;
    ID3D11DepthStencilState *ds;
    ID3D11BlendState *blend;
} w32_d3d_pipe;

typedef struct {
    int used;
    ID3D11Buffer *buf;
    uint32_t byte_size;
    uint8_t dynamic;
} w32_d3d_buf;

static w32_d3d_dev g_devs[W32_MAX_D3D_DEVS];
static w32_d3d_pipe g_pipes[W32_MAX_D3D_PIPES];
static w32_d3d_buf g_bufs[W32_MAX_D3D_BUFS];

static w32_d3d_dev *get_dev(uint32_t id) {
    if (id == 0 || id > W32_MAX_D3D_DEVS) return NULL;
    w32_d3d_dev *d = &g_devs[id - 1];
    return d->used ? d : NULL;
}

static w32_d3d_pipe *get_pipe(uint32_t id) {
    if (id == 0 || id > W32_MAX_D3D_PIPES) return NULL;
    w32_d3d_pipe *p = &g_pipes[id - 1];
    return p->used ? p : NULL;
}

static w32_d3d_buf *get_buf(uint32_t id) {
    if (id == 0 || id > W32_MAX_D3D_BUFS) return NULL;
    w32_d3d_buf *b = &g_bufs[id - 1];
    return b->used ? b : NULL;
}

/* ---- Render target + depth buffer creation ------------------------------- */

static void release_targets(w32_d3d_dev *d) {
    if (d->ctx) {
        ID3D11DeviceContext_OMSetRenderTargets(d->ctx, 0, NULL, NULL);
    }
    if (d->rtv) { ID3D11RenderTargetView_Release(d->rtv); d->rtv = NULL; }
    if (d->dsv) { ID3D11DepthStencilView_Release(d->dsv); d->dsv = NULL; }
    if (d->depth_tex) { ID3D11Texture2D_Release(d->depth_tex); d->depth_tex = NULL; }
}

static int create_targets(w32_d3d_dev *d, int32_t w, int32_t h) {
    if (w <= 0) w = 1;
    if (h <= 0) h = 1;

    ID3D11Texture2D *backbuf = NULL;
    HRESULT hr = IDXGISwapChain_GetBuffer(d->swapchain, 0, &IID_ID3D11Texture2D, (void **)&backbuf);
    if (FAILED(hr) || !backbuf) return 0;

    hr = ID3D11Device_CreateRenderTargetView(d->device, (ID3D11Resource *)backbuf, NULL, &d->rtv);
    ID3D11Texture2D_Release(backbuf);
    if (FAILED(hr) || !d->rtv) return 0;

    D3D11_TEXTURE2D_DESC dd;
    memset(&dd, 0, sizeof(dd));
    dd.Width = (UINT)w;
    dd.Height = (UINT)h;
    dd.MipLevels = 1;
    dd.ArraySize = 1;
    dd.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dd.SampleDesc.Count = 1;
    dd.Usage = D3D11_USAGE_DEFAULT;
    dd.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    hr = ID3D11Device_CreateTexture2D(d->device, &dd, NULL, &d->depth_tex);
    if (FAILED(hr) || !d->depth_tex) {
        release_targets(d);
        return 0;
    }

    hr = ID3D11Device_CreateDepthStencilView(d->device, (ID3D11Resource *)d->depth_tex, NULL, &d->dsv);
    if (FAILED(hr) || !d->dsv) {
        release_targets(d);
        return 0;
    }

    d->width = w;
    d->height = h;
    return 1;
}

/* ---- Device lifecycle ---------------------------------------------------- */

uint32_t w32_d3d11_create(uint64_t hwnd_u64, uint8_t vsync) {
    HWND hwnd = (HWND)(uintptr_t)hwnd_u64;
    if (!hwnd) return 0;

    int slot = -1;
    for (int i = 0; i < W32_MAX_D3D_DEVS; i++) {
        if (!g_devs[i].used) { slot = i; break; }
    }
    if (slot < 0) return 0;

    RECT rc;
    GetClientRect(hwnd, &rc);
    int32_t w = (int32_t)(rc.right - rc.left);
    int32_t h = (int32_t)(rc.bottom - rc.top);
    if (w <= 0) w = 1;
    if (h <= 0) h = 1;

    DXGI_SWAP_CHAIN_DESC scd;
    memset(&scd, 0, sizeof(scd));
    scd.BufferCount = 2;
    scd.BufferDesc.Width = (UINT)w;
    scd.BufferDesc.Height = (UINT)h;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = hwnd;
    scd.SampleDesc.Count = 1;
    scd.Windowed = TRUE;
    scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };
    D3D_FEATURE_LEVEL chosen = D3D_FEATURE_LEVEL_11_0;

    w32_d3d_dev *d = &g_devs[slot];
    memset(d, 0, sizeof(*d));

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0,
        levels, 3, D3D11_SDK_VERSION,
        &scd, &d->swapchain, &d->device, &chosen, &d->ctx
    );
    if (FAILED(hr)) {
        /* Fallback to WARP software rasterizer for headless / VM environments. */
        hr = D3D11CreateDeviceAndSwapChain(
            NULL, D3D_DRIVER_TYPE_WARP, NULL, 0,
            levels, 3, D3D11_SDK_VERSION,
            &scd, &d->swapchain, &d->device, &chosen, &d->ctx
        );
    }
    if (FAILED(hr) || !d->device || !d->ctx || !d->swapchain) {
        memset(d, 0, sizeof(*d));
        return 0;
    }

    d->hwnd = hwnd;
    d->vsync = vsync ? 1 : 0;
    if (!create_targets(d, w, h)) {
        IDXGISwapChain_Release(d->swapchain);
        ID3D11DeviceContext_Release(d->ctx);
        ID3D11Device_Release(d->device);
        memset(d, 0, sizeof(*d));
        return 0;
    }

    d->used = 1;
    return (uint32_t)(slot + 1);
}

void w32_d3d11_destroy(uint32_t dev_id) {
    w32_d3d_dev *d = get_dev(dev_id);
    if (!d) return;
    release_targets(d);
    if (d->swapchain) IDXGISwapChain_Release(d->swapchain);
    if (d->ctx) ID3D11DeviceContext_Release(d->ctx);
    if (d->device) ID3D11Device_Release(d->device);
    memset(d, 0, sizeof(*d));
}

uint8_t w32_d3d11_resize(uint32_t dev_id, int32_t width, int32_t height) {
    w32_d3d_dev *d = get_dev(dev_id);
    if (!d || width <= 0 || height <= 0) return 0;
    if (d->rtv && d->width == width && d->height == height) return 1;

    release_targets(d);
    HRESULT hr = IDXGISwapChain_ResizeBuffers(
        d->swapchain, 0, (UINT)width, (UINT)height, DXGI_FORMAT_UNKNOWN, 0
    );
    if (FAILED(hr)) return 0;
    return create_targets(d, width, height) ? 1 : 0;
}

uint8_t w32_d3d11_adapter_name(uint32_t dev_id, gs_rref out) {
    w32_d3d_dev *d = get_dev(dev_id);
    if (!d) return 0;

    IDXGIDevice *dxgi_dev = NULL;
    if (FAILED(ID3D11Device_QueryInterface(d->device, &IID_IDXGIDevice, (void **)&dxgi_dev)) || !dxgi_dev) {
        return 0;
    }
    IDXGIAdapter *adapter = NULL;
    uint8_t ok = 0;
    if (SUCCEEDED(IDXGIDevice_GetAdapter(dxgi_dev, &adapter)) && adapter) {
        DXGI_ADAPTER_DESC desc;
        if (SUCCEEDED(IDXGIAdapter_GetDesc(adapter, &desc))) {
            ok = w32_append_wide_as_utf8(desc.Description, -1, out);
        }
        IDXGIAdapter_Release(adapter);
    }
    IDXGIDevice_Release(dxgi_dev);
    return ok;
}

/* ---- Frame begin / end --------------------------------------------------- */

void w32_d3d11_begin_frame(uint32_t dev_id, float r, float g, float b, float a) {
    w32_d3d_dev *d = get_dev(dev_id);
    if (!d || !d->rtv) return;

    float clear[4] = { r, g, b, a };
    ID3D11DeviceContext_OMSetRenderTargets(d->ctx, 1, &d->rtv, d->dsv);
    ID3D11DeviceContext_ClearRenderTargetView(d->ctx, d->rtv, clear);
    if (d->dsv) {
        ID3D11DeviceContext_ClearDepthStencilView(d->ctx, d->dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    }

    D3D11_VIEWPORT vp;
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    vp.Width = (float)d->width;
    vp.Height = (float)d->height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    ID3D11DeviceContext_RSSetViewports(d->ctx, 1, &vp);
    ID3D11DeviceContext_IASetPrimitiveTopology(d->ctx, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

uint8_t w32_d3d11_end_frame(uint32_t dev_id) {
    w32_d3d_dev *d = get_dev(dev_id);
    if (!d) return 0;
    HRESULT hr = IDXGISwapChain_Present(d->swapchain, d->vsync ? 1 : 0, 0);
    return SUCCEEDED(hr) ? 1 : 0;
}

/* ---- HLSL Pipeline ------------------------------------------------------- */

static char *w32_cstr_dup(w32_bytes s) {
    size_t n = s.len > 0 ? (size_t)s.len : 0;
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    if (n > 0 && s.data) memcpy(p, s.data, n);
    p[n] = '\0';
    return p;
}

uint32_t w32_d3d11_create_pipeline(
    uint32_t dev_id,
    w32_bytes hlsl,
    w32_bytes vs_entry,
    w32_bytes ps_entry,
    uint8_t depth_enable,
    gs_rref out_error
) {
    w32_d3d_dev *d = get_dev(dev_id);
    if (!d || !hlsl.data || hlsl.len <= 0) return 0;

    int slot = -1;
    for (int i = 0; i < W32_MAX_D3D_PIPES; i++) {
        if (!g_pipes[i].used) { slot = i; break; }
    }
    if (slot < 0) return 0;

    char *vs_name = w32_cstr_dup(vs_entry);
    char *ps_name = w32_cstr_dup(ps_entry);
    if (!vs_name || !ps_name) {
        free(vs_name);
        free(ps_name);
        return 0;
    }

    ID3DBlob *vs_blob = NULL, *ps_blob = NULL, *err_blob = NULL;
    HRESULT hr = D3DCompile(
        hlsl.data, (SIZE_T)hlsl.len, "win32_goose.hlsl",
        NULL, NULL, vs_name, "vs_4_0", 0, 0, &vs_blob, &err_blob
    );
    if (FAILED(hr) || !vs_blob) {
        if (err_blob) {
            gs_bld_append(out_error, ID3D10Blob_GetBufferPointer(err_blob), (int64_t)ID3D10Blob_GetBufferSize(err_blob));
            ID3D10Blob_Release(err_blob);
        }
        free(vs_name);
        free(ps_name);
        return 0;
    }
    if (err_blob) { ID3D10Blob_Release(err_blob); err_blob = NULL; }

    hr = D3DCompile(
        hlsl.data, (SIZE_T)hlsl.len, "win32_goose.hlsl",
        NULL, NULL, ps_name, "ps_4_0", 0, 0, &ps_blob, &err_blob
    );
    free(vs_name);
    free(ps_name);
    if (FAILED(hr) || !ps_blob) {
        if (err_blob) {
            gs_bld_append(out_error, ID3D10Blob_GetBufferPointer(err_blob), (int64_t)ID3D10Blob_GetBufferSize(err_blob));
            ID3D10Blob_Release(err_blob);
        }
        ID3D10Blob_Release(vs_blob);
        return 0;
    }
    if (err_blob) { ID3D10Blob_Release(err_blob); err_blob = NULL; }

    w32_d3d_pipe *p = &g_pipes[slot];
    memset(p, 0, sizeof(*p));

    hr = ID3D11Device_CreateVertexShader(
        d->device, ID3D10Blob_GetBufferPointer(vs_blob), ID3D10Blob_GetBufferSize(vs_blob), NULL, &p->vs
    );
    if (SUCCEEDED(hr)) {
        hr = ID3D11Device_CreatePixelShader(
            d->device, ID3D10Blob_GetBufferPointer(ps_blob), ID3D10Blob_GetBufferSize(ps_blob), NULL, &p->ps
        );
    }

    D3D11_INPUT_ELEMENT_DESC ied[2] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    if (SUCCEEDED(hr)) {
        hr = ID3D11Device_CreateInputLayout(
            d->device, ied, 2, ID3D10Blob_GetBufferPointer(vs_blob), ID3D10Blob_GetBufferSize(vs_blob), &p->layout
        );
    }
    ID3D10Blob_Release(vs_blob);
    ID3D10Blob_Release(ps_blob);

    if (SUCCEEDED(hr)) {
        D3D11_RASTERIZER_DESC rd;
        memset(&rd, 0, sizeof(rd));
        rd.FillMode = D3D11_FILL_SOLID;
        rd.CullMode = D3D11_CULL_NONE;
        rd.DepthClipEnable = TRUE;
        hr = ID3D11Device_CreateRasterizerState(d->device, &rd, &p->rast);
    }
    if (SUCCEEDED(hr)) {
        D3D11_DEPTH_STENCIL_DESC dsd;
        memset(&dsd, 0, sizeof(dsd));
        dsd.DepthEnable = depth_enable ? TRUE : FALSE;
        dsd.DepthWriteMask = depth_enable ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
        dsd.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
        hr = ID3D11Device_CreateDepthStencilState(d->device, &dsd, &p->ds);
    }
    if (SUCCEEDED(hr)) {
        D3D11_BLEND_DESC bd;
        memset(&bd, 0, sizeof(bd));
        bd.RenderTarget[0].BlendEnable = TRUE;
        bd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        hr = ID3D11Device_CreateBlendState(d->device, &bd, &p->blend);
    }

    if (FAILED(hr)) {
        if (p->blend) ID3D11BlendState_Release(p->blend);
        if (p->ds) ID3D11DepthStencilState_Release(p->ds);
        if (p->rast) ID3D11RasterizerState_Release(p->rast);
        if (p->layout) ID3D11InputLayout_Release(p->layout);
        if (p->ps) ID3D11PixelShader_Release(p->ps);
        if (p->vs) ID3D11VertexShader_Release(p->vs);
        memset(p, 0, sizeof(*p));
        return 0;
    }

    p->used = 1;
    return (uint32_t)(slot + 1);
}

void w32_d3d11_destroy_pipeline(uint32_t pipe_id) {
    w32_d3d_pipe *p = get_pipe(pipe_id);
    if (!p) return;
    if (p->blend) ID3D11BlendState_Release(p->blend);
    if (p->ds) ID3D11DepthStencilState_Release(p->ds);
    if (p->rast) ID3D11RasterizerState_Release(p->rast);
    if (p->layout) ID3D11InputLayout_Release(p->layout);
    if (p->ps) ID3D11PixelShader_Release(p->ps);
    if (p->vs) ID3D11VertexShader_Release(p->vs);
    memset(p, 0, sizeof(*p));
}

/* ---- Buffers ------------------------------------------------------------- */

static uint32_t alloc_buf_slot(void) {
    for (int i = 0; i < W32_MAX_D3D_BUFS; i++) {
        if (!g_bufs[i].used) return (uint32_t)(i + 1);
    }
    return 0;
}

uint32_t w32_d3d11_create_vertex_buffer(uint32_t dev_id, w32_d3d_vertex_slice verts, uint8_t dynamic) {
    w32_d3d_dev *d = get_dev(dev_id);
    if (!d || !verts.data || verts.len <= 0) return 0;
    if ((uint64_t)verts.len > UINT32_MAX / sizeof(w32_d3d_vertex)) return 0;

    uint32_t id = alloc_buf_slot();
    if (!id) return 0;

    uint32_t bytes = (uint32_t)((size_t)verts.len * sizeof(w32_d3d_vertex));
    D3D11_BUFFER_DESC bd;
    memset(&bd, 0, sizeof(bd));
    bd.ByteWidth = bytes;
    bd.Usage = dynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = dynamic ? D3D11_CPU_ACCESS_WRITE : 0;

    D3D11_SUBRESOURCE_DATA init;
    memset(&init, 0, sizeof(init));
    init.pSysMem = verts.data;

    w32_d3d_buf *b = &g_bufs[id - 1];
    memset(b, 0, sizeof(*b));
    if (FAILED(ID3D11Device_CreateBuffer(d->device, &bd, &init, &b->buf)) || !b->buf) {
        return 0;
    }
    b->byte_size = bytes;
    b->dynamic = dynamic ? 1 : 0;
    b->used = 1;
    return id;
}

uint32_t w32_d3d11_create_constant_buffer(uint32_t dev_id, uint32_t byte_size) {
    w32_d3d_dev *d = get_dev(dev_id);
    if (!d || byte_size == 0 || byte_size > 0xFFFFFFF0u) return 0;

    uint32_t id = alloc_buf_slot();
    if (!id) return 0;

    /* D3D11 constant buffer size must be a multiple of 16 bytes. */
    uint32_t aligned = (byte_size + 15u) & ~15u;
    D3D11_BUFFER_DESC bd;
    memset(&bd, 0, sizeof(bd));
    bd.ByteWidth = aligned;
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    w32_d3d_buf *b = &g_bufs[id - 1];
    memset(b, 0, sizeof(*b));
    if (FAILED(ID3D11Device_CreateBuffer(d->device, &bd, NULL, &b->buf)) || !b->buf) {
        return 0;
    }
    b->byte_size = aligned;
    b->dynamic = 1;
    b->used = 1;
    return id;
}

static uint8_t update_buf_raw(w32_d3d_dev *d, w32_d3d_buf *b, const void *src, size_t n) {
    if (!d || !b || !src || n == 0 || n > b->byte_size) return 0;
    if (b->dynamic) {
        D3D11_MAPPED_SUBRESOURCE mapped;
        if (FAILED(ID3D11DeviceContext_Map(d->ctx, (ID3D11Resource *)b->buf, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
            return 0;
        }
        memcpy(mapped.pData, src, n);
        ID3D11DeviceContext_Unmap(d->ctx, (ID3D11Resource *)b->buf, 0);
        return 1;
    }
    if (n < b->byte_size) {
        D3D11_BOX box = { 0, 0, 0, (UINT)n, 1, 1 };
        ID3D11DeviceContext_UpdateSubresource(d->ctx, (ID3D11Resource *)b->buf, 0, &box, src, 0, 0);
    } else {
        ID3D11DeviceContext_UpdateSubresource(d->ctx, (ID3D11Resource *)b->buf, 0, NULL, src, 0, 0);
    }
    return 1;
}

uint8_t w32_d3d11_update_buffer(uint32_t dev_id, uint32_t buf_id, w32_bytes data) {
    return update_buf_raw(get_dev(dev_id), get_buf(buf_id), data.data, data.len > 0 ? (size_t)data.len : 0);
}

uint8_t w32_d3d11_update_vertex_buffer(uint32_t dev_id, uint32_t buf_id, w32_d3d_vertex_slice verts) {
    size_t n = verts.len > 0 ? (size_t)verts.len * sizeof(w32_d3d_vertex) : 0;
    return update_buf_raw(get_dev(dev_id), get_buf(buf_id), verts.data, n);
}

void w32_d3d11_destroy_buffer(uint32_t buf_id) {
    w32_d3d_buf *b = get_buf(buf_id);
    if (!b) return;
    if (b->buf) ID3D11Buffer_Release(b->buf);
    memset(b, 0, sizeof(*b));
}

/* ---- Binding & drawing --------------------------------------------------- */

void w32_d3d11_bind_pipeline(uint32_t dev_id, uint32_t pipe_id) {
    w32_d3d_dev *d = get_dev(dev_id);
    w32_d3d_pipe *p = get_pipe(pipe_id);
    if (!d || !p) return;
    float blend_factor[4] = { 0, 0, 0, 0 };
    ID3D11DeviceContext_IASetInputLayout(d->ctx, p->layout);
    ID3D11DeviceContext_VSSetShader(d->ctx, p->vs, NULL, 0);
    ID3D11DeviceContext_PSSetShader(d->ctx, p->ps, NULL, 0);
    ID3D11DeviceContext_RSSetState(d->ctx, p->rast);
    ID3D11DeviceContext_OMSetDepthStencilState(d->ctx, p->ds, 0);
    ID3D11DeviceContext_OMSetBlendState(d->ctx, p->blend, blend_factor, 0xFFFFFFFF);
}

void w32_d3d11_bind_vertex_buffer(uint32_t dev_id, uint32_t buf_id, uint32_t stride) {
    w32_d3d_dev *d = get_dev(dev_id);
    w32_d3d_buf *b = get_buf(buf_id);
    if (!d || !b) return;
    UINT s = stride ? (UINT)stride : (UINT)sizeof(w32_d3d_vertex);
    UINT offset = 0;
    ID3D11DeviceContext_IASetVertexBuffers(d->ctx, 0, 1, &b->buf, &s, &offset);
}

void w32_d3d11_bind_constant_buffer(uint32_t dev_id, uint32_t slot, uint32_t buf_id) {
    w32_d3d_dev *d = get_dev(dev_id);
    w32_d3d_buf *b = get_buf(buf_id);
    if (!d || !b) return;
    ID3D11DeviceContext_VSSetConstantBuffers(d->ctx, (UINT)slot, 1, &b->buf);
    ID3D11DeviceContext_PSSetConstantBuffers(d->ctx, (UINT)slot, 1, &b->buf);
}

void w32_d3d11_draw(uint32_t dev_id, uint32_t vertex_count, uint32_t start_vertex) {
    w32_d3d_dev *d = get_dev(dev_id);
    if (!d) return;
    ID3D11DeviceContext_Draw(d->ctx, (UINT)vertex_count, (UINT)start_vertex);
}

/* Reads back one RGBA8 pixel (packed 0xAABBGGRR) from the backbuffer via a 1x1 staging texture. */
uint8_t w32_d3d11_readback_pixel(uint32_t dev_id, int32_t x, int32_t y, uint32_t *out_rgba) {
    w32_d3d_dev *d = get_dev(dev_id);
    if (!d || !out_rgba || x < 0 || y < 0 || x >= d->width || y >= d->height) return 0;

    ID3D11Texture2D *backbuf = NULL;
    if (FAILED(IDXGISwapChain_GetBuffer(d->swapchain, 0, &IID_ID3D11Texture2D, (void **)&backbuf)) || !backbuf) {
        return 0;
    }

    D3D11_TEXTURE2D_DESC sd;
    memset(&sd, 0, sizeof(sd));
    sd.Width = 1;
    sd.Height = 1;
    sd.MipLevels = 1;
    sd.ArraySize = 1;
    sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.SampleDesc.Count = 1;
    sd.Usage = D3D11_USAGE_STAGING;
    sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    ID3D11Texture2D *staging = NULL;
    uint8_t ok = 0;
    if (SUCCEEDED(ID3D11Device_CreateTexture2D(d->device, &sd, NULL, &staging)) && staging) {
        D3D11_BOX box = { (UINT)x, (UINT)y, 0, (UINT)(x + 1), (UINT)(y + 1), 1 };
        ID3D11DeviceContext_CopySubresourceRegion(
            d->ctx, (ID3D11Resource *)staging, 0, 0, 0, 0, (ID3D11Resource *)backbuf, 0, &box
        );
        D3D11_MAPPED_SUBRESOURCE mapped;
        if (SUCCEEDED(ID3D11DeviceContext_Map(d->ctx, (ID3D11Resource *)staging, 0, D3D11_MAP_READ, 0, &mapped))) {
            uint32_t px = 0;
            memcpy(&px, mapped.pData, 4);
            *out_rgba = px;
            ID3D11DeviceContext_Unmap(d->ctx, (ID3D11Resource *)staging, 0);
            ok = 1;
        }
        ID3D11Texture2D_Release(staging);
    }

    ID3D11Texture2D_Release(backbuf);
    return ok;
}

/* ---- D3D11 Textured Pipelines, Index Buffers, and Texture2D -------------- */

#define W32_MAX_D3D_TEXTURES 64

typedef struct {
    int used;
    ID3D11Texture2D *tex;
    ID3D11ShaderResourceView *srv;
    ID3D11SamplerState *sampler;
    int32_t width;
    int32_t height;
} w32_d3d_tex;

static w32_d3d_tex g_d3d_texs[W32_MAX_D3D_TEXTURES];

static w32_d3d_tex *get_tex(uint32_t id) {
    if (id == 0 || id > W32_MAX_D3D_TEXTURES) return NULL;
    return g_d3d_texs[id - 1].used ? &g_d3d_texs[id - 1] : NULL;
}

uint32_t w32_d3d11_create_pipeline_textured(
    uint32_t dev_id,
    w32_bytes hlsl,
    w32_bytes vs_entry,
    w32_bytes ps_entry,
    uint8_t depth_enable,
    gs_rref out_error
) {
    w32_d3d_dev *d = get_dev(dev_id);
    if (!d || !hlsl.data || hlsl.len <= 0) return 0;

    int slot = -1;
    for (int i = 0; i < W32_MAX_D3D_PIPES; i++) {
        if (!g_pipes[i].used) { slot = i; break; }
    }
    if (slot < 0) return 0;

    char *vs_name = w32_cstr_dup(vs_entry);
    char *ps_name = w32_cstr_dup(ps_entry);
    if (!vs_name || !ps_name) {
        free(vs_name);
        free(ps_name);
        return 0;
    }

    ID3DBlob *vs_blob = NULL, *ps_blob = NULL, *err_blob = NULL;
    HRESULT hr = D3DCompile(hlsl.data, (SIZE_T)hlsl.len, "win32_goose_tex.hlsl", NULL, NULL, vs_name, "vs_4_0", 0, 0, &vs_blob, &err_blob);
    if (FAILED(hr) || !vs_blob) {
        if (err_blob) {
            gs_bld_append(out_error, ID3D10Blob_GetBufferPointer(err_blob), (int64_t)ID3D10Blob_GetBufferSize(err_blob));
            ID3D10Blob_Release(err_blob);
        }
        free(vs_name);
        free(ps_name);
        return 0;
    }
    if (err_blob) { ID3D10Blob_Release(err_blob); err_blob = NULL; }

    hr = D3DCompile(hlsl.data, (SIZE_T)hlsl.len, "win32_goose_tex.hlsl", NULL, NULL, ps_name, "ps_4_0", 0, 0, &ps_blob, &err_blob);
    free(vs_name);
    free(ps_name);
    if (FAILED(hr) || !ps_blob) {
        if (err_blob) {
            gs_bld_append(out_error, ID3D10Blob_GetBufferPointer(err_blob), (int64_t)ID3D10Blob_GetBufferSize(err_blob));
            ID3D10Blob_Release(err_blob);
        }
        ID3D10Blob_Release(vs_blob);
        return 0;
    }
    if (err_blob) { ID3D10Blob_Release(err_blob); err_blob = NULL; }

    w32_d3d_pipe *p = &g_pipes[slot];
    memset(p, 0, sizeof(*p));

    hr = ID3D11Device_CreateVertexShader(d->device, ID3D10Blob_GetBufferPointer(vs_blob), ID3D10Blob_GetBufferSize(vs_blob), NULL, &p->vs);
    if (SUCCEEDED(hr)) {
        hr = ID3D11Device_CreatePixelShader(d->device, ID3D10Blob_GetBufferPointer(ps_blob), ID3D10Blob_GetBufferSize(ps_blob), NULL, &p->ps);
    }
    D3D11_INPUT_ELEMENT_DESC ied[3] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    if (SUCCEEDED(hr)) {
        hr = ID3D11Device_CreateInputLayout(d->device, ied, 3, ID3D10Blob_GetBufferPointer(vs_blob), ID3D10Blob_GetBufferSize(vs_blob), &p->layout);
    }
    ID3D10Blob_Release(vs_blob);
    ID3D10Blob_Release(ps_blob);

    if (SUCCEEDED(hr)) {
        D3D11_RASTERIZER_DESC rd;
        memset(&rd, 0, sizeof(rd));
        rd.FillMode = D3D11_FILL_SOLID;
        rd.CullMode = D3D11_CULL_NONE;
        rd.DepthClipEnable = TRUE;
        hr = ID3D11Device_CreateRasterizerState(d->device, &rd, &p->rast);
    }
    if (SUCCEEDED(hr)) {
        D3D11_DEPTH_STENCIL_DESC dsd;
        memset(&dsd, 0, sizeof(dsd));
        dsd.DepthEnable = depth_enable ? TRUE : FALSE;
        dsd.DepthWriteMask = depth_enable ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
        dsd.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
        hr = ID3D11Device_CreateDepthStencilState(d->device, &dsd, &p->ds);
    }
    if (SUCCEEDED(hr)) {
        D3D11_BLEND_DESC bd;
        memset(&bd, 0, sizeof(bd));
        bd.RenderTarget[0].BlendEnable = TRUE;
        bd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        hr = ID3D11Device_CreateBlendState(d->device, &bd, &p->blend);
    }

    if (FAILED(hr)) {
        if (p->blend) ID3D11BlendState_Release(p->blend);
        if (p->ds) ID3D11DepthStencilState_Release(p->ds);
        if (p->rast) ID3D11RasterizerState_Release(p->rast);
        if (p->layout) ID3D11InputLayout_Release(p->layout);
        if (p->ps) ID3D11PixelShader_Release(p->ps);
        if (p->vs) ID3D11VertexShader_Release(p->vs);
        memset(p, 0, sizeof(*p));
        return 0;
    }
    p->used = 1;
    return (uint32_t)(slot + 1);
}

uint32_t w32_d3d11_create_tex_vertex_buffer(uint32_t dev_id, w32_tex_vertex_slice verts, uint8_t dynamic) {
    w32_d3d_dev *d = get_dev(dev_id);
    if (!d || !verts.data || verts.len <= 0) return 0;
    if ((uint64_t)verts.len > UINT32_MAX / sizeof(w32_tex_vertex)) return 0;
    uint32_t id = alloc_buf_slot();
    if (!id) return 0;

    uint32_t bytes = (uint32_t)((size_t)verts.len * sizeof(w32_tex_vertex));
    D3D11_BUFFER_DESC bd;
    memset(&bd, 0, sizeof(bd));
    bd.ByteWidth = bytes;
    bd.Usage = dynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = dynamic ? D3D11_CPU_ACCESS_WRITE : 0;

    D3D11_SUBRESOURCE_DATA init = { verts.data, 0, 0 };
    w32_d3d_buf *b = &g_bufs[id - 1];
    memset(b, 0, sizeof(*b));
    if (FAILED(ID3D11Device_CreateBuffer(d->device, &bd, &init, &b->buf)) || !b->buf) return 0;
    b->byte_size = bytes;
    b->dynamic = dynamic ? 1 : 0;
    b->used = 1;
    return id;
}

uint32_t w32_d3d11_create_index_buffer(uint32_t dev_id, w32_u32_slice indices) {
    w32_d3d_dev *d = get_dev(dev_id);
    if (!d || !indices.data || indices.len <= 0) return 0;
    if ((uint64_t)indices.len > UINT32_MAX / sizeof(uint32_t)) return 0;
    uint32_t id = alloc_buf_slot();
    if (!id) return 0;

    uint32_t bytes = (uint32_t)((size_t)indices.len * sizeof(uint32_t));
    D3D11_BUFFER_DESC bd;
    memset(&bd, 0, sizeof(bd));
    bd.ByteWidth = bytes;
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA init = { indices.data, 0, 0 };
    w32_d3d_buf *b = &g_bufs[id - 1];
    memset(b, 0, sizeof(*b));
    if (FAILED(ID3D11Device_CreateBuffer(d->device, &bd, &init, &b->buf)) || !b->buf) return 0;
    b->byte_size = bytes;
    b->dynamic = 0;
    b->used = 1;
    return id;
}

void w32_d3d11_bind_index_buffer(uint32_t dev_id, uint32_t buf_id) {
    w32_d3d_dev *d = get_dev(dev_id);
    w32_d3d_buf *b = get_buf(buf_id);
    if (!d || !b) return;
    ID3D11DeviceContext_IASetIndexBuffer(d->ctx, b->buf, DXGI_FORMAT_R32_UINT, 0);
}

void w32_d3d11_draw_indexed(uint32_t dev_id, uint32_t index_count, uint32_t start_index, int32_t base_vertex) {
    w32_d3d_dev *d = get_dev(dev_id);
    if (!d) return;
    ID3D11DeviceContext_DrawIndexed(d->ctx, (UINT)index_count, (UINT)start_index, (INT)base_vertex);
}

uint32_t w32_d3d11_create_texture_rgba(uint32_t dev_id, int32_t w, int32_t h, w32_u32_slice pixels_rgba) {
    w32_d3d_dev *d = get_dev(dev_id);
    if (!d || w <= 0 || h <= 0 || !pixels_rgba.data || pixels_rgba.len < (int64_t)w * (int64_t)h) return 0;

    int slot = -1;
    for (int i = 0; i < W32_MAX_D3D_TEXTURES; i++) {
        if (!g_d3d_texs[i].used) { slot = i; break; }
    }
    if (slot < 0) return 0;

    D3D11_TEXTURE2D_DESC td;
    memset(&td, 0, sizeof(td));
    td.Width = (UINT)w;
    td.Height = (UINT)h;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA init = { pixels_rgba.data, (UINT)(w * 4), 0 };
    w32_d3d_tex *t = &g_d3d_texs[slot];
    memset(t, 0, sizeof(*t));

    if (FAILED(ID3D11Device_CreateTexture2D(d->device, &td, &init, &t->tex)) || !t->tex) return 0;
    if (FAILED(ID3D11Device_CreateShaderResourceView(d->device, (ID3D11Resource *)t->tex, NULL, &t->srv)) || !t->srv) {
        ID3D11Texture2D_Release(t->tex);
        memset(t, 0, sizeof(*t));
        return 0;
    }

    D3D11_SAMPLER_DESC sd;
    memset(&sd, 0, sizeof(sd));
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    if (FAILED(ID3D11Device_CreateSamplerState(d->device, &sd, &t->sampler)) || !t->sampler) {
        ID3D11ShaderResourceView_Release(t->srv);
        ID3D11Texture2D_Release(t->tex);
        memset(t, 0, sizeof(*t));
        return 0;
    }

    t->width = w;
    t->height = h;
    t->used = 1;
    return (uint32_t)(slot + 1);
}

uint8_t w32_d3d11_update_texture_rgba(uint32_t dev_id, uint32_t tex_id, w32_u32_slice pixels_rgba) {
    w32_d3d_dev *d = get_dev(dev_id);
    w32_d3d_tex *t = get_tex(tex_id);
    if (!d || !t || !pixels_rgba.data || pixels_rgba.len < (int64_t)t->width * (int64_t)t->height) return 0;
    ID3D11DeviceContext_UpdateSubresource(
        d->ctx, (ID3D11Resource *)t->tex, 0, NULL, pixels_rgba.data, (UINT)(t->width * 4), 0
    );
    return 1;
}

void w32_d3d11_bind_texture(uint32_t dev_id, uint32_t slot_idx, uint32_t tex_id) {
    w32_d3d_dev *d = get_dev(dev_id);
    w32_d3d_tex *t = get_tex(tex_id);
    if (!d || !t) return;
    ID3D11DeviceContext_PSSetShaderResources(d->ctx, (UINT)slot_idx, 1, &t->srv);
    ID3D11DeviceContext_PSSetSamplers(d->ctx, (UINT)slot_idx, 1, &t->sampler);
}

void w32_d3d11_destroy_texture(uint32_t tex_id) {
    w32_d3d_tex *t = get_tex(tex_id);
    if (!t) return;
    if (t->sampler) ID3D11SamplerState_Release(t->sampler);
    if (t->srv) ID3D11ShaderResourceView_Release(t->srv);
    if (t->tex) ID3D11Texture2D_Release(t->tex);
    memset(t, 0, sizeof(*t));
}
