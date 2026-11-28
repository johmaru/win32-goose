#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "win32_internal.h"

/* ---- UTF-8 <-> UTF-16 conversion helpers --------------------------------- */

int w32_wide_init(w32_wide_buf *wb, w32_bytes s) {
    wb->ptr = wb->stack;
    wb->len = 0;
    wb->stack[0] = L'\0';
    if (!s.data || s.len <= 0) return 1;
    if (s.len > INT_MAX) return 0;

    int n = MultiByteToWideChar(CP_UTF8, 0, (const char *)s.data, (int)s.len, NULL, 0);
    if (n <= 0) return 0;

    if (n + 1 <= W32_STACK_WIDE_CAP) {
        wb->ptr = wb->stack;
    } else {
        wb->ptr = (wchar_t *)malloc((size_t)(n + 1) * sizeof(wchar_t));
        if (!wb->ptr) {
            wb->ptr = wb->stack;
            return 0;
        }
    }
    MultiByteToWideChar(CP_UTF8, 0, (const char *)s.data, (int)s.len, wb->ptr, n);
    wb->ptr[n] = L'\0';
    wb->len = n;
    return 1;
}

void w32_wide_free(w32_wide_buf *wb) {
    if (wb->ptr && wb->ptr != wb->stack) {
        free(wb->ptr);
    }
    wb->ptr = NULL;
    wb->len = 0;
}

uint8_t w32_append_wide_as_utf8(const wchar_t *ws, int wlen, gs_rref out) {
    if (!ws || wlen == 0) return 1;
    int n = WideCharToMultiByte(CP_UTF8, 0, ws, wlen, NULL, 0, NULL, NULL);
    if (n <= 0) return 0;
    char *buf = (char *)malloc((size_t)n);
    if (!buf) return 0;
    int written = WideCharToMultiByte(CP_UTF8, 0, ws, wlen, buf, n, NULL, NULL);
    if (written > 0) {
        /* If wlen == -1, WideCharToMultiByte includes the trailing NUL. */
        int actual = (wlen < 0 && buf[written - 1] == '\0') ? written - 1 : written;
        gs_bld_append(out, buf, (int64_t)actual);
    }
    free(buf);
    return written > 0 ? 1 : 0;
}

/* ---- Event ring buffer --------------------------------------------------- */

#define W32_EVENT_CAP 1024

static w32_event g_events[W32_EVENT_CAP];
static uint32_t g_ev_head = 0;
static uint32_t g_ev_tail = 0;

static int w32_queue_empty(void) {
    return g_ev_head == g_ev_tail;
}

static void w32_queue_push(const w32_event *ev) {
    /* Coalesce consecutive WM_PAINT, WM_SIZE, WM_MOVE, W32_WM_WEBVIEW_EVENT, or W32_WM_HTTP_ASYNC events for the same window. */
    if ((ev->msg == WM_PAINT || ev->msg == WM_SIZE || ev->msg == WM_MOVE || ev->msg == W32_WM_WEBVIEW_EVENT || ev->msg == W32_WM_HTTP_ASYNC) && !w32_queue_empty()) {
        uint32_t idx = g_ev_head;
        while (idx != g_ev_tail) {
            if (g_events[idx].msg == ev->msg && g_events[idx].hwnd == ev->hwnd) {
                if (ev->msg == WM_SIZE) {
                    g_events[idx].wparam = ev->wparam;
                    g_events[idx].lparam = ev->lparam;
                    g_events[idx].width = ev->width;
                    g_events[idx].height = ev->height;
                } else if (ev->msg == WM_MOVE) {
                    g_events[idx].wparam = ev->wparam;
                    g_events[idx].lparam = ev->lparam;
                    g_events[idx].x = ev->x;
                    g_events[idx].y = ev->y;
                }
                return;
            }
            idx = (idx + 1) % W32_EVENT_CAP;
        }
    }
    uint32_t next_tail = (g_ev_tail + 1) % W32_EVENT_CAP;
    if (next_tail == g_ev_head) {
        g_ev_head = (g_ev_head + 1) % W32_EVENT_CAP;
    }
    g_events[g_ev_tail] = *ev;
    g_ev_tail = next_tail;
}

static int w32_queue_pop(w32_event *out) {
    if (w32_queue_empty()) return 0;
    *out = g_events[g_ev_head];
    g_ev_head = (g_ev_head + 1) % W32_EVENT_CAP;
    return 1;
}

/* ---- Minimum client size table & modal-sizemove fiber state -------------- */

#define W32_MAX_MIN_SIZES 32
#define W32_FIBER_FLAG_FLOAT_SWITCH 0x1

typedef struct {
    HWND hwnd;
    int32_t min_w;
    int32_t min_h;
    int active;
} w32_min_size_entry;

