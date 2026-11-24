#define D2D_USE_C_DEFINITIONS
#include <string.h>

#include "win32_internal.h"
#include <d2d1.h>

/* ---- Minimal C COM vtable shims for Direct2D & DirectWrite ---------------
   Windows SDK d2d1.h / dwrite.h expose C++ interfaces; slots below are verified
   against Windows SDK 10.0.26100.0 d2d1.h and dwrite.h. */

static const GUID W32_IID_ID2D1Factory =
    { 0x06152247, 0x6f50, 0x465a, { 0x92, 0x45, 0x11, 0x8b, 0xfd, 0x3b, 0x60, 0x07 } };

static const GUID W32_IID_IDWriteFactory =
    { 0xb859ee5a, 0xd838, 0x4b5b, { 0xa2, 0xe8, 0x1a, 0xdc, 0x7d, 0x93, 0xdb, 0x48 } };

typedef HRESULT (WINAPI *pfn_D2D1CreateFactory)(
    D2D1_FACTORY_TYPE, REFIID, const D2D1_FACTORY_OPTIONS *, void **
);
typedef HRESULT (WINAPI *pfn_DWriteCreateFactory)(int, REFIID, void **);

static struct {
    int tried;
    HMODULE h_d2d1;
    HMODULE h_dwrite;
    pfn_D2D1CreateFactory create_d2d_factory;
    pfn_DWriteCreateFactory create_dwrite_factory;
} g_d2d_api;

static int ensure_d2d_api(void) {
    if (g_d2d_api.create_d2d_factory && g_d2d_api.create_dwrite_factory) return 1;
    if (g_d2d_api.tried) return 0;
    g_d2d_api.tried = 1;

    g_d2d_api.h_d2d1 = LoadLibraryW(L"d2d1.dll");
    g_d2d_api.h_dwrite = LoadLibraryW(L"dwrite.dll");
    if (g_d2d_api.h_d2d1) {
        g_d2d_api.create_d2d_factory =
            (pfn_D2D1CreateFactory)(uintptr_t)GetProcAddress(g_d2d_api.h_d2d1, "D2D1CreateFactory");
    }
    if (g_d2d_api.h_dwrite) {
        g_d2d_api.create_dwrite_factory =
            (pfn_DWriteCreateFactory)(uintptr_t)GetProcAddress(g_d2d_api.h_dwrite, "DWriteCreateFactory");
    }
    return (g_d2d_api.create_d2d_factory && g_d2d_api.create_dwrite_factory) ? 1 : 0;
}

/* Generic COM vtable helper: index into interface's vtable array. */
static inline void **com_vtbl(void *obj) {
    return *(void ***)obj;
}

static inline ULONG com_release(void *obj) {
    if (!obj) return 0;
    typedef ULONG (STDMETHODCALLTYPE *pfn_Release)(void *);
    return ((pfn_Release)com_vtbl(obj)[2])(obj);
}

/* ID2D1Factory (slot 14 = CreateHwndRenderTarget) */
static HRESULT d2d_factory_create_hwnd_rt(
    void *factory,
    const D2D1_RENDER_TARGET_PROPERTIES *rt_props,
    const D2D1_HWND_RENDER_TARGET_PROPERTIES *hwnd_props,
    void **out_rt
) {
    typedef HRESULT (STDMETHODCALLTYPE *pfn)(
        void *, const D2D1_RENDER_TARGET_PROPERTIES *, const D2D1_HWND_RENDER_TARGET_PROPERTIES *, void **
    );
    return ((pfn)com_vtbl(factory)[14])(factory, rt_props, hwnd_props, out_rt);
}

/* ID2D1RenderTarget / ID2D1HwndRenderTarget slots */
static HRESULT d2d_rt_create_solid_brush(void *rt, const D2D1_COLOR_F *col, void **out_brush) {
    typedef HRESULT (STDMETHODCALLTYPE *pfn)(void *, const D2D1_COLOR_F *, const D2D1_BRUSH_PROPERTIES *, void **);
    return ((pfn)com_vtbl(rt)[8])(rt, col, NULL, out_brush);
}

static void d2d_brush_set_color(void *brush, const D2D1_COLOR_F *col) {
    typedef void (STDMETHODCALLTYPE *pfn)(void *, const D2D1_COLOR_F *);
    ((pfn)com_vtbl(brush)[8])(brush, col);
}

