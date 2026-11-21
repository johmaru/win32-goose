#include <stdlib.h>
#include <string.h>

#include "win32_internal.h"
#include <commctrl.h>
#include <shellapi.h>

/* ---- Standard Win32 Child Controls --------------------------------------- */

uint8_t w32_init_common_controls(void) {
    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(icc);
    /* ICC_WIN95_CLASSES works under both ComCtl32 v5 (no v6 manifest) and v6. */
    icc.dwICC = ICC_WIN95_CLASSES;
    return InitCommonControlsEx(&icc) ? 1 : 0;
}

uint64_t w32_create_control(
    uint64_t parent_u64,
    uint32_t id,
    w32_bytes class_name,
    w32_bytes text,
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height,
    uint32_t style,
    uint32_t ex_style
) {
    w32_wide_buf wcls, wtxt;
    if (!w32_wide_init(&wcls, class_name) || wcls.len == 0) {
        w32_wide_free(&wcls);
        return 0;
    }
    if (!w32_wide_init(&wtxt, text)) {
        w32_wide_free(&wcls);
        return 0;
    }

    HWND hwnd = CreateWindowExW(
        (DWORD)ex_style,
        wcls.ptr,
        wtxt.ptr,
        (DWORD)(style | WS_CHILD | WS_VISIBLE),
        (int)x, (int)y, (int)width, (int)height,
        (HWND)(uintptr_t)parent_u64,
        (HMENU)(uintptr_t)id,
        GetModuleHandleW(NULL),
        NULL
    );

    w32_wide_free(&wtxt);
    w32_wide_free(&wcls);

    if (hwnd) {
        SendMessageW(hwnd, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
    }
    return (uint64_t)(uintptr_t)hwnd;
}

int32_t w32_listbox_add(uint64_t hwnd_u64, w32_bytes text) {
    if (!hwnd_u64) return -1;
    w32_wide_buf wb;
    if (!w32_wide_init(&wb, text)) return -1;
    LRESULT idx = SendMessageW((HWND)(uintptr_t)hwnd_u64, LB_ADDSTRING, 0, (LPARAM)wb.ptr);
    w32_wide_free(&wb);
    return (int32_t)idx;
}

uint8_t w32_listbox_item_text(uint64_t hwnd_u64, int32_t index, gs_rref out) {
    HWND hwnd = (HWND)(uintptr_t)hwnd_u64;
    if (!hwnd || index < 0) return 0;
    LRESULT len = SendMessageW(hwnd, LB_GETTEXTLEN, (WPARAM)index, 0);
    if (len == LB_ERR || len < 0) return 0;
    if (len == 0) return 1;
    wchar_t *buf = (wchar_t *)malloc((size_t)(len + 1) * sizeof(wchar_t));
    if (!buf) return 0;
    LRESULT got = SendMessageW(hwnd, LB_GETTEXT, (WPARAM)index, (LPARAM)buf);
    uint8_t ok = (got != LB_ERR && got >= 0) ? w32_append_wide_as_utf8(buf, (int)got, out) : 0;
    free(buf);
    return ok;
}

int32_t w32_combobox_add(uint64_t hwnd_u64, w32_bytes text) {
    if (!hwnd_u64) return -1;
    w32_wide_buf wb;
    if (!w32_wide_init(&wb, text)) return -1;
    LRESULT idx = SendMessageW((HWND)(uintptr_t)hwnd_u64, CB_ADDSTRING, 0, (LPARAM)wb.ptr);
    w32_wide_free(&wb);
    return (int32_t)idx;
}

uint8_t w32_combobox_item_text(uint64_t hwnd_u64, int32_t index, gs_rref out) {
    HWND hwnd = (HWND)(uintptr_t)hwnd_u64;
    if (!hwnd || index < 0) return 0;
    LRESULT len = SendMessageW(hwnd, CB_GETLBTEXTLEN, (WPARAM)index, 0);
    if (len == CB_ERR || len < 0) return 0;
    if (len == 0) return 1;
    wchar_t *buf = (wchar_t *)malloc((size_t)(len + 1) * sizeof(wchar_t));
    if (!buf) return 0;
    LRESULT got = SendMessageW(hwnd, CB_GETLBTEXT, (WPARAM)index, (LPARAM)buf);
    uint8_t ok = (got != CB_ERR && got >= 0) ? w32_append_wide_as_utf8(buf, (int)got, out) : 0;
    free(buf);
    return ok;
}

/* ---- Drag & Drop (WM_DROPFILES) ------------------------------------------ */

static wchar_t **g_drop_paths = NULL;
static int32_t g_drop_count = 0;

static void clear_drops(void) {
    for (int32_t i = 0; i < g_drop_count; i++) {
        free(g_drop_paths[i]);
    }
    free(g_drop_paths);
    g_drop_paths = NULL;
    g_drop_count = 0;
}

uint32_t w32_capture_hdrop(void *hdrop_ptr) {
    clear_drops();
    HDROP hdrop = (HDROP)hdrop_ptr;
    if (!hdrop) return 0;

    UINT n = DragQueryFileW(hdrop, 0xFFFFFFFFu, NULL, 0);
    if (n > 0) {
        g_drop_paths = (wchar_t **)calloc(n, sizeof(wchar_t *));
        if (g_drop_paths) {
            for (UINT i = 0; i < n; i++) {
                UINT len = DragQueryFileW(hdrop, i, NULL, 0);
                wchar_t *buf = (wchar_t *)malloc((size_t)(len + 1) * sizeof(wchar_t));
                if (buf) {
                    DragQueryFileW(hdrop, i, buf, len + 1);
                    g_drop_paths[g_drop_count++] = buf;
                }
            }
        }
    }
    DragFinish(hdrop);
    return (uint32_t)g_drop_count;
}

void w32_accept_drop_files(uint64_t hwnd_u64, uint8_t enable) {
    if (!hwnd_u64) return;
    DragAcceptFiles((HWND)(uintptr_t)hwnd_u64, enable ? TRUE : FALSE);
}

int32_t w32_dropped_file_count(void) {
    return g_drop_count;
}

uint8_t w32_dropped_file_path(int32_t index, gs_rref out) {
    if (index < 0 || index >= g_drop_count || !g_drop_paths[index]) return 0;
    return w32_append_wide_as_utf8(g_drop_paths[index], -1, out);
}

/* ---- Tab Control & Status Bar -------------------------------------------- */

int32_t w32_tab_add(uint64_t hwnd_u64, int32_t index, w32_bytes text) {
    HWND hwnd = (HWND)(uintptr_t)hwnd_u64;
    if (!hwnd) return -1;
    w32_wide_buf wb;
    if (!w32_wide_init(&wb, text)) return -1;
    TCITEMW item;
    memset(&item, 0, sizeof(item));
    item.mask = TCIF_TEXT;
    item.pszText = wb.ptr;
    LRESULT idx = SendMessageW(hwnd, TCM_INSERTITEMW, (WPARAM)index, (LPARAM)&item);
    w32_wide_free(&wb);
    return (int32_t)idx;
}

uint8_t w32_statusbar_set_text(uint64_t hwnd_u64, int32_t part, w32_bytes text) {
    HWND hwnd = (HWND)(uintptr_t)hwnd_u64;
    if (!hwnd || part < 0 || part > 255) return 0;
    w32_wide_buf wb;
    if (!w32_wide_init(&wb, text)) return 0;
    LRESULT ok = SendMessageW(hwnd, SB_SETTEXTW, (WPARAM)part, (LPARAM)wb.ptr);
    w32_wide_free(&wb);
    return ok ? 1 : 0;
}