static w32_min_size_entry g_min_sizes[W32_MAX_MIN_SIZES];
static HWND g_suppress_erase[W32_MAX_MIN_SIZES];

static int w32_is_erase_suppressed(HWND hwnd) {
    if (!hwnd) return 0;
    for (int i = 0; i < W32_MAX_MIN_SIZES; i++) {
        if (g_suppress_erase[i] == hwnd) return 1;
    }
    return 0;
}

uint8_t w32_set_window_suppress_erase(uint64_t hwnd_u64, uint8_t suppress) {
    HWND hwnd = (HWND)(uintptr_t)hwnd_u64;
    if (!hwnd) return 0;
    int idx = -1;
    int free_idx = -1;
    for (int i = 0; i < W32_MAX_MIN_SIZES; i++) {
        if (g_suppress_erase[i] == hwnd) {
            idx = i;
            break;
        }
        if (g_suppress_erase[i] == NULL && free_idx < 0) {
            free_idx = i;
        }
    }
    if (!suppress) {
        if (idx >= 0) g_suppress_erase[idx] = NULL;
        return 1;
    }
    if (idx >= 0) return 1;
    if (free_idx < 0) return 0;
    g_suppress_erase[free_idx] = hwnd;
    return 1;
}

typedef LPVOID (WINAPI *pfn_w32_ConvertThreadToFiberEx)(LPVOID, DWORD);
typedef LPVOID (WINAPI *pfn_w32_CreateFiberEx)(SIZE_T, SIZE_T, DWORD, LPFIBER_START_ROUTINE, LPVOID);
typedef void   (WINAPI *pfn_w32_SwitchToFiber)(LPVOID);

static struct {
    int init_state; /* 0 = uninitialized, 1 = ready, -1 = unavailable */
    DWORD tid;
    LPVOID main_fiber;
    LPVOID dispatch_fiber;
    MSG dispatch_msg;
    int dispatch_pending;
    int on_dispatch_fiber;
    int in_sizemove;
    int yielded_this_pump;
    pfn_w32_ConvertThreadToFiberEx convert_thread_to_fiber_ex;
    pfn_w32_CreateFiberEx create_fiber_ex;
    pfn_w32_SwitchToFiber switch_to_fiber;
} g_fiber;

static VOID WINAPI w32_dispatch_fiber_proc(LPVOID param) {
    (void)param;
    for (;;) {
        DispatchMessageW(&g_fiber.dispatch_msg);
        g_fiber.dispatch_pending = 0;
        g_fiber.in_sizemove = 0;
        g_fiber.yielded_this_pump = 0;
        g_fiber.on_dispatch_fiber = 0;
        g_fiber.switch_to_fiber(g_fiber.main_fiber);
    }
}

static int w32_ensure_fibers(void) {
    if (g_fiber.init_state == 1) {
        return GetCurrentThreadId() == g_fiber.tid ? 1 : 0;
    }
    if (g_fiber.init_state < 0) return 0;

    HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
    if (!k32) k32 = LoadLibraryW(L"kernel32.dll");
    if (!k32) {
        g_fiber.init_state = -1;
        return 0;
    }
    g_fiber.convert_thread_to_fiber_ex =
        (pfn_w32_ConvertThreadToFiberEx)(uintptr_t)GetProcAddress(k32, "ConvertThreadToFiberEx");
    g_fiber.create_fiber_ex =
        (pfn_w32_CreateFiberEx)(uintptr_t)GetProcAddress(k32, "CreateFiberEx");
    g_fiber.switch_to_fiber =
        (pfn_w32_SwitchToFiber)(uintptr_t)GetProcAddress(k32, "SwitchToFiber");

    if (!g_fiber.convert_thread_to_fiber_ex || !g_fiber.create_fiber_ex || !g_fiber.switch_to_fiber) {
        g_fiber.init_state = -1;
        return 0;
    }

    LPVOID main_fib = g_fiber.convert_thread_to_fiber_ex(NULL, W32_FIBER_FLAG_FLOAT_SWITCH);
    if (!main_fib) {
        g_fiber.init_state = -1;
        return 0;
    }

    LPVOID disp_fib = g_fiber.create_fiber_ex(
        65536, 1048576, W32_FIBER_FLAG_FLOAT_SWITCH, w32_dispatch_fiber_proc, NULL
    );
    if (!disp_fib) {
        g_fiber.init_state = -1;
        return 0;
    }

    g_fiber.tid = GetCurrentThreadId();
    g_fiber.main_fiber = main_fib;
    g_fiber.dispatch_fiber = disp_fib;
    g_fiber.init_state = 1;
    return 1;
}

