#define COBJMACROS
#include <stdlib.h>
#include <string.h>

#include "win32_internal.h"
#include <commdlg.h>
#include <shobjidl.h>

/* ---- Filter string conversion ("Label|*.ext|All|*.*" -> double-NUL) ------ */

static wchar_t *w32_build_filter(w32_bytes filter) {
    if (!filter.data || filter.len <= 0) return NULL;
    int n = MultiByteToWideChar(CP_UTF8, 0, (const char *)filter.data, (int)filter.len, NULL, 0);
    if (n <= 0) return NULL;
    /* Reserve two trailing NULs so Win32 always sees the double-NUL terminator. */
    wchar_t *buf = (wchar_t *)malloc((size_t)(n + 2) * sizeof(wchar_t));
    if (!buf) return NULL;
    MultiByteToWideChar(CP_UTF8, 0, (const char *)filter.data, (int)filter.len, buf, n);
    for (int i = 0; i < n; i++) {
        if (buf[i] == L'|') buf[i] = L'\0';
    }
    buf[n] = L'\0';
    buf[n + 1] = L'\0';
    return buf;
}

/* ---- Open / Save File Dialogs (comdlg32) --------------------------------- */

#define W32_PATH_BUF_CAP 4096

uint8_t w32_open_file_dialog(
    uint64_t owner,
    w32_bytes title,
    w32_bytes filter,
    w32_bytes default_ext,
    gs_rref out_path
) {
    wchar_t *path_buf = (wchar_t *)calloc(W32_PATH_BUF_CAP, sizeof(wchar_t));
    if (!path_buf) return 0;

    w32_wide_buf wtitle, wext;
    w32_wide_init(&wtitle, title);
    w32_wide_init(&wext, default_ext);
    wchar_t *wfilter = w32_build_filter(filter);

    OPENFILENAMEW ofn;
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = (HWND)(uintptr_t)owner;
    ofn.lpstrFilter = wfilter;
    ofn.lpstrFile = path_buf;
    ofn.nMaxFile = W32_PATH_BUF_CAP;
    ofn.lpstrTitle = wtitle.len > 0 ? wtitle.ptr : NULL;
    ofn.lpstrDefExt = wext.len > 0 ? wext.ptr : NULL;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    BOOL ok = GetOpenFileNameW(&ofn);
    uint8_t result = 0;
    if (ok) {
        result = w32_append_wide_as_utf8(path_buf, -1, out_path);
    }

    free(wfilter);
    w32_wide_free(&wext);
    w32_wide_free(&wtitle);
    free(path_buf);
    return result;
}

uint8_t w32_save_file_dialog(
    uint64_t owner,
    w32_bytes title,
    w32_bytes filter,
    w32_bytes default_ext,
    w32_bytes initial_name,
    gs_rref out_path
) {
    wchar_t *path_buf = (wchar_t *)calloc(W32_PATH_BUF_CAP, sizeof(wchar_t));
    if (!path_buf) return 0;

    w32_wide_buf wtitle, wext, winit;
    w32_wide_init(&wtitle, title);
    w32_wide_init(&wext, default_ext);
    w32_wide_init(&winit, initial_name);
    if (winit.len > 0) {
        size_t copy_n = (size_t)winit.len;
        if (copy_n >= W32_PATH_BUF_CAP) copy_n = W32_PATH_BUF_CAP - 1;
        memcpy(path_buf, winit.ptr, copy_n * sizeof(wchar_t));
        path_buf[copy_n] = L'\0';
    }
    wchar_t *wfilter = w32_build_filter(filter);

    OPENFILENAMEW ofn;
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = (HWND)(uintptr_t)owner;
    ofn.lpstrFilter = wfilter;
    ofn.lpstrFile = path_buf;
    ofn.nMaxFile = W32_PATH_BUF_CAP;
    ofn.lpstrTitle = wtitle.len > 0 ? wtitle.ptr : NULL;
    ofn.lpstrDefExt = wext.len > 0 ? wext.ptr : NULL;
    ofn.Flags = OFN_EXPLORER | OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    BOOL ok = GetSaveFileNameW(&ofn);
    uint8_t result = 0;
    if (ok) {
        result = w32_append_wide_as_utf8(path_buf, -1, out_path);
    }

    free(wfilter);
    w32_wide_free(&winit);
    w32_wide_free(&wext);
    w32_wide_free(&wtitle);
    free(path_buf);
    return result;
}

/* ---- Modern Folder Picker (IFileOpenDialog + FOS_PICKFOLDERS) ------------ */

