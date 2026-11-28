#ifndef WIN32_GOOSE_INTERNAL_H
#define WIN32_GOOSE_INTERNAL_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include "win32_shim.h"

#define W32_STACK_WIDE_CAP 256

typedef struct {
    wchar_t stack[W32_STACK_WIDE_CAP];
    wchar_t *ptr;
    int len; /* wide char count excluding NUL */
} w32_wide_buf;

int w32_wide_init(w32_wide_buf *wb, w32_bytes s);
void w32_wide_free(w32_wide_buf *wb);
uint8_t w32_append_wide_as_utf8(const wchar_t *ws, int wlen, gs_rref out);

/* Drag & drop capture invoked from w32_wndproc on WM_DROPFILES. */
uint32_t w32_capture_hdrop(void *hdrop);

#define W32_WM_WEBVIEW_EVENT (WM_APP + 2)
#define W32_WM_HTTP_ASYNC    (WM_APP + 3)

/* Parent window teardown hook for active child WebView2 instances. */
void w32_webview_on_parent_destroy(HWND hwnd);

/* Typed Goose builder append helpers (advances stack top by count * sizeof(T)
   and header len by element count). */
static inline void w32_bld_append_u32(gs_rref b, const uint32_t *p, int64_t count) {
    if (count <= 0) return;
    memcpy(b.stk->top, p, (size_t)count * sizeof(uint32_t));
    b.stk->top += count * (int64_t)sizeof(uint32_t);
    b.hdr->len += count;
}

static inline void w32_bld_append_f32(gs_rref b, const float *p, int64_t count) {
    if (count <= 0) return;
    memcpy(b.stk->top, p, (size_t)count * sizeof(float));
    b.stk->top += count * (int64_t)sizeof(float);
    b.hdr->len += count;
}

#endif /* WIN32_GOOSE_INTERNAL_H */