static void w32_maybe_yield_sizemove(void) {
    if (g_fiber.init_state == 1 &&
        g_fiber.on_dispatch_fiber &&
        g_fiber.in_sizemove > 0 &&
        g_fiber.main_fiber &&
        GetCurrentThreadId() == g_fiber.tid) {
        g_fiber.yielded_this_pump = 1;
        g_fiber.on_dispatch_fiber = 0;
        g_fiber.switch_to_fiber(g_fiber.main_fiber);
    }
}

static void w32_dispatch_os_msg(const MSG *msg) {
    UINT m = msg->message;
    /* Route messages that enter DefWindowProc's modal move/resize loop on a
       worker fiber so WM_SIZE / WM_PAINT / WM_TIMER can yield live frames. */
    int may_enter_modal = (m == WM_NCLBUTTONDOWN || m == WM_SYSCOMMAND || m == 0x0242 /* WM_NCPOINTERDOWN */);
    if (may_enter_modal && !g_fiber.dispatch_pending && w32_ensure_fibers()) {
        g_fiber.dispatch_msg = *msg;
        g_fiber.dispatch_pending = 1;
        g_fiber.yielded_this_pump = 0;
        g_fiber.on_dispatch_fiber = 1;
        g_fiber.switch_to_fiber(g_fiber.dispatch_fiber);
        g_fiber.on_dispatch_fiber = 0;
        return;
    }
    DispatchMessageW(msg);
}

uint8_t w32_set_window_min_size(uint64_t hwnd_u64, int32_t min_client_w, int32_t min_client_h) {
    HWND hwnd = (HWND)(uintptr_t)hwnd_u64;
    if (!hwnd) return 0;

    int idx = -1;
    int free_idx = -1;
    for (int i = 0; i < W32_MAX_MIN_SIZES; i++) {
        if (g_min_sizes[i].active && g_min_sizes[i].hwnd == hwnd) {
            idx = i;
            break;
        }
        if (!g_min_sizes[i].active && free_idx < 0) {
            free_idx = i;
        }
    }

    if (min_client_w <= 0 && min_client_h <= 0) {
        if (idx >= 0) memset(&g_min_sizes[idx], 0, sizeof(g_min_sizes[idx]));
        return 1;
    }

    int slot = (idx >= 0) ? idx : free_idx;
    if (slot < 0) return 0;
    g_min_sizes[slot].hwnd = hwnd;
    g_min_sizes[slot].min_w = min_client_w;
    g_min_sizes[slot].min_h = min_client_h;
    g_min_sizes[slot].active = 1;
    return 1;
}

/* ---- WndProc ------------------------------------------------------------- */

static int32_t w32_lo_i16(LPARAM lp) {
    return (int32_t)(int16_t)(lp & 0xFFFF);
}

static int32_t w32_hi_i16(LPARAM lp) {
    return (int32_t)(int16_t)((lp >> 16) & 0xFFFF);
}

static void w32_cleanup_window_state(HWND hwnd);