static void d2d_rt_draw_line(void *rt, D2D1_POINT_2F p0, D2D1_POINT_2F p1, void *brush, float w) {
    typedef void (STDMETHODCALLTYPE *pfn)(void *, D2D1_POINT_2F, D2D1_POINT_2F, void *, float, void *);
    ((pfn)com_vtbl(rt)[15])(rt, p0, p1, brush, w, NULL);
}

static void d2d_rt_draw_rect(void *rt, const D2D1_RECT_F *rc, void *brush, float w) {
    typedef void (STDMETHODCALLTYPE *pfn)(void *, const D2D1_RECT_F *, void *, float, void *);
    ((pfn)com_vtbl(rt)[16])(rt, rc, brush, w, NULL);
}

static void d2d_rt_fill_rect(void *rt, const D2D1_RECT_F *rc, void *brush) {
    typedef void (STDMETHODCALLTYPE *pfn)(void *, const D2D1_RECT_F *, void *);
    ((pfn)com_vtbl(rt)[17])(rt, rc, brush);
}

static void d2d_rt_draw_rounded_rect(void *rt, const D2D1_ROUNDED_RECT *rr, void *brush, float w) {
    typedef void (STDMETHODCALLTYPE *pfn)(void *, const D2D1_ROUNDED_RECT *, void *, float, void *);
    ((pfn)com_vtbl(rt)[18])(rt, rr, brush, w, NULL);
}

static void d2d_rt_fill_rounded_rect(void *rt, const D2D1_ROUNDED_RECT *rr, void *brush) {
    typedef void (STDMETHODCALLTYPE *pfn)(void *, const D2D1_ROUNDED_RECT *, void *);
    ((pfn)com_vtbl(rt)[19])(rt, rr, brush);
}

static void d2d_rt_draw_ellipse(void *rt, const D2D1_ELLIPSE *el, void *brush, float w) {
    typedef void (STDMETHODCALLTYPE *pfn)(void *, const D2D1_ELLIPSE *, void *, float, void *);
    ((pfn)com_vtbl(rt)[20])(rt, el, brush, w, NULL);
}

static void d2d_rt_fill_ellipse(void *rt, const D2D1_ELLIPSE *el, void *brush) {
    typedef void (STDMETHODCALLTYPE *pfn)(void *, const D2D1_ELLIPSE *, void *);
    ((pfn)com_vtbl(rt)[21])(rt, el, brush);
}

static void d2d_rt_draw_text(
    void *rt,
    const wchar_t *str,
    UINT32 len,
    void *text_format,
    const D2D1_RECT_F *layout_rect,
    void *brush
) {
    typedef void (STDMETHODCALLTYPE *pfn)(
        void *, const wchar_t *, UINT32, void *, const D2D1_RECT_F *, void *, D2D1_DRAW_TEXT_OPTIONS, int
    );
    /* D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT = 4, DWRITE_MEASURING_MODE_NATURAL = 0 */
    ((pfn)com_vtbl(rt)[27])(rt, str, len, text_format, layout_rect, brush, D2D1_DRAW_TEXT_OPTIONS_NONE, 0);
}

static void d2d_rt_set_transform(void *rt, const D2D1_MATRIX_3X2_F *m) {
    typedef void (STDMETHODCALLTYPE *pfn)(void *, const D2D1_MATRIX_3X2_F *);
    ((pfn)com_vtbl(rt)[30])(rt, m);
}

static void d2d_rt_clear(void *rt, const D2D1_COLOR_F *col) {
    typedef void (STDMETHODCALLTYPE *pfn)(void *, const D2D1_COLOR_F *);
    ((pfn)com_vtbl(rt)[47])(rt, col);
}

static void d2d_rt_begin_draw(void *rt) {
    typedef void (STDMETHODCALLTYPE *pfn)(void *);
    ((pfn)com_vtbl(rt)[48])(rt);
}

static HRESULT d2d_rt_end_draw(void *rt) {
    typedef HRESULT (STDMETHODCALLTYPE *pfn)(void *, D2D1_TAG *, D2D1_TAG *);
    return ((pfn)com_vtbl(rt)[49])(rt, NULL, NULL);
}

static HRESULT d2d_hwnd_rt_resize(void *rt, const D2D1_SIZE_U *sz) {
    typedef HRESULT (STDMETHODCALLTYPE *pfn)(void *, const D2D1_SIZE_U *);
    return ((pfn)com_vtbl(rt)[58])(rt, sz);
}