uint8_t w32_browse_folder_dialog(uint64_t owner, w32_bytes title, gs_rref out_path) {
    HRESULT hr_co = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    int did_co_init = SUCCEEDED(hr_co);
    if (FAILED(hr_co) && hr_co != RPC_E_CHANGED_MODE) {
        return 0;
    }

    IFileOpenDialog *dlg = NULL;
    HRESULT hr = CoCreateInstance(
        &CLSID_FileOpenDialog,
        NULL,
        CLSCTX_INPROC_SERVER,
        &IID_IFileOpenDialog,
        (void **)&dlg
    );
    if (FAILED(hr) || !dlg) {
        if (did_co_init) CoUninitialize();
        return 0;
    }

    DWORD opts = 0;
    if (SUCCEEDED(IFileOpenDialog_GetOptions(dlg, &opts))) {
        IFileOpenDialog_SetOptions(dlg, opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_NOCHANGEDIR);
    }

    w32_wide_buf wtitle;
    w32_wide_init(&wtitle, title);
    if (wtitle.len > 0) {
        IFileOpenDialog_SetTitle(dlg, wtitle.ptr);
    }

    uint8_t result = 0;
    if (SUCCEEDED(IFileOpenDialog_Show(dlg, (HWND)(uintptr_t)owner))) {
        IShellItem *item = NULL;
        if (SUCCEEDED(IFileOpenDialog_GetResult(dlg, &item)) && item) {
            PWSTR path = NULL;
            if (SUCCEEDED(IShellItem_GetDisplayName(item, SIGDN_FILESYSPATH, &path)) && path) {
                result = w32_append_wide_as_utf8(path, -1, out_path);
                CoTaskMemFree(path);
            }
            IShellItem_Release(item);
        }
    }

    w32_wide_free(&wtitle);
    IFileOpenDialog_Release(dlg);
    if (did_co_init) CoUninitialize();
    return result;
}

/* ---- Color Picker Dialog (comdlg32) -------------------------------------- */

static COLORREF g_custom_colors[16] = {
    RGB(24, 28, 36),   RGB(59, 130, 246), RGB(16, 185, 129), RGB(245, 158, 11),
    RGB(239, 68, 68),  RGB(168, 85, 247), RGB(236, 72, 153), RGB(20, 184, 166),
    RGB(255, 255, 255),RGB(226, 232, 240),RGB(148, 163, 184),RGB(71, 85, 105),
    RGB(30, 41, 59),   RGB(15, 23, 42),   RGB(0, 0, 0),      RGB(250, 204, 21),
};

uint8_t w32_choose_color_dialog(uint64_t owner, uint32_t initial_color, uint32_t *out_color) {
    CHOOSECOLORW cc;
    memset(&cc, 0, sizeof(cc));
    cc.lStructSize = sizeof(cc);
    cc.hwndOwner = (HWND)(uintptr_t)owner;
    cc.rgbResult = (COLORREF)initial_color;
    cc.lpCustColors = g_custom_colors;
    cc.Flags = CC_FULLOPEN | CC_RGBINIT;

    if (!ChooseColorW(&cc)) return 0;
    if (out_color) *out_color = (uint32_t)cc.rgbResult;
    return 1;
}

/* ---- Modal Text Input Dialog (in-memory DLGTEMPLATE, no .rc needed) ------ */

#define W32_IDC_PROMPT 1001
#define W32_IDC_EDIT   1002

typedef struct {
    const wchar_t *prompt;
    const wchar_t *initial_text;
    wchar_t *result_buf;
    int result_len;
    int accepted;
} w32_input_ctx;

static INT_PTR CALLBACK w32_input_dlgproc(HWND hdlg, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_INITDIALOG: {
            w32_input_ctx *ctx = (w32_input_ctx *)lp;
            SetWindowLongPtrW(hdlg, GWLP_USERDATA, (LONG_PTR)ctx);
            if (ctx) {
                if (ctx->prompt) SetDlgItemTextW(hdlg, W32_IDC_PROMPT, ctx->prompt);
                if (ctx->initial_text) SetDlgItemTextW(hdlg, W32_IDC_EDIT, ctx->initial_text);
            }
            HWND edit = GetDlgItem(hdlg, W32_IDC_EDIT);
            if (edit) {
                SetFocus(edit);
                SendMessageW(edit, EM_SETSEL, 0, -1);
            }
            /* Center on parent if present. */
            HWND parent = GetParent(hdlg);
            if (parent) {
                RECT pr, dr;
                if (GetWindowRect(parent, &pr) && GetWindowRect(hdlg, &dr)) {
                    int dw = dr.right - dr.left;
                    int dh = dr.bottom - dr.top;
                    int x = pr.left + ((pr.right - pr.left) - dw) / 2;
                    int y = pr.top + ((pr.bottom - pr.top) - dh) / 2;
                    SetWindowPos(hdlg, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
                }
            }
            return FALSE; /* we set focus explicitly */
        }

        case WM_COMMAND: {
            WORD id = LOWORD(wp);
            if (id == IDOK) {
                w32_input_ctx *ctx = (w32_input_ctx *)(uintptr_t)GetWindowLongPtrW(hdlg, GWLP_USERDATA);
                if (ctx) {
                    HWND edit = GetDlgItem(hdlg, W32_IDC_EDIT);
                    int len = edit ? GetWindowTextLengthW(edit) : 0;
                    ctx->result_buf = (wchar_t *)malloc((size_t)(len + 1) * sizeof(wchar_t));
                    if (ctx->result_buf) {
                        ctx->result_len = edit ? GetWindowTextW(edit, ctx->result_buf, len + 1) : 0;
                        ctx->result_buf[ctx->result_len] = L'\0';
                    }
                    ctx->accepted = 1;
                }
                EndDialog(hdlg, IDOK);
                return TRUE;
            }
            if (id == IDCANCEL) {
                EndDialog(hdlg, IDCANCEL);
                return TRUE;
            }
            break;
        }
    }
    return FALSE;
}