static LRESULT CALLBACK w32_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    w32_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.hwnd = (uint64_t)(uintptr_t)hwnd;
    ev.msg = (uint32_t)msg;
    ev.wparam = (uint64_t)wp;
    ev.lparam = (int64_t)lp;

    switch (msg) {
        case WM_CLOSE:
            w32_queue_push(&ev);
            return 0;

        case WM_DESTROY:
            g_fiber.in_sizemove = 0;
            w32_cleanup_window_state(hwnd);
            w32_queue_push(&ev);
            return 0;

        case WM_ERASEBKGND:
            /* Preserve default background fill for parent windows hosting child controls;
               suppress erase for custom-drawn / GPU windows or WebView2 hosts to eliminate flicker. */
            if (!w32_is_erase_suppressed(hwnd) && GetWindow(hwnd, GW_CHILD) != NULL) {
                return DefWindowProcW(hwnd, msg, wp, lp);
            }
            return 1;

        case WM_MOVE:
            ev.x = w32_lo_i16(lp);
            ev.y = w32_hi_i16(lp);
            w32_queue_push(&ev);
            return DefWindowProcW(hwnd, msg, wp, lp);

        case WM_ENTERSIZEMOVE:
            g_fiber.in_sizemove = 1;
            return 0;

        case WM_EXITSIZEMOVE:
            g_fiber.in_sizemove = 0;
            return 0;

        case WM_GETMINMAXINFO: {
            LRESULT res = DefWindowProcW(hwnd, msg, wp, lp);
            for (int i = 0; i < W32_MAX_MIN_SIZES; i++) {
                if (g_min_sizes[i].active && g_min_sizes[i].hwnd == hwnd) {
                    MINMAXINFO *mmi = (MINMAXINFO *)(uintptr_t)lp;
                    if (mmi) {
                        RECT rc = {
                            0,
                            0,
                            g_min_sizes[i].min_w > 0 ? g_min_sizes[i].min_w : 0,
                            g_min_sizes[i].min_h > 0 ? g_min_sizes[i].min_h : 0
                        };
                        DWORD style = (DWORD)GetWindowLongPtrW(hwnd, GWL_STYLE);
                        DWORD ex_style = (DWORD)GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
                        BOOL has_menu = (GetMenu(hwnd) != NULL) ? TRUE : FALSE;
                        AdjustWindowRectEx(&rc, style, has_menu, ex_style);
                        if (g_min_sizes[i].min_w > 0) {
                            LONG mw = rc.right - rc.left;
                            if (mw > mmi->ptMinTrackSize.x) mmi->ptMinTrackSize.x = mw;
                        }
                        if (g_min_sizes[i].min_h > 0) {
                            LONG mh = rc.bottom - rc.top;
                            if (mh > mmi->ptMinTrackSize.y) mmi->ptMinTrackSize.y = mh;
                        }
                    }
                    return 0;
                }
            }
            return res;
        }

        case WM_PAINT:
            ValidateRect(hwnd, NULL);
            w32_queue_push(&ev);
            w32_maybe_yield_sizemove();
            return 0;

        case WM_SIZE:
            ev.width = (int32_t)(uint16_t)(lp & 0xFFFF);
            ev.height = (int32_t)(uint16_t)((lp >> 16) & 0xFFFF);
            w32_queue_push(&ev);
            w32_maybe_yield_sizemove();
            ValidateRect(hwnd, NULL);
            return 0;

        case WM_COMMAND:
            ev.command_id = (uint32_t)(wp & 0xFFFF);
            ev.notify_code = (uint32_t)((wp >> 16) & 0xFFFF);
            w32_queue_push(&ev);
            return 0;

        case WM_HOTKEY:
            ev.command_id = (uint32_t)wp;
            ev.key = (uint32_t)((lp >> 16) & 0xFFFF);
            w32_queue_push(&ev);
            return 0;

        case WM_DROPFILES:
            ev.command_id = w32_capture_hdrop((void *)wp);
            w32_queue_push(&ev);
            return 0;

        case (WM_APP + 1): /* WM_TRAYICON */
            ev.command_id = (uint32_t)wp;
            ev.notify_code = (uint32_t)(lp & 0xFFFF);
            w32_queue_push(&ev);
            return 0;

        case W32_WM_WEBVIEW_EVENT:
        case W32_WM_HTTP_ASYNC:
            ev.command_id = (uint32_t)wp;
            w32_queue_push(&ev);
            return 0;

        case WM_INPUT: {
            UINT sz = 0;
            GetRawInputData((HRAWINPUT)lp, RID_INPUT, NULL, &sz, sizeof(RAWINPUTHEADER));
            if (sz > 0 && sz <= 256) {
                union {
                    RAWINPUT ri;
                    uint8_t bytes[256];
                } raw_buf;
                if (GetRawInputData((HRAWINPUT)lp, RID_INPUT, raw_buf.bytes, &sz, sizeof(RAWINPUTHEADER)) == sz) {
                    const RAWINPUT *ri = &raw_buf.ri;
                    if (ri->header.dwType == RIM_TYPEMOUSE) {
                        ev.x = (int32_t)ri->data.mouse.lLastX;
                        ev.y = (int32_t)ri->data.mouse.lLastY;
                        ev.wparam = (uint64_t)ri->data.mouse.usButtonFlags;
                        w32_queue_push(&ev);
                    }
                }
            }
            return DefWindowProcW(hwnd, msg, wp, lp);
        }

        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
            ev.key = (uint32_t)wp;
            w32_queue_push(&ev);
            return DefWindowProcW(hwnd, msg, wp, lp);

        case WM_CHAR:
            ev.codepoint = (uint32_t)wp;
            w32_queue_push(&ev);
            return 0;

        case WM_MOUSEMOVE:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
            ev.x = w32_lo_i16(lp);
            ev.y = w32_hi_i16(lp);
            w32_queue_push(&ev);
            return 0;

        case WM_MOUSEWHEEL: {
            POINT pt;
            pt.x = w32_lo_i16(lp);
            pt.y = w32_hi_i16(lp);
            ScreenToClient(hwnd, &pt);
            ev.x = (int32_t)pt.x;
            ev.y = (int32_t)pt.y;
            ev.wheel_delta = (int32_t)(int16_t)((wp >> 16) & 0xFFFF);
            w32_queue_push(&ev);
            return 0;
        }

        case WM_SETFOCUS:
        case WM_KILLFOCUS:
            w32_queue_push(&ev);
            return DefWindowProcW(hwnd, msg, wp, lp);

        case WM_TIMER: {
            w32_queue_push(&ev);
            LRESULT res = DefWindowProcW(hwnd, msg, wp, lp);
            w32_maybe_yield_sizemove();
            return res;
        }

        case WM_DPICHANGED: {
            /* Copy DPI and suggested rect dimensions; do not queue dangling lParam RECT*. */
            ev.x = (int32_t)(uint16_t)(wp & 0xFFFF);
            ev.y = (int32_t)(uint16_t)((wp >> 16) & 0xFFFF);
            if (lp) {
                const RECT *suggested = (const RECT *)(uintptr_t)lp;
                ev.width = (int32_t)(suggested->right - suggested->left);
                ev.height = (int32_t)(suggested->bottom - suggested->top);
            }
            ev.lparam = 0;
            w32_queue_push(&ev);
            return DefWindowProcW(hwnd, msg, wp, lp);
        }

        default:
            return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

/* ---- Window creation & event loop ---------------------------------------- */

typedef BOOL (WINAPI *pfn_SetProcessDpiAwarenessContext)(HANDLE);

uint8_t w32_enable_dpi_awareness(void) {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (!user32) user32 = LoadLibraryW(L"user32.dll");
    if (user32) {
        pfn_SetProcessDpiAwarenessContext fn =
            (pfn_SetProcessDpiAwarenessContext)(uintptr_t)GetProcAddress(
                user32, "SetProcessDpiAwarenessContext");
        if (fn) {
            if (fn((HANDLE)(intptr_t)-4)) return 1;
        }
    }
    return SetProcessDPIAware() ? 1 : 0;
}

uint8_t w32_register_class(w32_bytes class_name, uint32_t style) {
    w32_wide_buf wclass;
    if (!w32_wide_init(&wclass, class_name) || wclass.len == 0) {
        w32_wide_free(&wclass);
        return 0;
    }

    WNDCLASSEXW wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = style;
    wc.lpfnWndProc = w32_wndproc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.hCursor = LoadCursorW(NULL, MAKEINTRESOURCEW(32512)); /* IDC_ARROW */
    wc.hbrBackground = (HBRUSH)(uintptr_t)(COLOR_WINDOW + 1);
    wc.lpszClassName = wclass.ptr;

    ATOM atom = RegisterClassExW(&wc);
    DWORD err = atom ? 0 : GetLastError();
    w32_wide_free(&wclass);
    return (atom != 0 || err == ERROR_CLASS_ALREADY_EXISTS) ? 1 : 0;
}

uint64_t w32_create_window(
    w32_bytes class_name,
    w32_bytes title,
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height,
    uint32_t style,
    uint32_t ex_style,
    uint64_t parent
) {
    w32_wide_buf wclass, wtitle;
    if (!w32_wide_init(&wclass, class_name)) return 0;
    if (!w32_wide_init(&wtitle, title)) {
        w32_wide_free(&wclass);
        return 0;
    }

    HWND hwnd = CreateWindowExW(
        (DWORD)ex_style,
        wclass.ptr,
        wtitle.ptr,
        (DWORD)style,
        (int)x,
        (int)y,
        (int)width,
        (int)height,
        (HWND)(uintptr_t)parent,
        NULL,
        GetModuleHandleW(NULL),
        NULL
    );

    w32_wide_free(&wtitle);
    w32_wide_free(&wclass);
    return (uint64_t)(uintptr_t)hwnd;
}

uint8_t w32_poll_event(w32_event *out) {
    if (w32_queue_pop(out)) return 1;
    if (g_fiber.dispatch_pending) {
        if (g_fiber.yielded_this_pump) {
            g_fiber.yielded_this_pump = 0;
            return 0;
        }
        g_fiber.on_dispatch_fiber = 1;
        g_fiber.switch_to_fiber(g_fiber.dispatch_fiber);
        g_fiber.on_dispatch_fiber = 0;
        if (w32_queue_pop(out)) return 1;
        if (g_fiber.dispatch_pending) {
            g_fiber.yielded_this_pump = 0;
            return 0;
        }
    }

    MSG msg;
    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            memset(out, 0, sizeof(*out));
            out->msg = WM_QUIT;
            out->wparam = (uint64_t)msg.wParam;
            out->lparam = (int64_t)msg.lParam;
            return 1;
        }
        TranslateMessage(&msg);
        w32_dispatch_os_msg(&msg);
        if (w32_queue_pop(out)) return 1;
        if (g_fiber.dispatch_pending) {
            g_fiber.yielded_this_pump = 0;
            return 0;
        }
    }
    return 0;
}