/* IDWriteFactory (slot 15 = CreateTextFormat) & IDWriteTextFormat (slot 3, 4) */
static HRESULT dwrite_create_text_format(
    void *dw_factory,
    const wchar_t *family,
    int weight,
    int style,
    int stretch,
    float size,
    const wchar_t *locale,
    void **out_fmt
) {
    typedef HRESULT (STDMETHODCALLTYPE *pfn)(
        void *, const wchar_t *, void *, int, int, int, float, const wchar_t *, void **
    );
    return ((pfn)com_vtbl(dw_factory)[15])(dw_factory, family, NULL, weight, style, stretch, size, locale, out_fmt);
}

static HRESULT dwrite_set_text_align(void *fmt, int align) {
    typedef HRESULT (STDMETHODCALLTYPE *pfn)(void *, int);
    return ((pfn)com_vtbl(fmt)[3])(fmt, align);
}

static HRESULT dwrite_set_para_align(void *fmt, int align) {
    typedef HRESULT (STDMETHODCALLTYPE *pfn)(void *, int);
    return ((pfn)com_vtbl(fmt)[4])(fmt, align);
}

/* ---- Slot tables --------------------------------------------------------- */

#define W32_MAX_D2D_TARGETS 8
#define W32_MAX_D2D_FONTS   64

typedef struct {
    int used;
    HWND hwnd;
    uint8_t vsync;
    void *factory;
    void *dw_factory;
    void *rt;
    void *brush;
} w32_d2d_target;

typedef struct {
    int used;
    void *fmt;
} w32_d2d_font;

static w32_d2d_target g_targets[W32_MAX_D2D_TARGETS];
static w32_d2d_font g_fonts[W32_MAX_D2D_FONTS];

static w32_d2d_target *get_target(uint32_t id) {
    if (id == 0 || id > W32_MAX_D2D_TARGETS) return NULL;
    return g_targets[id - 1].used ? &g_targets[id - 1] : NULL;
}

static w32_d2d_font *get_font(uint32_t id) {
    if (id == 0 || id > W32_MAX_D2D_FONTS) return NULL;
    return g_fonts[id - 1].used ? &g_fonts[id - 1] : NULL;
}

static int create_hwnd_target(w32_d2d_target *t) {
    RECT rc;
    GetClientRect(t->hwnd, &rc);
    UINT32 w = (UINT32)(rc.right > rc.left ? rc.right - rc.left : 1);
    UINT32 h = (UINT32)(rc.bottom > rc.top ? rc.bottom - rc.top : 1);

    D2D1_RENDER_TARGET_PROPERTIES rtp;
    memset(&rtp, 0, sizeof(rtp));
    rtp.type = D2D1_RENDER_TARGET_TYPE_DEFAULT;
    rtp.pixelFormat.format = DXGI_FORMAT_UNKNOWN;
    rtp.pixelFormat.alphaMode = D2D1_ALPHA_MODE_UNKNOWN;
    /* Use 96 DPI so coordinates match physical client pixels 1:1. */
    rtp.dpiX = 96.0f;
    rtp.dpiY = 96.0f;

    D2D1_HWND_RENDER_TARGET_PROPERTIES hrtp;
    hrtp.hwnd = t->hwnd;
    hrtp.pixelSize.width = w;
    hrtp.pixelSize.height = h;
    hrtp.presentOptions = t->vsync ? D2D1_PRESENT_OPTIONS_NONE : D2D1_PRESENT_OPTIONS_IMMEDIATELY;

    if (FAILED(d2d_factory_create_hwnd_rt(t->factory, &rtp, &hrtp, &t->rt)) || !t->rt) {
        return 0;
    }

    D2D1_COLOR_F white = { 1.0f, 1.0f, 1.0f, 1.0f };
    if (FAILED(d2d_rt_create_solid_brush(t->rt, &white, &t->brush)) || !t->brush) {
        com_release(t->rt);
        t->rt = NULL;
        return 0;
    }
    return 1;
}

static void *use_brush(w32_d2d_target *t, w32_colorf c) {
    D2D1_COLOR_F col = { c.r, c.g, c.b, c.a };
    d2d_brush_set_color(t->brush, &col);
    return t->brush;
}

/* ---- Public API ---------------------------------------------------------- */