/* Helper to emit aligned WORDs/DWORDs into an in-memory DLGTEMPLATE buffer. */
typedef struct {
    uint8_t *data;
    size_t len;
    size_t cap;
} w32_dlg_builder;

static void dlg_align4(w32_dlg_builder *b) {
    while (b->len & 3) {
        if (b->len < b->cap) b->data[b->len] = 0;
        b->len++;
    }
}

static void dlg_write(w32_dlg_builder *b, const void *src, size_t n) {
    if (b->len + n <= b->cap) {
        memcpy(b->data + b->len, src, n);
    }
    b->len += n;
}

static void dlg_u16(w32_dlg_builder *b, uint16_t v) { dlg_write(b, &v, 2); }
static void dlg_wstr(w32_dlg_builder *b, const wchar_t *s) {
    size_t n = (wcslen(s) + 1) * sizeof(wchar_t);
    dlg_write(b, s, n);
}

static void dlg_item(
    w32_dlg_builder *b,
    DWORD style,
    short x, short y, short cx, short cy,
    WORD id,
    WORD atom_class,
    const wchar_t *text
) {
    dlg_align4(b);
    DLGITEMTEMPLATE it;
    memset(&it, 0, sizeof(it));
    it.style = style | WS_CHILD | WS_VISIBLE;
    it.x = x; it.y = y; it.cx = cx; it.cy = cy;
    it.id = id;
    dlg_write(b, &it, sizeof(it));
    dlg_u16(b, 0xFFFF);
    dlg_u16(b, atom_class);
    dlg_wstr(b, text ? text : L"");
    dlg_u16(b, 0); /* no creation data */
}

uint8_t w32_input_dialog(
    uint64_t owner,
    w32_bytes title,
    w32_bytes prompt,
    w32_bytes initial_text,
    gs_rref out_text
) {
    w32_wide_buf wtitle, wprompt, winit;
    w32_wide_init(&wtitle, title);
    w32_wide_init(&wprompt, prompt);
    w32_wide_init(&winit, initial_text);

    uint8_t raw[2048];
    memset(raw, 0, sizeof(raw));
    w32_dlg_builder b = { raw, 0, sizeof(raw) };

    DLGTEMPLATE dt;
    memset(&dt, 0, sizeof(dt));
    dt.style = DS_SETFONT | DS_MODALFRAME | WS_POPUP | WS_CAPTION | WS_SYSMENU;
    dt.cdit = 4; /* static prompt, edit box, OK button, Cancel button */
    dt.x = 0; dt.y = 0; dt.cx = 220; dt.cy = 78;
    dlg_write(&b, &dt, sizeof(dt));
    dlg_u16(&b, 0); /* no menu */
    dlg_u16(&b, 0); /* default dialog class */
    dlg_wstr(&b, wtitle.len > 0 ? wtitle.ptr : L"Input");
    dlg_u16(&b, 9); /* font size */
    dlg_wstr(&b, L"Segoe UI");

    /* 0x0082 = Static, 0x0081 = Edit, 0x0080 = Button */
    dlg_item(&b, SS_LEFT, 10, 8, 200, 14, W32_IDC_PROMPT, 0x0082, L"");
    dlg_item(&b, ES_LEFT | ES_AUTOHSCROLL | WS_BORDER | WS_TABSTOP, 10, 26, 200, 15, W32_IDC_EDIT, 0x0081, L"");
    dlg_item(&b, BS_DEFPUSHBUTTON | WS_TABSTOP, 104, 52, 50, 16, IDOK, 0x0080, L"OK");
    dlg_item(&b, BS_PUSHBUTTON | WS_TABSTOP, 160, 52, 50, 16, IDCANCEL, 0x0080, L"Cancel");

    w32_input_ctx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.prompt = wprompt.ptr;
    ctx.initial_text = winit.ptr;

    INT_PTR ret = DialogBoxIndirectParamW(
        GetModuleHandleW(NULL),
        (LPCDLGTEMPLATEW)raw,
        (HWND)(uintptr_t)owner,
        w32_input_dlgproc,
        (LPARAM)&ctx
    );

    uint8_t ok = 0;
    if (ret == IDOK && ctx.accepted && ctx.result_buf) {
        ok = w32_append_wide_as_utf8(ctx.result_buf, ctx.result_len, out_text);
    }

    free(ctx.result_buf);
    w32_wide_free(&winit);
    w32_wide_free(&wprompt);
    w32_wide_free(&wtitle);
    return ok;
}