uint8_t w32_wait_event(w32_event *out) {
    if (w32_queue_pop(out)) {
        return out->msg == WM_QUIT ? 0 : 1;
    }

    MSG msg;
    for (;;) {
        if (g_fiber.dispatch_pending) {
            g_fiber.yielded_this_pump = 0;
            g_fiber.on_dispatch_fiber = 1;
            g_fiber.switch_to_fiber(g_fiber.dispatch_fiber);
            g_fiber.on_dispatch_fiber = 0;
            if (w32_queue_pop(out)) {
                return out->msg == WM_QUIT ? 0 : 1;
            }
            continue;
        }

        BOOL ret = GetMessageW(&msg, NULL, 0, 0);
        if (ret <= 0) {
            memset(out, 0, sizeof(*out));
            out->msg = WM_QUIT;
            out->wparam = (uint64_t)msg.wParam;
            out->lparam = (int64_t)msg.lParam;
            return 0;
        }
        TranslateMessage(&msg);
        w32_dispatch_os_msg(&msg);
        if (w32_queue_pop(out)) {
            return out->msg == WM_QUIT ? 0 : 1;
        }
    }
}

/* ---- Double-buffered paint session --------------------------------------- */

typedef struct {
    HWND hwnd;
    HDC win_dc;
    HDC mem_dc;
    HBITMAP bmp;
    HGDIOBJ old_bmp;
    int32_t width;
    int32_t height;
    int active;
} w32_paint_state;