uint32_t w32_d2d_create(uint64_t hwnd_u64, uint8_t vsync) {
    HWND hwnd = (HWND)(uintptr_t)hwnd_u64;
    if (!hwnd || !ensure_d2d_api()) return 0;

    int slot = -1;
    for (int i = 0; i < W32_MAX_D2D_TARGETS; i++) {
        if (!g_targets[i].used) { slot = i; break; }
    }
    if (slot < 0) return 0;

    w32_d2d_target *t = &g_targets[slot];
    memset(t, 0, sizeof(*t));
    t->hwnd = hwnd;
    t->vsync = vsync ? 1 : 0;

    HRESULT hr = g_d2d_api.create_d2d_factory(
        D2D1_FACTORY_TYPE_SINGLE_THREADED, &W32_IID_ID2D1Factory, NULL, &t->factory
    );
    if (FAILED(hr) || !t->factory) {
        memset(t, 0, sizeof(*t));
        return 0;
    }

    /* DWRITE_FACTORY_TYPE_SHARED = 0 */
    hr = g_d2d_api.create_dwrite_factory(0, &W32_IID_IDWriteFactory, &t->dw_factory);
    if (FAILED(hr) || !t->dw_factory) {
        com_release(t->factory);
        memset(t, 0, sizeof(*t));
        return 0;
    }

    if (!create_hwnd_target(t)) {
        com_release(t->dw_factory);
        com_release(t->factory);
        memset(t, 0, sizeof(*t));
        return 0;
    }

    t->used = 1;
    return (uint32_t)(slot + 1);
}

void w32_d2d_destroy(uint32_t target_id) {
    w32_d2d_target *t = get_target(target_id);
    if (!t) return;
    com_release(t->brush);
    com_release(t->rt);
    com_release(t->dw_factory);
    com_release(t->factory);
    memset(t, 0, sizeof(*t));
}

uint8_t w32_d2d_resize(uint32_t target_id, int32_t width, int32_t height) {
    w32_d2d_target *t = get_target(target_id);
    if (!t || !t->rt || width <= 0 || height <= 0) return 0;
    D2D1_SIZE_U sz = { (UINT32)width, (UINT32)height };
    return SUCCEEDED(d2d_hwnd_rt_resize(t->rt, &sz)) ? 1 : 0;
}

void w32_d2d_begin_draw(uint32_t target_id) {
    w32_d2d_target *t = get_target(target_id);
    if (!t) return;
    if (!t->rt && !create_hwnd_target(t)) return;
    d2d_rt_begin_draw(t->rt);
}

uint8_t w32_d2d_end_draw(uint32_t target_id) {
    w32_d2d_target *t = get_target(target_id);
    if (!t || !t->rt) return 0;
    HRESULT hr = d2d_rt_end_draw(t->rt);
    if (hr == (HRESULT)0x8899000C /* D2DERR_RECREATE_TARGET */) {
        com_release(t->brush);
        com_release(t->rt);
        t->brush = NULL;
        t->rt = NULL;
        create_hwnd_target(t);
        return 1;
    }
    return SUCCEEDED(hr) ? 1 : 0;
}

void w32_d2d_clear(uint32_t target_id, w32_colorf color) {
    w32_d2d_target *t = get_target(target_id);
    if (!t || !t->rt) return;
    D2D1_COLOR_F c = { color.r, color.g, color.b, color.a };
    d2d_rt_clear(t->rt, &c);
}

void w32_d2d_set_transform(uint32_t target_id, float m11, float m12, float m21, float m22, float dx, float dy) {
    w32_d2d_target *t = get_target(target_id);
    if (!t || !t->rt) return;
    D2D1_MATRIX_3X2_F m;
    m._11 = m11; m._12 = m12;
    m._21 = m21; m._22 = m22;
    m._31 = dx;  m._32 = dy;
    d2d_rt_set_transform(t->rt, &m);
}

void w32_d2d_draw_line(uint32_t target_id, float x0, float y0, float x1, float y1, w32_colorf color, float stroke_width) {
    w32_d2d_target *t = get_target(target_id);
    if (!t || !t->rt || !t->brush) return;
    D2D1_POINT_2F p0 = { x0, y0 };
    D2D1_POINT_2F p1 = { x1, y1 };
    d2d_rt_draw_line(t->rt, p0, p1, use_brush(t, color), stroke_width);
}

void w32_d2d_draw_rect(uint32_t target_id, float x, float y, float w, float h, w32_colorf color, float stroke_width) {
    w32_d2d_target *t = get_target(target_id);
    if (!t || !t->rt || !t->brush) return;
    D2D1_RECT_F rc = { x, y, x + w, y + h };
    d2d_rt_draw_rect(t->rt, &rc, use_brush(t, color), stroke_width);
}