static w32_paint_state g_paint;

uint64_t w32_begin_paint(uint64_t hwnd_u64, w32_rect *out_rect) {
    HWND hwnd = (HWND)(uintptr_t)hwnd_u64;
    if (!hwnd || g_paint.active) return 0;

    RECT rc;
    if (!GetClientRect(hwnd, &rc)) return 0;
    int32_t w = (int32_t)(rc.right - rc.left);
    int32_t h = (int32_t)(rc.bottom - rc.top);
    if (w <= 0) w = 1;
    if (h <= 0) h = 1;

    HDC win_dc = GetDC(hwnd);
    if (!win_dc) return 0;

    HDC mem_dc = CreateCompatibleDC(win_dc);
    HBITMAP bmp = CreateCompatibleBitmap(win_dc, w, h);
    if (!mem_dc || !bmp) {
        if (bmp) DeleteObject(bmp);
        if (mem_dc) DeleteDC(mem_dc);
        ReleaseDC(hwnd, win_dc);
        return 0;
    }

    HGDIOBJ old_bmp = SelectObject(mem_dc, bmp);
    RECT fill_rc = { 0, 0, w, h };
    FillRect(mem_dc, &fill_rc, GetSysColorBrush(COLOR_WINDOW));

    g_paint.hwnd = hwnd;
    g_paint.win_dc = win_dc;
    g_paint.mem_dc = mem_dc;
    g_paint.bmp = bmp;
    g_paint.old_bmp = old_bmp;
    g_paint.width = w;
    g_paint.height = h;
    g_paint.active = 1;

    if (out_rect) {
        out_rect->left = (int32_t)rc.left;
        out_rect->top = (int32_t)rc.top;
        out_rect->right = (int32_t)rc.right;
        out_rect->bottom = (int32_t)rc.bottom;
    }
    return (uint64_t)(uintptr_t)mem_dc;
}

void w32_end_paint(uint64_t hwnd_u64) {
    if (!g_paint.active) return;
    if (hwnd_u64 != 0 && g_paint.hwnd != (HWND)(uintptr_t)hwnd_u64) return;

    BitBlt(g_paint.win_dc, 0, 0, g_paint.width, g_paint.height, g_paint.mem_dc, 0, 0, SRCCOPY);
    SelectObject(g_paint.mem_dc, g_paint.old_bmp);
    DeleteObject(g_paint.bmp);
    DeleteDC(g_paint.mem_dc);
    ReleaseDC(g_paint.hwnd, g_paint.win_dc);
    memset(&g_paint, 0, sizeof(g_paint));
}

/* ---- Aligned UTF-16 wrappers for *W APIs --------------------------------- */

uint8_t w32_set_window_text(uint64_t hwnd_u64, w32_bytes text) {
    w32_wide_buf wb;
    if (!w32_wide_init(&wb, text)) return 0;
    BOOL ok = SetWindowTextW((HWND)(uintptr_t)hwnd_u64, wb.ptr);
    w32_wide_free(&wb);
    return ok ? 1 : 0;
}

int32_t w32_message_box(uint64_t hwnd_u64, w32_bytes text, w32_bytes caption, uint32_t flags) {
    w32_wide_buf wtext, wcap;
    if (!w32_wide_init(&wtext, text)) return 0;
    if (!w32_wide_init(&wcap, caption)) {
        w32_wide_free(&wtext);
        return 0;
    }
    int ret = MessageBoxW((HWND)(uintptr_t)hwnd_u64, wtext.ptr, wcap.ptr, (UINT)flags);
    w32_wide_free(&wcap);
    w32_wide_free(&wtext);
    return (int32_t)ret;
}

int32_t w32_draw_text(uint64_t hdc_u64, w32_bytes text, w32_rect *rect, uint32_t format) {
    if (!rect) return 0;
    w32_wide_buf wb;
    if (!w32_wide_init(&wb, text)) return 0;
    RECT rc = { rect->left, rect->top, rect->right, rect->bottom };
    int ret = DrawTextW((HDC)(uintptr_t)hdc_u64, wb.ptr, wb.len, &rc, (UINT)format);
    rect->left = (int32_t)rc.left;
    rect->top = (int32_t)rc.top;
    rect->right = (int32_t)rc.right;
    rect->bottom = (int32_t)rc.bottom;
    w32_wide_free(&wb);
    return (int32_t)ret;
}

uint8_t w32_text_out(uint64_t hdc_u64, int32_t x, int32_t y, w32_bytes text) {
    w32_wide_buf wb;
    if (!w32_wide_init(&wb, text)) return 0;
    BOOL ok = TextOutW((HDC)(uintptr_t)hdc_u64, (int)x, (int)y, wb.ptr, wb.len);
    w32_wide_free(&wb);
    return ok ? 1 : 0;
}

/* ---- Menu helpers -------------------------------------------------------- */

uint8_t w32_append_menu(uint64_t hmenu_u64, uint32_t flags, uint64_t id_or_submenu, w32_bytes text) {
    HMENU hmenu = (HMENU)(uintptr_t)hmenu_u64;
    if (!hmenu) return 0;
    if (flags & MF_SEPARATOR) {
        return AppendMenuW(hmenu, (UINT)flags, 0, NULL) ? 1 : 0;
    }
    w32_wide_buf wb;
    if (!w32_wide_init(&wb, text)) return 0;
    BOOL ok = AppendMenuW(hmenu, (UINT)flags, (UINT_PTR)id_or_submenu, wb.ptr);
    w32_wide_free(&wb);
    return ok ? 1 : 0;
}

uint8_t w32_get_menu_string(uint64_t hmenu_u64, uint32_t id, uint32_t flags, gs_rref out) {
    HMENU hmenu = (HMENU)(uintptr_t)hmenu_u64;
    if (!hmenu) return 0;
    int len = GetMenuStringW(hmenu, (UINT)id, NULL, 0, (UINT)flags);
    if (len <= 0) return 0;
    wchar_t *buf = (wchar_t *)malloc((size_t)(len + 1) * sizeof(wchar_t));
    if (!buf) return 0;
    int got = GetMenuStringW(hmenu, (UINT)id, buf, len + 1, (UINT)flags);
    uint8_t ok = got > 0 ? w32_append_wide_as_utf8(buf, got, out) : 0;
    free(buf);
    return ok;
}

/* ---- UTF-8 builder helpers ----------------------------------------------- */

uint8_t w32_get_window_text(uint64_t hwnd_u64, gs_rref out) {
    HWND hwnd = (HWND)(uintptr_t)hwnd_u64;
    if (!hwnd) return 0;
    int len = GetWindowTextLengthW(hwnd);
    if (len <= 0) return 1;
    wchar_t *buf = (wchar_t *)malloc((size_t)(len + 1) * sizeof(wchar_t));
    if (!buf) return 0;
    int got = GetWindowTextW(hwnd, buf, len + 1);
    uint8_t ok = got > 0 ? w32_append_wide_as_utf8(buf, got, out) : 1;
    free(buf);
    return ok;
}

uint8_t w32_format_error(uint32_t error_code, gs_rref out) {
    wchar_t *msg = NULL;
    DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    DWORD len = FormatMessageW(flags, NULL, (DWORD)error_code, 0, (LPWSTR)&msg, 0, NULL);
    if (len == 0 || !msg) {
        return 0;
    }
    while (len > 0 && (msg[len - 1] == L'\r' || msg[len - 1] == L'\n')) {
        len--;
    }
    uint8_t ok = w32_append_wide_as_utf8(msg, (int)len, out);
    LocalFree(msg);
    return ok;
}

/* ---- Cursor, Capture, Borderless Fullscreen, Raw Input ------------------- */

uint8_t w32_set_system_cursor(uint32_t idc) {
    HCURSOR cur = LoadCursorW(NULL, MAKEINTRESOURCEW(idc));
    if (!cur) return 0;
    SetCursor(cur);
    return 1;
}