void w32_d2d_fill_rect(uint32_t target_id, float x, float y, float w, float h, w32_colorf color) {
    w32_d2d_target *t = get_target(target_id);
    if (!t || !t->rt || !t->brush) return;
    D2D1_RECT_F rc = { x, y, x + w, y + h };
    d2d_rt_fill_rect(t->rt, &rc, use_brush(t, color));
}

void w32_d2d_draw_rounded_rect(uint32_t target_id, float x, float y, float w, float h, float rx, float ry, w32_colorf color, float stroke_width) {
    w32_d2d_target *t = get_target(target_id);
    if (!t || !t->rt || !t->brush) return;
    D2D1_ROUNDED_RECT rr = { { x, y, x + w, y + h }, rx, ry };
    d2d_rt_draw_rounded_rect(t->rt, &rr, use_brush(t, color), stroke_width);
}

void w32_d2d_fill_rounded_rect(uint32_t target_id, float x, float y, float w, float h, float rx, float ry, w32_colorf color) {
    w32_d2d_target *t = get_target(target_id);
    if (!t || !t->rt || !t->brush) return;
    D2D1_ROUNDED_RECT rr = { { x, y, x + w, y + h }, rx, ry };
    d2d_rt_fill_rounded_rect(t->rt, &rr, use_brush(t, color));
}

void w32_d2d_draw_ellipse(uint32_t target_id, float cx, float cy, float rx, float ry, w32_colorf color, float stroke_width) {
    w32_d2d_target *t = get_target(target_id);
    if (!t || !t->rt || !t->brush) return;
    D2D1_ELLIPSE el = { { cx, cy }, rx, ry };
    d2d_rt_draw_ellipse(t->rt, &el, use_brush(t, color), stroke_width);
}

void w32_d2d_fill_ellipse(uint32_t target_id, float cx, float cy, float rx, float ry, w32_colorf color) {
    w32_d2d_target *t = get_target(target_id);
    if (!t || !t->rt || !t->brush) return;
    D2D1_ELLIPSE el = { { cx, cy }, rx, ry };
    d2d_rt_fill_ellipse(t->rt, &el, use_brush(t, color));
}

uint32_t w32_d2d_create_font(uint32_t target_id, w32_bytes family, float size, int32_t weight, uint8_t italic) {
    w32_d2d_target *t = get_target(target_id);
    if (!t || !t->dw_factory) return 0;

    int slot = -1;
    for (int i = 0; i < W32_MAX_D2D_FONTS; i++) {
        if (!g_fonts[i].used) { slot = i; break; }
    }
    if (slot < 0) return 0;

    w32_wide_buf wfam;
    if (!w32_wide_init(&wfam, family)) return 0;
    const wchar_t *fname = wfam.len > 0 ? wfam.ptr : L"Segoe UI";

    void *fmt = NULL;
    /* style: 0 = NORMAL, 2 = ITALIC; stretch: 5 = NORMAL */
    HRESULT hr = dwrite_create_text_format(
        t->dw_factory, fname, weight > 0 ? (int)weight : 400, italic ? 2 : 0, 5, size, L"", &fmt
    );
    w32_wide_free(&wfam);
    if (FAILED(hr) || !fmt) return 0;

    g_fonts[slot].fmt = fmt;
    g_fonts[slot].used = 1;
    return (uint32_t)(slot + 1);
}

void w32_d2d_set_font_align(uint32_t font_id, int32_t h_align, int32_t v_align) {
    w32_d2d_font *f = get_font(font_id);
    if (!f || !f->fmt) return;
    dwrite_set_text_align(f->fmt, (int)h_align);
    dwrite_set_para_align(f->fmt, (int)v_align);
}

void w32_d2d_destroy_font(uint32_t font_id) {
    w32_d2d_font *f = get_font(font_id);
    if (!f) return;
    com_release(f->fmt);
    memset(f, 0, sizeof(*f));
}

void w32_d2d_draw_text(
    uint32_t target_id,
    w32_bytes text,
    uint32_t font_id,
    float x, float y, float w, float h,
    w32_colorf color
) {
    w32_d2d_target *t = get_target(target_id);
    w32_d2d_font *f = get_font(font_id);
    if (!t || !t->rt || !t->brush || !f || !f->fmt) return;

    w32_wide_buf wb;
    if (!w32_wide_init(&wb, text)) return;
    D2D1_RECT_F rc = { x, y, x + w, y + h };
    d2d_rt_draw_text(t->rt, wb.ptr, (UINT32)wb.len, f->fmt, &rc, use_brush(t, color));
    w32_wide_free(&wb);
}