uint8_t w32_clip_cursor_window(uint64_t hwnd_u64) {
    HWND hwnd = (HWND)(uintptr_t)hwnd_u64;
    if (!hwnd) return 0;
    RECT rc;
    if (!GetClientRect(hwnd, &rc)) return 0;
    POINT tl = { rc.left, rc.top };
    POINT br = { rc.right, rc.bottom };
    ClientToScreen(hwnd, &tl);
    ClientToScreen(hwnd, &br);
    RECT screen_rc = { tl.x, tl.y, br.x, br.y };
    return ClipCursor(&screen_rc) ? 1 : 0;
}

uint8_t w32_unclip_cursor(void) {
    return ClipCursor(NULL) ? 1 : 0;
}

#define W32_MAX_FULLSCREEN 16

typedef struct {
    HWND hwnd;
    DWORD saved_style;
    WINDOWPLACEMENT saved_placement;
    int active;
} w32_fullscreen_state;

static w32_fullscreen_state g_fs[W32_MAX_FULLSCREEN];

static void w32_cleanup_window_state(HWND hwnd) {
    if (g_paint.active && g_paint.hwnd == hwnd) {
        SelectObject(g_paint.mem_dc, g_paint.old_bmp);
        DeleteObject(g_paint.bmp);
        DeleteDC(g_paint.mem_dc);
        ReleaseDC(g_paint.hwnd, g_paint.win_dc);
        memset(&g_paint, 0, sizeof(g_paint));
    }
    for (int i = 0; i < W32_MAX_FULLSCREEN; i++) {
        if (g_fs[i].active && g_fs[i].hwnd == hwnd) {
            memset(&g_fs[i], 0, sizeof(g_fs[i]));
        }
    }
    for (int i = 0; i < W32_MAX_MIN_SIZES; i++) {
        if (g_min_sizes[i].active && g_min_sizes[i].hwnd == hwnd) {
            memset(&g_min_sizes[i], 0, sizeof(g_min_sizes[i]));
        }
        if (g_suppress_erase[i] == hwnd) {
            g_suppress_erase[i] = NULL;
        }
    }
    w32_webview_on_parent_destroy(hwnd);
}

uint8_t w32_set_borderless_fullscreen(uint64_t hwnd_u64, uint8_t enable) {
    HWND hwnd = (HWND)(uintptr_t)hwnd_u64;
    if (!hwnd) return 0;

    int idx = -1;
    int free_idx = -1;
    for (int i = 0; i < W32_MAX_FULLSCREEN; i++) {
        if (g_fs[i].active && g_fs[i].hwnd == hwnd) {
            idx = i;
            break;
        }
        if (!g_fs[i].active && free_idx < 0) {
            free_idx = i;
        }
    }

    if (enable) {
        if (idx >= 0) return 1;
        if (free_idx < 0) return 0;

        w32_fullscreen_state st;
        memset(&st, 0, sizeof(st));
        st.saved_style = (DWORD)GetWindowLongPtrW(hwnd, GWL_STYLE);
        st.saved_placement.length = sizeof(WINDOWPLACEMENT);
        GetWindowPlacement(hwnd, &st.saved_placement);

        MONITORINFO mi;
        memset(&mi, 0, sizeof(mi));
        mi.cbSize = sizeof(mi);
        HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY);
        if (!GetMonitorInfoW(mon, &mi)) return 0;

        SetWindowLongPtrW(hwnd, GWL_STYLE, (LONG_PTR)(st.saved_style & ~WS_OVERLAPPEDWINDOW));
        SetWindowPos(
            hwnd, HWND_TOP,
            mi.rcMonitor.left, mi.rcMonitor.top,
            mi.rcMonitor.right - mi.rcMonitor.left,
            mi.rcMonitor.bottom - mi.rcMonitor.top,
            SWP_NOOWNERZORDER | SWP_FRAMECHANGED
        );
        st.hwnd = hwnd;
        st.active = 1;
        g_fs[free_idx] = st;
        return 1;
    } else {
        if (idx < 0) return 1;
        SetWindowLongPtrW(hwnd, GWL_STYLE, (LONG_PTR)g_fs[idx].saved_style);
        SetWindowPlacement(hwnd, &g_fs[idx].saved_placement);
        SetWindowPos(
            hwnd, NULL, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED
        );
        memset(&g_fs[idx], 0, sizeof(g_fs[idx]));
        return 1;
    }
}

uint8_t w32_enable_raw_mouse(uint64_t hwnd_u64, uint8_t enable) {
    RAWINPUTDEVICE rid;
    rid.usUsagePage = 0x01; /* HID_USAGE_PAGE_GENERIC */
    rid.usUsage = 0x02;     /* HID_USAGE_GENERIC_MOUSE */
    rid.dwFlags = enable ? 0 : RIDEV_REMOVE;
    rid.hwndTarget = enable ? (HWND)(uintptr_t)hwnd_u64 : NULL;
    return RegisterRawInputDevices(&rid, 1, sizeof(rid)) ? 1 : 0;
}
