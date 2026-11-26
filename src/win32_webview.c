#include <objbase.h>
#include <shellapi.h>
#include <stdlib.h>
#include <string.h>

#include "win32_internal.h"

/* ---- Event kinds & policies (must match win32-goose/webview.goose) -------- */

#define W32_WV_EV_NONE            0u
#define W32_WV_EV_READY           1u
#define W32_WV_EV_INIT_FAILED     2u
#define W32_WV_EV_NAV_STARTING    3u
#define W32_WV_EV_NAV_COMPLETED   4u
#define W32_WV_EV_SOURCE_CHANGED  5u
#define W32_WV_EV_TITLE_CHANGED   6u
#define W32_WV_EV_HISTORY_CHANGED 7u
#define W32_WV_EV_WEB_MESSAGE     8u
#define W32_WV_EV_SCRIPT_RESULT   9u
#define W32_WV_EV_NEW_WINDOW      10u
#define W32_WV_EV_FOCUS_CHANGED   11u

#define W32_WV_NEWWIN_BLOCK         0
#define W32_WV_NEWWIN_NAVIGATE_SELF 1
#define W32_WV_NEWWIN_OPEN_EXTERNAL 2
#define W32_WV_NEWWIN_ALLOW_POPUP   3

#define W32_WV_CMD_NAVIGATE       1
#define W32_WV_CMD_SET_HTML       2
#define W32_WV_CMD_EVAL_JS        3
#define W32_WV_CMD_ADD_INIT       4
#define W32_WV_CMD_POST_JSON      5
#define W32_WV_CMD_POST_STRING    6
#define W32_WV_CMD_SET_VHOST      7
#define W32_WV_CMD_CLEAR_VHOST    8
#define W32_WV_CMD_OPEN_DEVTOOLS  9

#define W32_WV_MAX_INSTANCES 32
#define W32_WV_MAX_ENVS      32
#define W32_WV_EV_CAP        512

/* ---- Verified COM IIDs (Windows WebView2 SDK 1.0.4078.44) ---------------- */

static const GUID W32_IID_IUnknown =
    { 0x00000000, 0x0000, 0x0000, { 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };
static const GUID W32_IID_ICoreWebView2Controller2 =
    { 0xc979903e, 0xd4ca, 0x4228, { 0x92, 0xeb, 0x47, 0xee, 0x3f, 0xa9, 0x6e, 0xab } };
static const GUID W32_IID_ICoreWebView2_3 =
    { 0xa0d6df20, 0x3b92, 0x416d, { 0xaa, 0x0c, 0x43, 0x7a, 0x9c, 0x72, 0x78, 0x57 } };
static const GUID W32_IID_ICoreWebView2_8 =
    { 0xe9632730, 0x6e1e, 0x43ab, { 0xb7, 0xb8, 0x7b, 0x2c, 0x9e, 0x62, 0xe0, 0x94 } };
static const GUID W32_IID_ICoreWebView2_13 =
    { 0xf75f09a8, 0x667e, 0x4983, { 0x88, 0xd6, 0xc8, 0x77, 0x3f, 0x31, 0x5e, 0x84 } };
static const GUID W32_IID_ICoreWebView2Settings2 =
    { 0xee9a0f68, 0xf46c, 0x4e32, { 0xac, 0x23, 0xef, 0x8c, 0xac, 0x22, 0x4d, 0x2a } };
static const GUID W32_IID_ICoreWebView2Settings3 =
    { 0xfdb5ab74, 0xaf33, 0x4854, { 0x84, 0xf0, 0x0a, 0x63, 0x1d, 0xeb, 0x5e, 0xba } };
static const GUID W32_IID_ICoreWebView2EnvironmentOptions =
    { 0x2fde08a8, 0x1e9a, 0x4766, { 0x8c, 0x05, 0x95, 0xa9, 0xce, 0xb9, 0xd1, 0xc5 } };
static const GUID W32_IID_EnvCompleted =
    { 0x4e8a3389, 0xc9d8, 0x4bd2, { 0xb6, 0xb5, 0x12, 0x4f, 0xee, 0x6c, 0xc1, 0x4d } };
static const GUID W32_IID_CtrlCompleted =
    { 0x6c4819f3, 0xc9b7, 0x4260, { 0x81, 0x27, 0xc9, 0xf5, 0xbd, 0xe7, 0xf6, 0x8c } };
static const GUID W32_IID_NavStarting =
    { 0x9adbe429, 0xf36d, 0x432b, { 0x9d, 0xdc, 0xf8, 0x88, 0x1f, 0xbd, 0x76, 0xe3 } };
static const GUID W32_IID_NavCompleted =
    { 0xd33a35bf, 0x1c49, 0x4f98, { 0x93, 0xab, 0x00, 0x6e, 0x05, 0x33, 0xfe, 0x1c } };
static const GUID W32_IID_SourceChanged =
    { 0x3c067f9f, 0x5388, 0x4772, { 0x8b, 0x48, 0x79, 0xf7, 0xef, 0x1a, 0xb3, 0x7c } };
static const GUID W32_IID_HistoryChanged =
    { 0xc79a420c, 0xefd9, 0x4058, { 0x92, 0x95, 0x3e, 0x8b, 0x4b, 0xca, 0xb6, 0x45 } };
static const GUID W32_IID_TitleChanged =
    { 0xf5f2b923, 0x953e, 0x4042, { 0x9f, 0x95, 0xf3, 0xa1, 0x18, 0xe1, 0xaf, 0xd4 } };
static const GUID W32_IID_WebMessage =
    { 0x57213f19, 0x00e6, 0x49fa, { 0x8e, 0x07, 0x89, 0x8e, 0xa0, 0x1e, 0xcb, 0xd2 } };
static const GUID W32_IID_ExecScript =
    { 0x49511172, 0xcc67, 0x4bca, { 0x99, 0x23, 0x13, 0x71, 0x12, 0xf4, 0xc4, 0xcc } };
static const GUID W32_IID_AddInitScript =
    { 0xb99369f3, 0x9b11, 0x47b5, { 0xbc, 0x6f, 0x8e, 0x78, 0x95, 0xfc, 0xea, 0x17 } };
static const GUID W32_IID_NewWindow =
    { 0xd4c185fe, 0xc81c, 0x4989, { 0x97, 0xaf, 0x2d, 0x3f, 0xa7, 0xab, 0x56, 0x51 } };
static const GUID W32_IID_FocusChanged =
    { 0x05ea24bd, 0x6452, 0x4926, { 0x90, 0x14, 0x4b, 0x82, 0xb4, 0x98, 0x13, 0x5d } };

/* ---- COM vtable helpers -------------------------------------------------- */

static inline void **wv_vtbl(void *obj) {
    return *(void ***)obj;
}

static inline HRESULT wv_qi(void *obj, const GUID *iid, void **out) {
    typedef HRESULT (STDMETHODCALLTYPE *pfn)(void *, const GUID *, void **);
    return ((pfn)wv_vtbl(obj)[0])(obj, iid, out);
}

static inline ULONG wv_release(void *obj) {
    if (!obj) return 0;
    typedef ULONG (STDMETHODCALLTYPE *pfn)(void *);
    return ((pfn)wv_vtbl(obj)[2])(obj);
}

static int wv_guid_eq(const GUID *a, const GUID *b) {
    return memcmp(a, b, sizeof(GUID)) == 0;
}

/* ---- String & Path Helpers ----------------------------------------------- */

static wchar_t *wv_dup_wide(const wchar_t *s) {
    if (!s) return NULL;
    size_t len = wcslen(s);
    wchar_t *out = (wchar_t *)malloc((len + 1) * sizeof(wchar_t));
    if (!out) return NULL;
    memcpy(out, s, (len + 1) * sizeof(wchar_t));
    return out;
}

static wchar_t *wv_utf8_to_wide_alloc(w32_bytes s, int allow_empty) {
    if (!s.data || s.len <= 0) {
        if (!allow_empty) return NULL;
        wchar_t *empty = (wchar_t *)malloc(sizeof(wchar_t));
        if (empty) empty[0] = L'\0';
        return empty;
    }
    int n = MultiByteToWideChar(CP_UTF8, 0, (const char *)s.data, (int)s.len, NULL, 0);
    if (n <= 0) return NULL;
    wchar_t *buf = (wchar_t *)malloc((size_t)(n + 1) * sizeof(wchar_t));
    if (!buf) return NULL;
    MultiByteToWideChar(CP_UTF8, 0, (const char *)s.data, (int)s.len, buf, n);
    buf[n] = L'\0';
    return buf;
}

static char *wv_wide_to_utf8_alloc(const wchar_t *ws) {
    if (!ws || !ws[0]) {
        char *empty = (char *)malloc(1);
        if (empty) empty[0] = '\0';
        return empty;
    }
    int n = WideCharToMultiByte(CP_UTF8, 0, ws, -1, NULL, 0, NULL, NULL);
    if (n <= 0) return NULL;
    char *buf = (char *)malloc((size_t)n);
    if (!buf) return NULL;
    int got = WideCharToMultiByte(CP_UTF8, 0, ws, -1, buf, n, NULL, NULL);
    if (got <= 0) {
        free(buf);
        return NULL;
    }
    return buf;
}

static char *wv_dup_cstr(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

static LPWSTR wv_cotask_dup(const wchar_t *ws) {
    if (!ws) return NULL;
    size_t len = wcslen(ws);
    size_t bytes = (len + 1) * sizeof(wchar_t);
    LPWSTR out = (LPWSTR)CoTaskMemAlloc(bytes);
    if (out) memcpy(out, ws, bytes);
    return out;
}

static int wv_wide_eq(const wchar_t *a, const wchar_t *b) {
    if (!a && !b) return 1;
    if (!a) return b[0] == L'\0';
    if (!b) return a[0] == L'\0';
    return wcscmp(a, b) == 0;
}

static int wv_file_exists_w(const wchar_t *path) {
    if (!path || !path[0]) return 0;
    DWORD attr = GetFileAttributesW(path);
    return (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) ? 1 : 0;
}

static void wv_create_dirs_w(const wchar_t *path) {
    if (!path || !path[0]) return;
    wchar_t *tmp = wv_dup_wide(path);
    if (!tmp) return;
    size_t len = wcslen(tmp);
    for (size_t i = 0; i < len; i++) {
        if (tmp[i] == L'/' || tmp[i] == L'\\') {
            if (i >= 2 && tmp[i - 1] != L':') {
                wchar_t saved = tmp[i];
                tmp[i] = L'\0';
                CreateDirectoryW(tmp, NULL);
                tmp[i] = saved;
            }
        }
    }
    CreateDirectoryW(tmp, NULL);
    free(tmp);
}

static wchar_t *wv_default_user_data_dir(void) {
    wchar_t base[MAX_PATH * 2];
    DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", base, MAX_PATH * 2);
    if (n == 0 || n >= MAX_PATH * 2) {
        n = GetTempPathW(MAX_PATH * 2, base);
        if (n == 0 || n >= MAX_PATH * 2) return wv_dup_wide(L"goose_webview2");
    }
    size_t blen = wcslen(base);
    while (blen > 0 && (base[blen - 1] == L'\\' || base[blen - 1] == L'/')) {
        base[--blen] = L'\0';
    }

    wchar_t exe[MAX_PATH * 2];
    const wchar_t *stem = L"goose_app";
    wchar_t stem_buf[MAX_PATH];
    DWORD en = GetModuleFileNameW(NULL, exe, MAX_PATH * 2);
    if (en > 0 && en < MAX_PATH * 2) {
        const wchar_t *last_sep = exe;
        for (DWORD i = 0; i < en; i++) {
            if (exe[i] == L'\\' || exe[i] == L'/') last_sep = &exe[i + 1];
        }
        size_t slen = wcslen(last_sep);
        if (slen > 4 && _wcsicmp(last_sep + slen - 4, L".exe") == 0) slen -= 4;
        if (slen > 0 && slen < MAX_PATH) {
            memcpy(stem_buf, last_sep, slen * sizeof(wchar_t));
            stem_buf[slen] = L'\0';
            stem = stem_buf;
        }
    }

    size_t total = blen + 1 + wcslen(stem) + 10 + 1;
    wchar_t *out = (wchar_t *)malloc(total * sizeof(wchar_t));
    if (!out) return NULL;
    wcscpy(out, base);
    wcscat(out, L"\\");
    wcscat(out, stem);
    wcscat(out, L".WebView2");
    return out;
}

/* ---- Loader Discovery (WebView2Loader.dll or EmbeddedBrowserWebView.dll) - */

#define WV_LOADER_NONE     0
#define WV_LOADER_PUBLIC   1
#define WV_LOADER_INTERNAL 2

typedef HRESULT (STDAPICALLTYPE *pfn_CreateCoreWebView2EnvWithOpts)(LPCWSTR, LPCWSTR, void *, void *);
typedef HRESULT (STDAPICALLTYPE *pfn_GetAvailBrowserVersion)(LPCWSTR, LPWSTR *);
typedef HRESULT (STDAPICALLTYPE *pfn_CreateWebViewEnvWithOptsInternal)(uint8_t, int32_t, LPCWSTR, void *, void *);

static struct {
    int probed;
    int available;
    int mode;
    int32_t runtime_type; /* 0 = Installed, 1 = Embedded */
    HMODULE mod;
    pfn_CreateCoreWebView2EnvWithOpts pub_create_env;
    pfn_GetAvailBrowserVersion pub_get_ver;
    pfn_CreateWebViewEnvWithOptsInternal int_create_env;
    wchar_t *browser_dir_w;
    wchar_t *target_ver_w;
    char *runtime_ver_utf8;
} g_wv_rt;

static int wv_try_reg_str(HKEY root, const wchar_t *subkey, const wchar_t *val_name, wchar_t *out, DWORD out_chars) {
    HKEY hk = NULL;
    if (RegOpenKeyExW(root, subkey, 0, KEY_READ, &hk) != ERROR_SUCCESS || !hk) {
        return 0;
    }
    DWORD type = 0;
    DWORD bytes = out_chars * (DWORD)sizeof(wchar_t);
    LONG rc = RegQueryValueExW(hk, val_name, NULL, &type, (LPBYTE)out, &bytes);
    RegCloseKey(hk);
    if (rc != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ) || bytes < sizeof(wchar_t)) {
        return 0;
    }
    out[out_chars - 1] = L'\0';
    return out[0] != L'\0' ? 1 : 0;
}

static int wv_try_load_embedded_dir(const wchar_t *base_dir, int32_t rt_type, const wchar_t *ver_hint) {
    if (!base_dir || !base_dir[0]) return 0;
    size_t blen = wcslen(base_dir);
    while (blen > 0 && (base_dir[blen - 1] == L'\\' || base_dir[blen - 1] == L'/')) blen--;
    if (blen == 0) return 0;

    static const wchar_t kSuffix[] = L"\\EBWebView\\x64\\EmbeddedBrowserWebView.dll";
    wchar_t *dll_path = (wchar_t *)malloc((blen + wcslen(kSuffix) + 1) * sizeof(wchar_t));
    if (!dll_path) return 0;
    memcpy(dll_path, base_dir, blen * sizeof(wchar_t));
    wcscpy(dll_path + blen, kSuffix);

    if (!wv_file_exists_w(dll_path)) {
        free(dll_path);
        return 0;
    }

    HMODULE hmod = LoadLibraryExW(dll_path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    free(dll_path);
    if (!hmod) return 0;

    pfn_CreateWebViewEnvWithOptsInternal fn =
        (pfn_CreateWebViewEnvWithOptsInternal)(uintptr_t)GetProcAddress(
            hmod, "CreateWebViewEnvironmentWithOptionsInternal"
        );
    if (!fn) {
        FreeLibrary(hmod);
        return 0;
    }

    g_wv_rt.mod = hmod;
    g_wv_rt.int_create_env = fn;
    g_wv_rt.mode = WV_LOADER_INTERNAL;
    g_wv_rt.runtime_type = rt_type;
    g_wv_rt.browser_dir_w = wv_dup_wide(base_dir);
    if (ver_hint && ver_hint[0]) {
        g_wv_rt.target_ver_w = wv_dup_wide(ver_hint);
        g_wv_rt.runtime_ver_utf8 = wv_wide_to_utf8_alloc(ver_hint);
    }
    return 1;
}

static int wv_probe_runtime(void) {
    if (g_wv_rt.probed) return g_wv_rt.available;
    g_wv_rt.probed = 1;

    /* 1. Optional WebView2Loader.dll if present alongside the app or on PATH. */
    HMODULE loader = LoadLibraryW(L"WebView2Loader.dll");
    if (loader) {
        pfn_CreateCoreWebView2EnvWithOpts p_create =
            (pfn_CreateCoreWebView2EnvWithOpts)(uintptr_t)GetProcAddress(
                loader, "CreateCoreWebView2EnvironmentWithOptions"
            );
        pfn_GetAvailBrowserVersion p_ver =
            (pfn_GetAvailBrowserVersion)(uintptr_t)GetProcAddress(
                loader, "GetAvailableCoreWebView2BrowserVersionString"
            );
        if (p_create && p_ver) {
            LPWSTR ver = NULL;
            if (SUCCEEDED(p_ver(NULL, &ver)) && ver && ver[0]) {
                g_wv_rt.mod = loader;
                g_wv_rt.pub_create_env = p_create;
                g_wv_rt.pub_get_ver = p_ver;
                g_wv_rt.mode = WV_LOADER_PUBLIC;
                g_wv_rt.target_ver_w = wv_dup_wide(ver);
                g_wv_rt.runtime_ver_utf8 = wv_wide_to_utf8_alloc(ver);
                CoTaskMemFree(ver);
                g_wv_rt.available = 1;
                return 1;
            }
            if (ver) CoTaskMemFree(ver);
        }
        FreeLibrary(loader);
    }

    /* 2. Fixed-version folder override via WEBVIEW2_BROWSER_EXECUTABLE_FOLDER. */
    wchar_t env_dir[MAX_PATH * 2];
    DWORD en = GetEnvironmentVariableW(L"WEBVIEW2_BROWSER_EXECUTABLE_FOLDER", env_dir, MAX_PATH * 2);
    if (en > 0 && en < MAX_PATH * 2) {
        if (wv_try_load_embedded_dir(env_dir, 1, L"")) {
            g_wv_rt.available = 1;
            return 1;
        }
    }

    /* 3. Zero-dependency discovery of installed Evergreen WebView2 / Edge channels via Registry. */
    static const wchar_t *const kGuids[] = {
        L"{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}", /* Microsoft Edge WebView2 Runtime */
        L"{2CD8A007-E189-409D-A2C8-9AF4EF3C72AA}", /* Edge Beta */
        L"{0D50BFEC-CD6A-4F9A-964C-C7416E3ACB10}", /* Edge Dev */
        L"{65C35B14-6C1D-4122-AC46-7148CC9D6497}", /* Edge Canary */
    };
    static const wchar_t *const kRoots[] = {
        L"SOFTWARE\\WOW6432Node\\Microsoft\\EdgeUpdate",
        L"SOFTWARE\\Microsoft\\EdgeUpdate",
    };
    HKEY hives[2] = { HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE };

    for (int g = 0; g < 4; g++) {
        for (int h = 0; h < 2; h++) {
            for (int r = 0; r < 2; r++) {
                wchar_t sub_state[256];
                wchar_t sub_client[256];
                wchar_t eb_path[MAX_PATH * 2];
                wchar_t pv[64];
                wchar_t loc[MAX_PATH * 2];
                pv[0] = L'\0';

                wcscpy(sub_client, kRoots[r]);
                wcscat(sub_client, L"\\Clients\\");
                wcscat(sub_client, kGuids[g]);
                wv_try_reg_str(hives[h], sub_client, L"pv", pv, 64);

                wcscpy(sub_state, kRoots[r]);
                wcscat(sub_state, L"\\ClientState\\");
                wcscat(sub_state, kGuids[g]);
                if (wv_try_reg_str(hives[h], sub_state, L"EBWebView", eb_path, MAX_PATH * 2)) {
                    if (wv_try_load_embedded_dir(eb_path, 0, pv)) {
                        g_wv_rt.available = 1;
                        return 1;
                    }
                }

                if (pv[0] && wv_try_reg_str(hives[h], sub_client, L"location", loc, MAX_PATH * 2)) {
                    wchar_t combined[MAX_PATH * 2];
                    wcscpy(combined, loc);
                    size_t ll = wcslen(combined);
                    if (ll > 0 && combined[ll - 1] != L'\\' && combined[ll - 1] != L'/') wcscat(combined, L"\\");
                    wcscat(combined, pv);
                    if (wv_try_load_embedded_dir(combined, 0, pv)) {
                        g_wv_rt.available = 1;
                        return 1;
                    }
                }
            }
        }
    }

    return 0;
}

/* ---- State Structures ---------------------------------------------------- */

typedef struct wv_cmd_node {
    int kind;
    uint64_t req_id;
    int32_t int_arg;
    wchar_t *wstr1;
    wchar_t *wstr2;
    struct wv_cmd_node *next;
} wv_cmd_node;

typedef struct {
    int used;
    int ready;
    int failed;
    int32_t last_hr;
    wchar_t *user_data_dir;
    wchar_t *additional_args;
    void *env;
} wv_env_entry;

typedef struct {
    int used;
    uint32_t gen;
    HWND parent_hwnd;
    HWND host_hwnd;
    int env_idx;
    int ready;
    int failed;
    int32_t last_hr;

    void *controller;
    void *webview;
    void *settings;

    int32_t bounds_x, bounds_y, bounds_w, bounds_h;
    int32_t clip_x, clip_y, clip_w, clip_h;
    int has_clip;
    uint8_t desired_visible;

    uint8_t bg_r, bg_g, bg_b, bg_a;
    int32_t color_scheme;
    double zoom_factor;
    uint8_t devtools_enabled;
    uint8_t context_menu_enabled;
    uint8_t status_bar_enabled;
    uint8_t zoom_control_enabled;
    uint8_t accel_keys_enabled;
    uint8_t muted;
    int32_t new_window_policy;
    wchar_t *user_agent;

    int32_t can_go_back;
    int32_t can_go_forward;
    char *cached_url;
    char *cached_title;

    int64_t tok_nav_start;
    int64_t tok_nav_done;
    int64_t tok_src;
    int64_t tok_hist;
    int64_t tok_title;
    int64_t tok_msg;
    int64_t tok_newwin;
    int64_t tok_got_focus;
    int64_t tok_lost_focus;

    wv_cmd_node *cmd_head;
    wv_cmd_node *cmd_tail;
} wv_slot;

typedef struct {
    w32_webview_event meta;
    char *str1;
    char *str2;
} wv_ev_item;

static wv_env_entry g_envs[W32_WV_MAX_ENVS];
static wv_slot g_slots[W32_WV_MAX_INSTANCES];
static wv_ev_item g_ev_queue[W32_WV_EV_CAP];
static uint32_t g_ev_head = 0;
static uint32_t g_ev_tail = 0;
static uint64_t g_next_script_req = 1;
static int g_host_class_registered = 0;

static wv_slot *wv_get_slot(uint32_t id) {
    if (id == 0 || id > W32_WV_MAX_INSTANCES) return NULL;
    return g_slots[id - 1].used ? &g_slots[id - 1] : NULL;
}

static uint32_t wv_id_from_idx(int idx) {
    return (uint32_t)(idx + 1);
}

/* ---- Event Queue --------------------------------------------------------- */

static void wv_push_event(
    uint32_t wv_id,
    uint32_t kind,
    uint64_t req_id,
    int32_t int_val,
    uint8_t flag_a,
    uint8_t flag_b,
    const char *s1_owned,
    const char *s2_owned
) {
    uint32_t next_tail = (g_ev_tail + 1) % W32_WV_EV_CAP;
    if (next_tail == g_ev_head) {
        free(g_ev_queue[g_ev_head].str1);
        free(g_ev_queue[g_ev_head].str2);
        g_ev_queue[g_ev_head].str1 = NULL;
        g_ev_queue[g_ev_head].str2 = NULL;
        g_ev_head = (g_ev_head + 1) % W32_WV_EV_CAP;
    }

    wv_ev_item *it = &g_ev_queue[g_ev_tail];
    memset(&it->meta, 0, sizeof(it->meta));
    it->meta.webview_id = wv_id;
    it->meta.kind = kind;
    it->meta.req_id = req_id;
    it->meta.int_val = int_val;
    it->meta.flag_a = flag_a;
    it->meta.flag_b = flag_b;
    it->str1 = (char *)s1_owned;
    it->str2 = (char *)s2_owned;
    g_ev_tail = next_tail;

    wv_slot *sl = wv_get_slot(wv_id);
    if (sl && sl->parent_hwnd && IsWindow(sl->parent_hwnd)) {
        PostMessageW(sl->parent_hwnd, W32_WM_WEBVIEW_EVENT, (WPARAM)wv_id, 0);
    }
}

uint8_t w32_webview_poll_event(w32_webview_event *out_meta, gs_rref out_str1, gs_rref out_str2) {
    if (g_ev_head == g_ev_tail) return 0;
    wv_ev_item it = g_ev_queue[g_ev_head];
    memset(&g_ev_queue[g_ev_head], 0, sizeof(wv_ev_item));
    g_ev_head = (g_ev_head + 1) % W32_WV_EV_CAP;

    if (out_meta) *out_meta = it.meta;
    if (it.str1 && it.str1[0]) {
        gs_bld_append(out_str1, it.str1, (int64_t)strlen(it.str1));
    }
    if (it.str2 && it.str2[0]) {
        gs_bld_append(out_str2, it.str2, (int64_t)strlen(it.str2));
    }
    free(it.str1);
    free(it.str2);
    return 1;
}

/* ---- Pending Command Queue ----------------------------------------------- */

static void wv_queue_cmd(wv_slot *sl, int kind, uint64_t req_id, int32_t int_arg, const wchar_t *w1, const wchar_t *w2) {
    wv_cmd_node *n = (wv_cmd_node *)calloc(1, sizeof(wv_cmd_node));
    if (!n) return;
    n->kind = kind;
    n->req_id = req_id;
    n->int_arg = int_arg;
    n->wstr1 = w1 ? wv_dup_wide(w1) : NULL;
    n->wstr2 = w2 ? wv_dup_wide(w2) : NULL;
    if (sl->cmd_tail) {
        sl->cmd_tail->next = n;
        sl->cmd_tail = n;
    } else {
        sl->cmd_head = n;
        sl->cmd_tail = n;
    }
}

static void wv_clear_cmds(wv_slot *sl) {
    wv_cmd_node *cur = sl->cmd_head;
    while (cur) {
        wv_cmd_node *nxt = cur->next;
        free(cur->wstr1);
        free(cur->wstr2);
        free(cur);
        cur = nxt;
    }
    sl->cmd_head = NULL;
    sl->cmd_tail = NULL;
}

/* ---- Host Child Window --------------------------------------------------- */

static LRESULT CALLBACK wv_host_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_ERASEBKGND) {
        uint32_t id = (uint32_t)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
        wv_slot *sl = wv_get_slot(id);
        RECT rc;
        GetClientRect(hwnd, &rc);
        COLORREF col = sl ? RGB(sl->bg_r, sl->bg_g, sl->bg_b) : RGB(24, 24, 27);
        HBRUSH br = CreateSolidBrush(col);
        FillRect((HDC)wp, &rc, br);
        DeleteObject(br);
        return 1;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static int wv_ensure_host_class(void) {
    if (g_host_class_registered) return 1;
    WNDCLASSEXW wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = wv_host_wndproc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = L"Win32GooseWebViewHost";
    ATOM a = RegisterClassExW(&wc);
    if (a != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS) {
        g_host_class_registered = 1;
        return 1;
    }
    return 0;
}

/* ---- ICoreWebView2EnvironmentOptions COM Object -------------------------- */

typedef struct {
    const void *vtbl;
    volatile LONG ref_count;
    wchar_t *additional_args;
    wchar_t *target_version;
} wv_env_options_obj;

static HRESULT STDMETHODCALLTYPE wv_opts_qi(void *This, const GUID *riid, void **ppv) {
    if (!ppv) return E_POINTER;
    if (wv_guid_eq(riid, &W32_IID_IUnknown) || wv_guid_eq(riid, &W32_IID_ICoreWebView2EnvironmentOptions)) {
        *ppv = This;
        InterlockedIncrement(&((wv_env_options_obj *)This)->ref_count);
        return S_OK;
    }
    *ppv = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE wv_opts_addref(void *This) {
    return (ULONG)InterlockedIncrement(&((wv_env_options_obj *)This)->ref_count);
}

static ULONG STDMETHODCALLTYPE wv_opts_release(void *This) {
    wv_env_options_obj *o = (wv_env_options_obj *)This;
    LONG r = InterlockedDecrement(&o->ref_count);
    if (r == 0) {
        free(o->additional_args);
        free(o->target_version);
        free(o);
    }
    return (ULONG)r;
}

static HRESULT STDMETHODCALLTYPE wv_opts_get_args(void *This, LPWSTR *val) {
    if (!val) return E_POINTER;
    wv_env_options_obj *o = (wv_env_options_obj *)This;
    *val = o->additional_args ? wv_cotask_dup(o->additional_args) : NULL;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE wv_opts_put_args(void *This, LPCWSTR val) {
    wv_env_options_obj *o = (wv_env_options_obj *)This;
    free(o->additional_args);
    o->additional_args = val ? wv_dup_wide(val) : NULL;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE wv_opts_get_lang(void *This, LPWSTR *val) {
    (void)This;
    if (!val) return E_POINTER;
    *val = NULL;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE wv_opts_put_lang(void *This, LPCWSTR val) {
    (void)This;
    (void)val;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE wv_opts_get_ver(void *This, LPWSTR *val) {
    if (!val) return E_POINTER;
    wv_env_options_obj *o = (wv_env_options_obj *)This;
    *val = o->target_version ? wv_cotask_dup(o->target_version) : NULL;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE wv_opts_put_ver(void *This, LPCWSTR val) {
    wv_env_options_obj *o = (wv_env_options_obj *)This;
    free(o->target_version);
    o->target_version = val ? wv_dup_wide(val) : NULL;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE wv_opts_get_sso(void *This, BOOL *val) {
    (void)This;
    if (!val) return E_POINTER;
    *val = FALSE;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE wv_opts_put_sso(void *This, BOOL val) {
    (void)This;
    (void)val;
    return S_OK;
}

static void *const g_wv_opts_vtbl[11] = {
    (void *)wv_opts_qi,
    (void *)wv_opts_addref,
    (void *)wv_opts_release,
    (void *)wv_opts_get_args,
    (void *)wv_opts_put_args,
    (void *)wv_opts_get_lang,
    (void *)wv_opts_put_lang,
    (void *)wv_opts_get_ver,
    (void *)wv_opts_put_ver,
    (void *)wv_opts_get_sso,
    (void *)wv_opts_put_sso,
};

static wv_env_options_obj *wv_create_env_options(const wchar_t *additional_args) {
    wv_env_options_obj *o = (wv_env_options_obj *)calloc(1, sizeof(wv_env_options_obj));
    if (!o) return NULL;
    o->vtbl = g_wv_opts_vtbl;
    o->ref_count = 1;
    o->additional_args = additional_args ? wv_dup_wide(additional_args) : NULL;
    const wchar_t *tver = (g_wv_rt.target_ver_w && g_wv_rt.target_ver_w[0]) ? g_wv_rt.target_ver_w : L"120.0.0.0";
    o->target_version = wv_dup_wide(tver);
    return o;
}

/* ---- Generic COM Callback Handler Base ----------------------------------- */

typedef struct {
    const void *vtbl;
    volatile LONG ref_count;
    const GUID *iid;
    int index;       /* slot index or env index */
    uint32_t gen;
    uint64_t req_id;
} wv_handler;

static HRESULT STDMETHODCALLTYPE wv_hdr_qi(void *This, const GUID *riid, void **ppv) {
    if (!ppv) return E_POINTER;
    wv_handler *h = (wv_handler *)This;
    if (wv_guid_eq(riid, &W32_IID_IUnknown) || (h->iid && wv_guid_eq(riid, h->iid))) {
        *ppv = This;
        InterlockedIncrement(&h->ref_count);
        return S_OK;
    }
    *ppv = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE wv_hdr_addref(void *This) {
    return (ULONG)InterlockedIncrement(&((wv_handler *)This)->ref_count);
}

static ULONG STDMETHODCALLTYPE wv_hdr_release(void *This) {
    wv_handler *h = (wv_handler *)This;
    LONG r = InterlockedDecrement(&h->ref_count);
    if (r == 0) free(h);
    return (ULONG)r;
}

static wv_handler *wv_alloc_handler(const void *vtbl, const GUID *iid, int index, uint32_t gen, uint64_t req_id) {
    wv_handler *h = (wv_handler *)calloc(1, sizeof(wv_handler));
    if (!h) return NULL;
    h->vtbl = vtbl;
    h->ref_count = 1;
    h->iid = iid;
    h->index = index;
    h->gen = gen;
    h->req_id = req_id;
    return h;
}

static wv_slot *wv_validate_handler_slot(const wv_handler *h) {
    if (!h || h->index < 0 || h->index >= W32_WV_MAX_INSTANCES) return NULL;
    wv_slot *sl = &g_slots[h->index];
    if (!sl->used || sl->gen != h->gen) return NULL;
    return sl;
}

/* ---- Low-level WebView2 COM calls ---------------------------------------- */

static void wv_ctrl_apply_geometry(wv_slot *sl) {
    if (!sl || !sl->host_hwnd) return;
    int32_t vx = sl->bounds_x;
    int32_t vy = sl->bounds_y;
    int32_t vw = sl->bounds_w;
    int32_t vh = sl->bounds_h;

    if (sl->has_clip) {
        int32_t x0 = vx > sl->clip_x ? vx : sl->clip_x;
        int32_t y0 = vy > sl->clip_y ? vy : sl->clip_y;
        int32_t x1 = (vx + vw) < (sl->clip_x + sl->clip_w) ? (vx + vw) : (sl->clip_x + sl->clip_w);
        int32_t y1 = (vy + vh) < (sl->clip_y + sl->clip_h) ? (vy + vh) : (sl->clip_y + sl->clip_h);
        int32_t cw = x1 - x0;
        int32_t ch = y1 - y0;
        if (!sl->desired_visible || vw <= 0 || vh <= 0 || cw <= 0 || ch <= 0) {
            ShowWindow(sl->host_hwnd, SW_HIDE);
            if (sl->controller) {
                typedef HRESULT (STDMETHODCALLTYPE *pfn_vis)(void *, BOOL);
                ((pfn_vis)wv_vtbl(sl->controller)[4])(sl->controller, FALSE);
            }
            return;
        }
        SetWindowPos(sl->host_hwnd, NULL, x0, y0, cw, ch, SWP_NOZORDER | SWP_NOACTIVATE);
        ShowWindow(sl->host_hwnd, SW_SHOWNOACTIVATE);
        if (sl->controller) {
            typedef HRESULT (STDMETHODCALLTYPE *pfn_vis)(void *, BOOL);
            typedef HRESULT (STDMETHODCALLTYPE *pfn_bounds)(void *, const RECT *);
            typedef HRESULT (STDMETHODCALLTYPE *pfn_notify)(void *);
            RECT rc = { vx - x0, vy - y0, (vx - x0) + vw, (vy - y0) + vh };
            ((pfn_bounds)wv_vtbl(sl->controller)[6])(sl->controller, &rc);
            ((pfn_vis)wv_vtbl(sl->controller)[4])(sl->controller, TRUE);
            ((pfn_notify)wv_vtbl(sl->controller)[23])(sl->controller);
        }
        return;
    }

    if (!sl->desired_visible || vw <= 0 || vh <= 0) {
        ShowWindow(sl->host_hwnd, SW_HIDE);
        if (sl->controller) {
            typedef HRESULT (STDMETHODCALLTYPE *pfn_vis)(void *, BOOL);
            ((pfn_vis)wv_vtbl(sl->controller)[4])(sl->controller, FALSE);
        }
        return;
    }

    SetWindowPos(sl->host_hwnd, NULL, vx, vy, vw, vh, SWP_NOZORDER | SWP_NOACTIVATE);
    ShowWindow(sl->host_hwnd, SW_SHOWNOACTIVATE);
    if (sl->controller) {
        typedef HRESULT (STDMETHODCALLTYPE *pfn_vis)(void *, BOOL);
        typedef HRESULT (STDMETHODCALLTYPE *pfn_bounds)(void *, const RECT *);
        typedef HRESULT (STDMETHODCALLTYPE *pfn_notify)(void *);
        RECT rc = { 0, 0, vw, vh };
        ((pfn_bounds)wv_vtbl(sl->controller)[6])(sl->controller, &rc);
        ((pfn_vis)wv_vtbl(sl->controller)[4])(sl->controller, TRUE);
        ((pfn_notify)wv_vtbl(sl->controller)[23])(sl->controller);
    }
}

static void wv_apply_bg_color(wv_slot *sl) {
    if (!sl || !sl->controller) return;
    void *c2 = NULL;
    if (SUCCEEDED(wv_qi(sl->controller, &W32_IID_ICoreWebView2Controller2, &c2)) && c2) {
        typedef HRESULT (STDMETHODCALLTYPE *pfn_bg)(void *, uint32_t);
        uint32_t packed = (uint32_t)sl->bg_a |
                          ((uint32_t)sl->bg_r << 8) |
                          ((uint32_t)sl->bg_g << 16) |
                          ((uint32_t)sl->bg_b << 24);
        ((pfn_bg)wv_vtbl(c2)[27])(c2, packed);
        wv_release(c2);
    }
}

static void wv_apply_color_scheme(wv_slot *sl) {
    if (!sl || !sl->webview) return;
    void *wv13 = NULL;
    if (SUCCEEDED(wv_qi(sl->webview, &W32_IID_ICoreWebView2_13, &wv13)) && wv13) {
        typedef HRESULT (STDMETHODCALLTYPE *pfn_get_prof)(void *, void **);
        void *prof = NULL;
        if (SUCCEEDED(((pfn_get_prof)wv_vtbl(wv13)[105])(wv13, &prof)) && prof) {
            typedef HRESULT (STDMETHODCALLTYPE *pfn_put_scheme)(void *, int32_t);
            ((pfn_put_scheme)wv_vtbl(prof)[9])(prof, sl->color_scheme);
            wv_release(prof);
        }
        wv_release(wv13);
    }
}

static void wv_apply_muted(wv_slot *sl) {
    if (!sl || !sl->webview) return;
    void *wv8 = NULL;
    if (SUCCEEDED(wv_qi(sl->webview, &W32_IID_ICoreWebView2_8, &wv8)) && wv8) {
        typedef HRESULT (STDMETHODCALLTYPE *pfn_put_muted)(void *, BOOL);
        ((pfn_put_muted)wv_vtbl(wv8)[84])(wv8, sl->muted ? TRUE : FALSE);
        wv_release(wv8);
    }
}

static void wv_apply_settings(wv_slot *sl) {
    if (!sl || !sl->settings) return;
    typedef HRESULT (STDMETHODCALLTYPE *pfn_put_bool)(void *, BOOL);
    ((pfn_put_bool)wv_vtbl(sl->settings)[4])(sl->settings, TRUE);  /* IsScriptEnabled */
    ((pfn_put_bool)wv_vtbl(sl->settings)[6])(sl->settings, TRUE);  /* IsWebMessageEnabled */
    ((pfn_put_bool)wv_vtbl(sl->settings)[10])(sl->settings, sl->status_bar_enabled ? TRUE : FALSE);
    ((pfn_put_bool)wv_vtbl(sl->settings)[12])(sl->settings, sl->devtools_enabled ? TRUE : FALSE);
    ((pfn_put_bool)wv_vtbl(sl->settings)[14])(sl->settings, sl->context_menu_enabled ? TRUE : FALSE);
    ((pfn_put_bool)wv_vtbl(sl->settings)[18])(sl->settings, sl->zoom_control_enabled ? TRUE : FALSE);

    if (sl->user_agent && sl->user_agent[0]) {
        void *s2 = NULL;
        if (SUCCEEDED(wv_qi(sl->settings, &W32_IID_ICoreWebView2Settings2, &s2)) && s2) {
            typedef HRESULT (STDMETHODCALLTYPE *pfn_ua)(void *, LPCWSTR);
            ((pfn_ua)wv_vtbl(s2)[22])(s2, sl->user_agent);
            wv_release(s2);
        }
    }

    void *s3 = NULL;
    if (SUCCEEDED(wv_qi(sl->settings, &W32_IID_ICoreWebView2Settings3, &s3)) && s3) {
        ((pfn_put_bool)wv_vtbl(s3)[24])(s3, sl->accel_keys_enabled ? TRUE : FALSE);
        wv_release(s3);
    }
}

/* ---- Event Handlers & Execution Callbacks -------------------------------- */

static HRESULT STDMETHODCALLTYPE wv_on_exec_script(void *This, HRESULT err, LPCWSTR result_json) {
    wv_handler *h = (wv_handler *)This;
    wv_slot *sl = wv_validate_handler_slot(h);
    if (!sl) return S_OK;
    char *res = result_json ? wv_wide_to_utf8_alloc(result_json) : wv_dup_cstr("null");
    wv_push_event(
        wv_id_from_idx(h->index),
        W32_WV_EV_SCRIPT_RESULT,
        h->req_id,
        (int32_t)err,
        SUCCEEDED(err) ? 1 : 0,
        0,
        res,
        NULL
    );
    return S_OK;
}

static void *const g_vtbl_exec_script[4] = {
    (void *)wv_hdr_qi, (void *)wv_hdr_addref, (void *)wv_hdr_release, (void *)wv_on_exec_script
};

static HRESULT STDMETHODCALLTYPE wv_on_add_init_script(void *This, HRESULT err, LPCWSTR id) {
    (void)This;
    (void)err;
    (void)id;
    return S_OK;
}

static void *const g_vtbl_add_init_script[4] = {
    (void *)wv_hdr_qi, (void *)wv_hdr_addref, (void *)wv_hdr_release, (void *)wv_on_add_init_script
};

static HRESULT STDMETHODCALLTYPE wv_on_nav_starting(void *This, void *sender, void *args) {
    (void)sender;
    wv_handler *h = (wv_handler *)This;
    wv_slot *sl = wv_validate_handler_slot(h);
    if (!sl || !args) return S_OK;

    typedef HRESULT (STDMETHODCALLTYPE *pfn_str)(void *, LPWSTR *);
    typedef HRESULT (STDMETHODCALLTYPE *pfn_bool)(void *, BOOL *);
    LPWSTR uri = NULL;
    BOOL user_init = FALSE, redir = FALSE;
    ((pfn_str)wv_vtbl(args)[3])(args, &uri);
    ((pfn_bool)wv_vtbl(args)[4])(args, &user_init);
    ((pfn_bool)wv_vtbl(args)[5])(args, &redir);
    char *u8 = uri ? wv_wide_to_utf8_alloc(uri) : NULL;
    if (uri) CoTaskMemFree(uri);

    if (u8) {
        free(sl->cached_url);
        sl->cached_url = wv_dup_cstr(u8);
    }
    wv_push_event(
        wv_id_from_idx(h->index),
        W32_WV_EV_NAV_STARTING,
        0, 0,
        user_init ? 1 : 0,
        redir ? 1 : 0,
        u8,
        NULL
    );
    return S_OK;
}

static void *const g_vtbl_nav_starting[4] = {
    (void *)wv_hdr_qi, (void *)wv_hdr_addref, (void *)wv_hdr_release, (void *)wv_on_nav_starting
};

static HRESULT STDMETHODCALLTYPE wv_on_nav_completed(void *This, void *sender, void *args) {
    (void)sender;
    wv_handler *h = (wv_handler *)This;
    wv_slot *sl = wv_validate_handler_slot(h);
    if (!sl || !args) return S_OK;

    typedef HRESULT (STDMETHODCALLTYPE *pfn_bool)(void *, BOOL *);
    typedef HRESULT (STDMETHODCALLTYPE *pfn_int)(void *, int32_t *);
    BOOL ok = FALSE;
    int32_t err_status = 0;
    ((pfn_bool)wv_vtbl(args)[3])(args, &ok);
    ((pfn_int)wv_vtbl(args)[4])(args, &err_status);

    wv_push_event(
        wv_id_from_idx(h->index),
        W32_WV_EV_NAV_COMPLETED,
        0,
        err_status,
        ok ? 1 : 0,
        0,
        NULL,
        NULL
    );
    return S_OK;
}

static void *const g_vtbl_nav_completed[4] = {
    (void *)wv_hdr_qi, (void *)wv_hdr_addref, (void *)wv_hdr_release, (void *)wv_on_nav_completed
};

static HRESULT STDMETHODCALLTYPE wv_on_source_changed(void *This, void *sender, void *args) {
    wv_handler *h = (wv_handler *)This;
    wv_slot *sl = wv_validate_handler_slot(h);
    if (!sl || !sender) return S_OK;

    typedef HRESULT (STDMETHODCALLTYPE *pfn_str)(void *, LPWSTR *);
    typedef HRESULT (STDMETHODCALLTYPE *pfn_bool)(void *, BOOL *);
    BOOL is_new_doc = FALSE;
    if (args) ((pfn_bool)wv_vtbl(args)[3])(args, &is_new_doc);

    LPWSTR src = NULL;
    ((pfn_str)wv_vtbl(sender)[4])(sender, &src);
    char *u8 = src ? wv_wide_to_utf8_alloc(src) : NULL;
    if (src) CoTaskMemFree(src);

    free(sl->cached_url);
    sl->cached_url = u8 ? wv_dup_cstr(u8) : NULL;

    wv_push_event(
        wv_id_from_idx(h->index),
        W32_WV_EV_SOURCE_CHANGED,
        0, 0,
        is_new_doc ? 1 : 0,
        0,
        u8,
        NULL
    );
    return S_OK;
}

static void *const g_vtbl_source_changed[4] = {
    (void *)wv_hdr_qi, (void *)wv_hdr_addref, (void *)wv_hdr_release, (void *)wv_on_source_changed
};

static HRESULT STDMETHODCALLTYPE wv_on_history_changed(void *This, void *sender, void *args) {
    (void)args;
    wv_handler *h = (wv_handler *)This;
    wv_slot *sl = wv_validate_handler_slot(h);
    if (!sl || !sender) return S_OK;

    typedef HRESULT (STDMETHODCALLTYPE *pfn_bool)(void *, BOOL *);
    BOOL can_back = FALSE, can_fwd = FALSE;
    ((pfn_bool)wv_vtbl(sender)[38])(sender, &can_back);
    ((pfn_bool)wv_vtbl(sender)[39])(sender, &can_fwd);
    sl->can_go_back = can_back ? 1 : 0;
    sl->can_go_forward = can_fwd ? 1 : 0;

    wv_push_event(
        wv_id_from_idx(h->index),
        W32_WV_EV_HISTORY_CHANGED,
        0, 0,
        can_back ? 1 : 0,
        can_fwd ? 1 : 0,
        NULL,
        NULL
    );
    return S_OK;
}

static void *const g_vtbl_history_changed[4] = {
    (void *)wv_hdr_qi, (void *)wv_hdr_addref, (void *)wv_hdr_release, (void *)wv_on_history_changed
};

static HRESULT STDMETHODCALLTYPE wv_on_title_changed(void *This, void *sender, void *args) {
    (void)args;
    wv_handler *h = (wv_handler *)This;
    wv_slot *sl = wv_validate_handler_slot(h);
    if (!sl || !sender) return S_OK;

    typedef HRESULT (STDMETHODCALLTYPE *pfn_str)(void *, LPWSTR *);
    LPWSTR title = NULL;
    ((pfn_str)wv_vtbl(sender)[48])(sender, &title);
    char *u8 = title ? wv_wide_to_utf8_alloc(title) : NULL;
    if (title) CoTaskMemFree(title);

    free(sl->cached_title);
    sl->cached_title = u8 ? wv_dup_cstr(u8) : NULL;

    wv_push_event(
        wv_id_from_idx(h->index),
        W32_WV_EV_TITLE_CHANGED,
        0, 0, 0, 0,
        u8,
        NULL
    );
    return S_OK;
}

static void *const g_vtbl_title_changed[4] = {
    (void *)wv_hdr_qi, (void *)wv_hdr_addref, (void *)wv_hdr_release, (void *)wv_on_title_changed
};

static HRESULT STDMETHODCALLTYPE wv_on_web_message(void *This, void *sender, void *args) {
    (void)sender;
    wv_handler *h = (wv_handler *)This;
    wv_slot *sl = wv_validate_handler_slot(h);
    if (!sl || !args) return S_OK;

    typedef HRESULT (STDMETHODCALLTYPE *pfn_str)(void *, LPWSTR *);
    LPWSTR msg_w = NULL;
    uint8_t is_json = 0;
    if (FAILED(((pfn_str)wv_vtbl(args)[5])(args, &msg_w)) || !msg_w) {
        if (msg_w) { CoTaskMemFree(msg_w); msg_w = NULL; }
        ((pfn_str)wv_vtbl(args)[4])(args, &msg_w);
        is_json = 1;
    }
    char *msg_u8 = msg_w ? wv_wide_to_utf8_alloc(msg_w) : NULL;
    if (msg_w) CoTaskMemFree(msg_w);

    LPWSTR src_w = NULL;
    ((pfn_str)wv_vtbl(args)[3])(args, &src_w);
    char *src_u8 = src_w ? wv_wide_to_utf8_alloc(src_w) : NULL;
    if (src_w) CoTaskMemFree(src_w);

    wv_push_event(
        wv_id_from_idx(h->index),
        W32_WV_EV_WEB_MESSAGE,
        0, 0,
        is_json,
        0,
        msg_u8,
        src_u8
    );
    return S_OK;
}

static void *const g_vtbl_web_message[4] = {
    (void *)wv_hdr_qi, (void *)wv_hdr_addref, (void *)wv_hdr_release, (void *)wv_on_web_message
};

static HRESULT STDMETHODCALLTYPE wv_on_new_window(void *This, void *sender, void *args) {
    wv_handler *h = (wv_handler *)This;
    wv_slot *sl = wv_validate_handler_slot(h);
    if (!sl || !args) return S_OK;

    typedef HRESULT (STDMETHODCALLTYPE *pfn_str)(void *, LPWSTR *);
    typedef HRESULT (STDMETHODCALLTYPE *pfn_bool_out)(void *, BOOL *);
    typedef HRESULT (STDMETHODCALLTYPE *pfn_bool_in)(void *, BOOL);
    LPWSTR uri_w = NULL;
    BOOL user_init = FALSE;
    ((pfn_str)wv_vtbl(args)[3])(args, &uri_w);
    ((pfn_bool_out)wv_vtbl(args)[8])(args, &user_init);

    if (sl->new_window_policy == W32_WV_NEWWIN_BLOCK) {
        ((pfn_bool_in)wv_vtbl(args)[6])(args, TRUE);
    } else if (sl->new_window_policy == W32_WV_NEWWIN_NAVIGATE_SELF) {
        ((pfn_bool_in)wv_vtbl(args)[6])(args, TRUE);
        if (uri_w && sender) {
            typedef HRESULT (STDMETHODCALLTYPE *pfn_nav)(void *, LPCWSTR);
            ((pfn_nav)wv_vtbl(sender)[5])(sender, uri_w);
        }
    } else if (sl->new_window_policy == W32_WV_NEWWIN_OPEN_EXTERNAL) {
        ((pfn_bool_in)wv_vtbl(args)[6])(args, TRUE);
        if (uri_w) {
            ShellExecuteW(NULL, L"open", uri_w, NULL, NULL, SW_SHOWNORMAL);
        }
    }

    char *u8 = uri_w ? wv_wide_to_utf8_alloc(uri_w) : NULL;
    if (uri_w) CoTaskMemFree(uri_w);

    wv_push_event(
        wv_id_from_idx(h->index),
        W32_WV_EV_NEW_WINDOW,
        0, 0,
        user_init ? 1 : 0,
        0,
        u8,
        NULL
    );
    return S_OK;
}

static void *const g_vtbl_new_window[4] = {
    (void *)wv_hdr_qi, (void *)wv_hdr_addref, (void *)wv_hdr_release, (void *)wv_on_new_window
};

static HRESULT STDMETHODCALLTYPE wv_on_got_focus(void *This, void *sender, void *args) {
    (void)sender;
    (void)args;
    wv_handler *h = (wv_handler *)This;
    if (wv_validate_handler_slot(h)) {
        wv_push_event(wv_id_from_idx(h->index), W32_WV_EV_FOCUS_CHANGED, 0, 0, 1, 0, NULL, NULL);
    }
    return S_OK;
}

static void *const g_vtbl_got_focus[4] = {
    (void *)wv_hdr_qi, (void *)wv_hdr_addref, (void *)wv_hdr_release, (void *)wv_on_got_focus
};

static HRESULT STDMETHODCALLTYPE wv_on_lost_focus(void *This, void *sender, void *args) {
    (void)sender;
    (void)args;
    wv_handler *h = (wv_handler *)This;
    if (wv_validate_handler_slot(h)) {
        wv_push_event(wv_id_from_idx(h->index), W32_WV_EV_FOCUS_CHANGED, 0, 0, 0, 0, NULL, NULL);
    }
    return S_OK;
}

static void *const g_vtbl_lost_focus[4] = {
    (void *)wv_hdr_qi, (void *)wv_hdr_addref, (void *)wv_hdr_release, (void *)wv_on_lost_focus
};

/* ---- Command Execution on Live Controller/CoreWebView2 ------------------- */

static void wv_exec_cmd_now(wv_slot *sl, int idx, int kind, uint64_t req_id, int32_t int_arg, const wchar_t *w1, const wchar_t *w2) {
    if (!sl || !sl->webview) return;
    typedef HRESULT (STDMETHODCALLTYPE *pfn_wstr)(void *, LPCWSTR);
    typedef HRESULT (STDMETHODCALLTYPE *pfn_void)(void *);

    switch (kind) {
        case W32_WV_CMD_NAVIGATE:
            if (w1) ((pfn_wstr)wv_vtbl(sl->webview)[5])(sl->webview, w1);
            break;
        case W32_WV_CMD_SET_HTML:
            if (w1) ((pfn_wstr)wv_vtbl(sl->webview)[6])(sl->webview, w1);
            break;
        case W32_WV_CMD_EVAL_JS: {
            typedef HRESULT (STDMETHODCALLTYPE *pfn_exec)(void *, LPCWSTR, void *);
            wv_handler *h = wv_alloc_handler(g_vtbl_exec_script, &W32_IID_ExecScript, idx, sl->gen, req_id);
            HRESULT hr = E_OUTOFMEMORY;
            if (h && w1) {
                hr = ((pfn_exec)wv_vtbl(sl->webview)[29])(sl->webview, w1, h);
            }
            if (h) wv_release(h);
            if (FAILED(hr)) {
                wv_push_event(wv_id_from_idx(idx), W32_WV_EV_SCRIPT_RESULT, req_id, (int32_t)hr, 0, 0, wv_dup_cstr("null"), NULL);
            }
            break;
        }
        case W32_WV_CMD_ADD_INIT: {
            typedef HRESULT (STDMETHODCALLTYPE *pfn_add)(void *, LPCWSTR, void *);
            wv_handler *h = wv_alloc_handler(g_vtbl_add_init_script, &W32_IID_AddInitScript, idx, sl->gen, 0);
            if (h && w1) {
                ((pfn_add)wv_vtbl(sl->webview)[27])(sl->webview, w1, h);
            }
            if (h) wv_release(h);
            break;
        }
        case W32_WV_CMD_POST_JSON:
            if (w1) ((pfn_wstr)wv_vtbl(sl->webview)[32])(sl->webview, w1);
            break;
        case W32_WV_CMD_POST_STRING:
            if (w1) ((pfn_wstr)wv_vtbl(sl->webview)[33])(sl->webview, w1);
            break;
        case W32_WV_CMD_SET_VHOST: {
            void *wv3 = NULL;
            if (w1 && w2 && SUCCEEDED(wv_qi(sl->webview, &W32_IID_ICoreWebView2_3, &wv3)) && wv3) {
                typedef HRESULT (STDMETHODCALLTYPE *pfn_map)(void *, LPCWSTR, LPCWSTR, int32_t);
                ((pfn_map)wv_vtbl(wv3)[71])(wv3, w1, w2, int_arg);
                wv_release(wv3);
            }
            break;
        }
        case W32_WV_CMD_CLEAR_VHOST: {
            void *wv3 = NULL;
            if (w1 && SUCCEEDED(wv_qi(sl->webview, &W32_IID_ICoreWebView2_3, &wv3)) && wv3) {
                typedef HRESULT (STDMETHODCALLTYPE *pfn_clr)(void *, LPCWSTR);
                ((pfn_clr)wv_vtbl(wv3)[72])(wv3, w1);
                wv_release(wv3);
            }
            break;
        }
        case W32_WV_CMD_OPEN_DEVTOOLS:
            ((pfn_void)wv_vtbl(sl->webview)[51])(sl->webview);
            break;
    }
}

static void wv_flush_pending_cmds(wv_slot *sl, int idx) {
    /* First apply virtual hosts & init scripts so initial navigation sees them. */
    for (int pass = 0; pass < 2; pass++) {
        wv_cmd_node *cur = sl->cmd_head;
        while (cur) {
            int is_setup = (cur->kind == W32_WV_CMD_SET_VHOST || cur->kind == W32_WV_CMD_CLEAR_VHOST || cur->kind == W32_WV_CMD_ADD_INIT);
            if ((pass == 0 && is_setup) || (pass == 1 && !is_setup)) {
                wv_exec_cmd_now(sl, idx, cur->kind, cur->req_id, cur->int_arg, cur->wstr1, cur->wstr2);
            }
            cur = cur->next;
        }
    }
    wv_clear_cmds(sl);
}

/* ---- Environment & Controller Completion Handlers ------------------------ */

static HRESULT STDMETHODCALLTYPE wv_on_controller_created(void *This, HRESULT err, void *ctrl) {
    wv_handler *h = (wv_handler *)This;
    wv_slot *sl = wv_validate_handler_slot(h);
    if (!sl) {
        if (SUCCEEDED(err) && ctrl) {
            typedef HRESULT (STDMETHODCALLTYPE *pfn_close)(void *);
            ((pfn_close)wv_vtbl(ctrl)[24])(ctrl);
        }
        return S_OK;
    }

    if (FAILED(err) || !ctrl) {
        sl->failed = 1;
        sl->last_hr = (int32_t)err;
        wv_clear_cmds(sl);
        char *msg = wv_dup_cstr("CreateCoreWebView2Controller failed");
        wv_push_event(wv_id_from_idx(h->index), W32_WV_EV_INIT_FAILED, 0, (int32_t)err, 0, 0, msg, NULL);
        return S_OK;
    }

    typedef ULONG (STDMETHODCALLTYPE *pfn_addref)(void *);
    typedef HRESULT (STDMETHODCALLTYPE *pfn_get_wv)(void *, void **);
    typedef HRESULT (STDMETHODCALLTYPE *pfn_get_set)(void *, void **);
    typedef HRESULT (STDMETHODCALLTYPE *pfn_zoom)(void *, double);
    typedef HRESULT (STDMETHODCALLTYPE *pfn_reg)(void *, void *, int64_t *);

    ((pfn_addref)wv_vtbl(ctrl)[1])(ctrl);
    sl->controller = ctrl;

    void *wv = NULL;
    if (FAILED(((pfn_get_wv)wv_vtbl(ctrl)[25])(ctrl, &wv)) || !wv) {
        sl->failed = 1;
        sl->last_hr = E_FAIL;
        wv_clear_cmds(sl);
        wv_push_event(wv_id_from_idx(h->index), W32_WV_EV_INIT_FAILED, 0, E_FAIL, 0, 0, wv_dup_cstr("get_CoreWebView2 failed"), NULL);
        return S_OK;
    }
    sl->webview = wv;
    ((pfn_get_set)wv_vtbl(wv)[3])(wv, &sl->settings);

    wv_apply_settings(sl);
    wv_apply_bg_color(sl);
    wv_apply_color_scheme(sl);
    wv_apply_muted(sl);
    if (sl->zoom_factor > 0.0 && sl->zoom_factor != 1.0) {
        ((pfn_zoom)wv_vtbl(ctrl)[8])(ctrl, sl->zoom_factor);
    }

    wv_handler *hn;
    if ((hn = wv_alloc_handler(g_vtbl_nav_starting, &W32_IID_NavStarting, h->index, sl->gen, 0)) != NULL) {
        ((pfn_reg)wv_vtbl(wv)[7])(wv, hn, &sl->tok_nav_start);
        wv_release(hn);
    }
    if ((hn = wv_alloc_handler(g_vtbl_source_changed, &W32_IID_SourceChanged, h->index, sl->gen, 0)) != NULL) {
        ((pfn_reg)wv_vtbl(wv)[11])(wv, hn, &sl->tok_src);
        wv_release(hn);
    }
    if ((hn = wv_alloc_handler(g_vtbl_history_changed, &W32_IID_HistoryChanged, h->index, sl->gen, 0)) != NULL) {
        ((pfn_reg)wv_vtbl(wv)[13])(wv, hn, &sl->tok_hist);
        wv_release(hn);
    }
    if ((hn = wv_alloc_handler(g_vtbl_nav_completed, &W32_IID_NavCompleted, h->index, sl->gen, 0)) != NULL) {
        ((pfn_reg)wv_vtbl(wv)[15])(wv, hn, &sl->tok_nav_done);
        wv_release(hn);
    }
    if ((hn = wv_alloc_handler(g_vtbl_web_message, &W32_IID_WebMessage, h->index, sl->gen, 0)) != NULL) {
        ((pfn_reg)wv_vtbl(wv)[34])(wv, hn, &sl->tok_msg);
        wv_release(hn);
    }
    if ((hn = wv_alloc_handler(g_vtbl_new_window, &W32_IID_NewWindow, h->index, sl->gen, 0)) != NULL) {
        ((pfn_reg)wv_vtbl(wv)[44])(wv, hn, &sl->tok_newwin);
        wv_release(hn);
    }
    if ((hn = wv_alloc_handler(g_vtbl_title_changed, &W32_IID_TitleChanged, h->index, sl->gen, 0)) != NULL) {
        ((pfn_reg)wv_vtbl(wv)[46])(wv, hn, &sl->tok_title);
        wv_release(hn);
    }
    if ((hn = wv_alloc_handler(g_vtbl_got_focus, &W32_IID_FocusChanged, h->index, sl->gen, 0)) != NULL) {
        ((pfn_reg)wv_vtbl(ctrl)[15])(ctrl, hn, &sl->tok_got_focus);
        wv_release(hn);
    }
    if ((hn = wv_alloc_handler(g_vtbl_lost_focus, &W32_IID_FocusChanged, h->index, sl->gen, 0)) != NULL) {
        ((pfn_reg)wv_vtbl(ctrl)[17])(ctrl, hn, &sl->tok_lost_focus);
        wv_release(hn);
    }

    sl->ready = 1;
    wv_ctrl_apply_geometry(sl);
    wv_flush_pending_cmds(sl, h->index);

    char *ver = g_wv_rt.runtime_ver_utf8 ? wv_dup_cstr(g_wv_rt.runtime_ver_utf8) : NULL;
    wv_push_event(wv_id_from_idx(h->index), W32_WV_EV_READY, 0, S_OK, 0, 0, ver, NULL);
    return S_OK;
}

static void *const g_vtbl_ctrl_created[4] = {
    (void *)wv_hdr_qi, (void *)wv_hdr_addref, (void *)wv_hdr_release, (void *)wv_on_controller_created
};

static void wv_start_controller_for_slot(int idx, int env_idx) {
    wv_slot *sl = &g_slots[idx];
    wv_env_entry *ee = &g_envs[env_idx];
    if (!sl->used || sl->controller || sl->failed) return;
    if (ee->failed || !ee->env) {
        sl->failed = 1;
        sl->last_hr = ee->last_hr;
        wv_clear_cmds(sl);
        wv_push_event(
            wv_id_from_idx(idx),
            W32_WV_EV_INIT_FAILED,
            0,
            ee->last_hr,
            0, 0,
            wv_dup_cstr("WebView2 environment creation failed"),
            NULL
        );
        return;
    }

    typedef HRESULT (STDMETHODCALLTYPE *pfn_create_ctrl)(void *, HWND, void *);
    wv_handler *ch = wv_alloc_handler(g_vtbl_ctrl_created, &W32_IID_CtrlCompleted, idx, sl->gen, 0);
    if (!ch) {
        sl->failed = 1;
        sl->last_hr = E_OUTOFMEMORY;
        return;
    }
    HRESULT hr = ((pfn_create_ctrl)wv_vtbl(ee->env)[3])(ee->env, sl->host_hwnd, ch);
    wv_release(ch);
    if (FAILED(hr)) {
        sl->failed = 1;
        sl->last_hr = (int32_t)hr;
        wv_clear_cmds(sl);
        wv_push_event(
            wv_id_from_idx(idx),
            W32_WV_EV_INIT_FAILED,
            0,
            (int32_t)hr,
            0, 0,
            wv_dup_cstr("CreateCoreWebView2Controller call failed"),
            NULL
        );
    }
}

static HRESULT STDMETHODCALLTYPE wv_on_env_created(void *This, HRESULT err, void *env) {
    wv_handler *h = (wv_handler *)This;
    int eidx = h->index;
    if (eidx < 0 || eidx >= W32_WV_MAX_ENVS || !g_envs[eidx].used) return S_OK;
    wv_env_entry *ee = &g_envs[eidx];

    if (FAILED(err) || !env) {
        ee->failed = 1;
        ee->last_hr = (int32_t)err;
    } else {
        typedef ULONG (STDMETHODCALLTYPE *pfn_addref)(void *);
        typedef HRESULT (STDMETHODCALLTYPE *pfn_ver)(void *, LPWSTR *);
        ((pfn_addref)wv_vtbl(env)[1])(env);
        ee->env = env;
        ee->ready = 1;

        LPWSTR ver_w = NULL;
        if (SUCCEEDED(((pfn_ver)wv_vtbl(env)[5])(env, &ver_w)) && ver_w) {
            if (!g_wv_rt.runtime_ver_utf8 || !g_wv_rt.runtime_ver_utf8[0]) {
                free(g_wv_rt.runtime_ver_utf8);
                g_wv_rt.runtime_ver_utf8 = wv_wide_to_utf8_alloc(ver_w);
            }
            CoTaskMemFree(ver_w);
        }
    }

    for (int i = 0; i < W32_WV_MAX_INSTANCES; i++) {
        if (g_slots[i].used && g_slots[i].env_idx == eidx && !g_slots[i].controller && !g_slots[i].failed) {
            wv_start_controller_for_slot(i, eidx);
        }
    }
    return S_OK;
}

static void *const g_vtbl_env_created[4] = {
    (void *)wv_hdr_qi, (void *)wv_hdr_addref, (void *)wv_hdr_release, (void *)wv_on_env_created
};

/* ---- Lifecycle & Public C Shim API --------------------------------------- */

uint8_t w32_webview_is_available(void) {
    return wv_probe_runtime() ? 1 : 0;
}

uint8_t w32_webview_runtime_version(gs_rref out) {
    if (!wv_probe_runtime()) return 0;
    if (!g_wv_rt.runtime_ver_utf8 || !g_wv_rt.runtime_ver_utf8[0]) return 0;
    gs_bld_append(out, g_wv_rt.runtime_ver_utf8, (int64_t)strlen(g_wv_rt.runtime_ver_utf8));
    return 1;
}

static void wv_update_parent_suppress(HWND parent_hwnd) {
    if (!parent_hwnd) return;
    for (int i = 0; i < W32_WV_MAX_INSTANCES; i++) {
        if (g_slots[i].used && g_slots[i].parent_hwnd == parent_hwnd) {
            return;
        }
    }
    w32_set_window_suppress_erase((uint64_t)(uintptr_t)parent_hwnd, 0);
}

static void wv_destroy_slot(int idx) {
    wv_slot *sl = &g_slots[idx];
    if (!sl->used) return;
    HWND parent = sl->parent_hwnd;
    uint32_t next_gen = sl->gen + 1;
    if (next_gen == 0) next_gen = 1;

    wv_clear_cmds(sl);
    if (sl->controller) {
        typedef HRESULT (STDMETHODCALLTYPE *pfn_close)(void *);
        ((pfn_close)wv_vtbl(sl->controller)[24])(sl->controller);
    }
    wv_release(sl->settings);
    wv_release(sl->webview);
    wv_release(sl->controller);

    if (sl->host_hwnd && IsWindow(sl->host_hwnd)) {
        DestroyWindow(sl->host_hwnd);
    }
    free(sl->user_agent);
    free(sl->cached_url);
    free(sl->cached_title);

    memset(sl, 0, sizeof(*sl));
    sl->gen = next_gen;
    wv_update_parent_suppress(parent);
}

void w32_webview_on_parent_destroy(HWND hwnd) {
    if (!hwnd) return;
    for (int i = 0; i < W32_WV_MAX_INSTANCES; i++) {
        if (g_slots[i].used && g_slots[i].parent_hwnd == hwnd) {
            wv_destroy_slot(i);
        }
    }
}

static int wv_env_has_active_slots(int eidx) {
    for (int i = 0; i < W32_WV_MAX_INSTANCES; i++) {
        if (g_slots[i].used && g_slots[i].env_idx == eidx) return 1;
    }
    return 0;
}

uint32_t w32_webview_create(
    uint64_t parent_hwnd_u64,
    int32_t x,
    int32_t y,
    int32_t w,
    int32_t h,
    w32_bytes user_data_dir,
    w32_bytes additional_args
) {
    HWND parent = (HWND)(uintptr_t)parent_hwnd_u64;
    if (!parent || !IsWindow(parent)) return 0;
    if (!wv_ensure_host_class()) return 0;

    int idx = -1;
    for (int i = 0; i < W32_WV_MAX_INSTANCES; i++) {
        if (!g_slots[i].used) { idx = i; break; }
    }
    if (idx < 0) return 0;

    /* Ensure parent clips child HWNDs and suppresses background erase flicker. */
    LONG_PTR pstyle = GetWindowLongPtrW(parent, GWL_STYLE);
    if ((pstyle & WS_CLIPCHILDREN) == 0) {
        SetWindowLongPtrW(parent, GWL_STYLE, pstyle | WS_CLIPCHILDREN);
    }
    w32_set_window_suppress_erase(parent_hwnd_u64, 1);

    uint32_t id = wv_id_from_idx(idx);
    int32_t init_w = w > 0 ? w : 1;
    int32_t init_h = h > 0 ? h : 1;
    HWND host = CreateWindowExW(
        0,
        L"Win32GooseWebViewHost",
        L"",
        WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
        x, y, init_w, init_h,
        parent,
        NULL,
        GetModuleHandleW(NULL),
        NULL
    );
    if (!host) {
        wv_update_parent_suppress(parent);
        return 0;
    }
    SetWindowLongPtrW(host, GWLP_USERDATA, (LONG_PTR)id);

    uint32_t gen = g_slots[idx].gen ? g_slots[idx].gen : 1;
    memset(&g_slots[idx], 0, sizeof(wv_slot));
    wv_slot *sl = &g_slots[idx];
    sl->used = 1;
    sl->gen = gen;
    sl->parent_hwnd = parent;
    sl->host_hwnd = host;
    sl->env_idx = -1;
    sl->bounds_x = x;
    sl->bounds_y = y;
    sl->bounds_w = w;
    sl->bounds_h = h;
    sl->desired_visible = 1;
    sl->bg_r = 24;
    sl->bg_g = 24;
    sl->bg_b = 27;
    sl->bg_a = 255;
    sl->color_scheme = 0;
    sl->zoom_factor = 1.0;
    sl->devtools_enabled = 1;
    sl->context_menu_enabled = 1;
    sl->status_bar_enabled = 0;
    sl->zoom_control_enabled = 1;
    sl->accel_keys_enabled = 1;
    sl->new_window_policy = W32_WV_NEWWIN_BLOCK;

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    if (!wv_probe_runtime()) {
        sl->failed = 1;
        sl->last_hr = (int32_t)HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
        wv_push_event(
            id,
            W32_WV_EV_INIT_FAILED,
            0,
            sl->last_hr,
            0, 0,
            wv_dup_cstr("Microsoft Edge WebView2 Runtime not found"),
            NULL
        );
        return id;
    }

    wchar_t *udir = wv_utf8_to_wide_alloc(user_data_dir, 0);
    if (!udir) udir = wv_default_user_data_dir();
    if (udir && udir[0]) wv_create_dirs_w(udir);
    wchar_t *uargs = wv_utf8_to_wide_alloc(additional_args, 0);

    int eidx = -1;
    int free_e = -1;
    int failed_e = -1;
    for (int i = 0; i < W32_WV_MAX_ENVS; i++) {
        if (g_envs[i].used && !g_envs[i].failed &&
            wv_wide_eq(g_envs[i].user_data_dir, udir) &&
            wv_wide_eq(g_envs[i].additional_args, uargs)) {
            eidx = i;
            break;
        }
        if (!g_envs[i].used && free_e < 0) free_e = i;
        if (g_envs[i].used && g_envs[i].failed && !g_envs[i].env && failed_e < 0 && !wv_env_has_active_slots(i)) {
            failed_e = i;
        }
    }

    if (eidx >= 0) {
        free(udir);
        free(uargs);
        sl->env_idx = eidx;
        if (g_envs[eidx].ready) {
            wv_start_controller_for_slot(idx, eidx);
        }
        return id;
    }

    if (free_e < 0 && failed_e >= 0) {
        free(g_envs[failed_e].user_data_dir);
        free(g_envs[failed_e].additional_args);
        memset(&g_envs[failed_e], 0, sizeof(g_envs[failed_e]));
        free_e = failed_e;
    }

    if (free_e < 0) {
        free(udir);
        free(uargs);
        sl->failed = 1;
        sl->last_hr = E_OUTOFMEMORY;
        wv_push_event(id, W32_WV_EV_INIT_FAILED, 0, E_OUTOFMEMORY, 0, 0, wv_dup_cstr("WebView2 environment cache full"), NULL);
        return id;
    }

    wv_env_entry *ee = &g_envs[free_e];
    memset(ee, 0, sizeof(*ee));
    ee->used = 1;
    ee->user_data_dir = udir;
    ee->additional_args = uargs;
    sl->env_idx = free_e;

    wv_handler *eh = wv_alloc_handler(g_vtbl_env_created, &W32_IID_EnvCompleted, free_e, 0, 0);
    wv_env_options_obj *opts = (uargs && uargs[0]) ? wv_create_env_options(uargs) : NULL;
    HRESULT hr = E_FAIL;
    if (eh) {
        if (g_wv_rt.mode == WV_LOADER_PUBLIC && g_wv_rt.pub_create_env) {
            hr = g_wv_rt.pub_create_env(NULL, udir, opts, eh);
        } else if (g_wv_rt.mode == WV_LOADER_INTERNAL && g_wv_rt.int_create_env) {
            hr = g_wv_rt.int_create_env(1, g_wv_rt.runtime_type, udir, opts, eh);
        }
        wv_release(eh);
    }
    if (opts) wv_opts_release(opts);

    if (FAILED(hr)) {
        ee->failed = 1;
        ee->last_hr = (int32_t)hr;
        sl->failed = 1;
        sl->last_hr = (int32_t)hr;
        wv_push_event(
            id,
            W32_WV_EV_INIT_FAILED,
            0,
            (int32_t)hr,
            0, 0,
            wv_dup_cstr("CreateWebView2Environment failed"),
            NULL
        );
    }
    return id;
}

void w32_webview_destroy(uint32_t wv_id) {
    if (wv_id == 0 || wv_id > W32_WV_MAX_INSTANCES) return;
    wv_destroy_slot((int)(wv_id - 1));
}

uint8_t w32_webview_is_ready(uint32_t wv_id) {
    wv_slot *sl = wv_get_slot(wv_id);
    return (sl && sl->ready) ? 1 : 0;
}

uint64_t w32_webview_host_hwnd(uint32_t wv_id) {
    wv_slot *sl = wv_get_slot(wv_id);
    return sl ? (uint64_t)(uintptr_t)sl->host_hwnd : 0;
}

void w32_webview_set_bounds(uint32_t wv_id, int32_t x, int32_t y, int32_t w, int32_t h) {
    wv_slot *sl = wv_get_slot(wv_id);
    if (!sl) return;
    sl->bounds_x = x;
    sl->bounds_y = y;
    sl->bounds_w = w;
    sl->bounds_h = h;
    sl->has_clip = 0;
    wv_ctrl_apply_geometry(sl);
}

void w32_webview_set_bounds_clipped(
    uint32_t wv_id,
    int32_t x,
    int32_t y,
    int32_t w,
    int32_t h,
    int32_t clip_x,
    int32_t clip_y,
    int32_t clip_w,
    int32_t clip_h
) {
    wv_slot *sl = wv_get_slot(wv_id);
    if (!sl) return;
    sl->bounds_x = x;
    sl->bounds_y = y;
    sl->bounds_w = w;
    sl->bounds_h = h;
    sl->clip_x = clip_x;
    sl->clip_y = clip_y;
    sl->clip_w = clip_w;
    sl->clip_h = clip_h;
    sl->has_clip = 1;
    wv_ctrl_apply_geometry(sl);
}

void w32_webview_set_visible(uint32_t wv_id, uint8_t visible) {
    wv_slot *sl = wv_get_slot(wv_id);
    if (!sl) return;
    sl->desired_visible = visible ? 1 : 0;
    wv_ctrl_apply_geometry(sl);
}

void w32_webview_focus(uint32_t wv_id) {
    wv_slot *sl = wv_get_slot(wv_id);
    if (!sl) return;
    if (sl->controller) {
        typedef HRESULT (STDMETHODCALLTYPE *pfn_mf)(void *, int32_t);
        ((pfn_mf)wv_vtbl(sl->controller)[12])(sl->controller, 0); /* COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC */
    } else if (sl->host_hwnd) {
        SetFocus(sl->host_hwnd);
    }
}

void w32_webview_notify_parent_pos_changed(uint32_t wv_id) {
    wv_slot *sl = wv_get_slot(wv_id);
    if (!sl || !sl->controller) return;
    typedef HRESULT (STDMETHODCALLTYPE *pfn_notify)(void *);
    ((pfn_notify)wv_vtbl(sl->controller)[23])(sl->controller);
}

static uint8_t wv_enqueue_or_run_w1(uint32_t wv_id, int kind, uint64_t req_id, int32_t int_arg, w32_bytes s1, w32_bytes s2) {
    wv_slot *sl = wv_get_slot(wv_id);
    if (!sl || sl->failed) return 0;
    wchar_t *w1 = wv_utf8_to_wide_alloc(s1, 1);
    wchar_t *w2 = (s2.data && s2.len > 0) ? wv_utf8_to_wide_alloc(s2, 1) : NULL;
    if (!w1) {
        free(w2);
        return 0;
    }
    if (sl->ready && sl->webview) {
        wv_exec_cmd_now(sl, (int)(wv_id - 1), kind, req_id, int_arg, w1, w2);
        free(w1);
        free(w2);
    } else {
        wv_queue_cmd(sl, kind, req_id, int_arg, w1, w2);
        free(w1);
        free(w2);
    }
    return 1;
}

uint8_t w32_webview_navigate(uint32_t wv_id, w32_bytes url) {
    return wv_enqueue_or_run_w1(wv_id, W32_WV_CMD_NAVIGATE, 0, 0, url, (w32_bytes){ NULL, 0 });
}

uint8_t w32_webview_set_html(uint32_t wv_id, w32_bytes html) {
    return wv_enqueue_or_run_w1(wv_id, W32_WV_CMD_SET_HTML, 0, 0, html, (w32_bytes){ NULL, 0 });
}

uint8_t w32_webview_reload(uint32_t wv_id) {
    wv_slot *sl = wv_get_slot(wv_id);
    if (!sl || !sl->webview) return 0;
    typedef HRESULT (STDMETHODCALLTYPE *pfn)(void *);
    return SUCCEEDED(((pfn)wv_vtbl(sl->webview)[31])(sl->webview)) ? 1 : 0;
}

uint8_t w32_webview_stop(uint32_t wv_id) {
    wv_slot *sl = wv_get_slot(wv_id);
    if (!sl || !sl->webview) return 0;
    typedef HRESULT (STDMETHODCALLTYPE *pfn)(void *);
    return SUCCEEDED(((pfn)wv_vtbl(sl->webview)[43])(sl->webview)) ? 1 : 0;
}

uint8_t w32_webview_go_back(uint32_t wv_id) {
    wv_slot *sl = wv_get_slot(wv_id);
    if (!sl || !sl->webview) return 0;
    typedef HRESULT (STDMETHODCALLTYPE *pfn)(void *);
    return SUCCEEDED(((pfn)wv_vtbl(sl->webview)[40])(sl->webview)) ? 1 : 0;
}

uint8_t w32_webview_go_forward(uint32_t wv_id) {
    wv_slot *sl = wv_get_slot(wv_id);
    if (!sl || !sl->webview) return 0;
    typedef HRESULT (STDMETHODCALLTYPE *pfn)(void *);
    return SUCCEEDED(((pfn)wv_vtbl(sl->webview)[41])(sl->webview)) ? 1 : 0;
}

uint8_t w32_webview_can_go_back(uint32_t wv_id) {
    wv_slot *sl = wv_get_slot(wv_id);
    if (!sl) return 0;
    if (sl->webview) {
        typedef HRESULT (STDMETHODCALLTYPE *pfn)(void *, BOOL *);
        BOOL b = FALSE;
        if (SUCCEEDED(((pfn)wv_vtbl(sl->webview)[38])(sl->webview, &b))) {
            sl->can_go_back = b ? 1 : 0;
        }
    }
    return sl->can_go_back ? 1 : 0;
}

uint8_t w32_webview_can_go_forward(uint32_t wv_id) {
    wv_slot *sl = wv_get_slot(wv_id);
    if (!sl) return 0;
    if (sl->webview) {
        typedef HRESULT (STDMETHODCALLTYPE *pfn)(void *, BOOL *);
        BOOL b = FALSE;
        if (SUCCEEDED(((pfn)wv_vtbl(sl->webview)[39])(sl->webview, &b))) {
            sl->can_go_forward = b ? 1 : 0;
        }
    }
    return sl->can_go_forward ? 1 : 0;
}

uint8_t w32_webview_get_url(uint32_t wv_id, gs_rref out) {
    wv_slot *sl = wv_get_slot(wv_id);
    if (!sl) return 0;
    if (sl->webview) {
        typedef HRESULT (STDMETHODCALLTYPE *pfn)(void *, LPWSTR *);
        LPWSTR ws = NULL;
        if (SUCCEEDED(((pfn)wv_vtbl(sl->webview)[4])(sl->webview, &ws)) && ws) {
            uint8_t ok = w32_append_wide_as_utf8(ws, -1, out);
            CoTaskMemFree(ws);
            return ok;
        }
        if (ws) CoTaskMemFree(ws);
    }
    if (sl->cached_url && sl->cached_url[0]) {
        gs_bld_append(out, sl->cached_url, (int64_t)strlen(sl->cached_url));
        return 1;
    }
    return 0;
}

uint8_t w32_webview_get_title(uint32_t wv_id, gs_rref out) {
    wv_slot *sl = wv_get_slot(wv_id);
    if (!sl) return 0;
    if (sl->webview) {
        typedef HRESULT (STDMETHODCALLTYPE *pfn)(void *, LPWSTR *);
        LPWSTR ws = NULL;
        if (SUCCEEDED(((pfn)wv_vtbl(sl->webview)[48])(sl->webview, &ws)) && ws) {
            uint8_t ok = w32_append_wide_as_utf8(ws, -1, out);
            CoTaskMemFree(ws);
            return ok;
        }
        if (ws) CoTaskMemFree(ws);
    }
    if (sl->cached_title && sl->cached_title[0]) {
        gs_bld_append(out, sl->cached_title, (int64_t)strlen(sl->cached_title));
        return 1;
    }
    return 0;
}

uint64_t w32_webview_eval_js(uint32_t wv_id, w32_bytes script) {
    uint64_t req = g_next_script_req++;
    if (!wv_enqueue_or_run_w1(wv_id, W32_WV_CMD_EVAL_JS, req, 0, script, (w32_bytes){ NULL, 0 })) {
        return 0;
    }
    return req;
}

uint8_t w32_webview_add_init_script(uint32_t wv_id, w32_bytes script) {
    return wv_enqueue_or_run_w1(wv_id, W32_WV_CMD_ADD_INIT, 0, 0, script, (w32_bytes){ NULL, 0 });
}

uint8_t w32_webview_post_message_json(uint32_t wv_id, w32_bytes json) {
    return wv_enqueue_or_run_w1(wv_id, W32_WV_CMD_POST_JSON, 0, 0, json, (w32_bytes){ NULL, 0 });
}

uint8_t w32_webview_post_message_string(uint32_t wv_id, w32_bytes text) {
    return wv_enqueue_or_run_w1(wv_id, W32_WV_CMD_POST_STRING, 0, 0, text, (w32_bytes){ NULL, 0 });
}

uint8_t w32_webview_set_background_color(uint32_t wv_id, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    wv_slot *sl = wv_get_slot(wv_id);
    if (!sl) return 0;
    sl->bg_r = r;
    sl->bg_g = g;
    sl->bg_b = b;
    sl->bg_a = a;
    if (sl->controller) wv_apply_bg_color(sl);
    if (sl->host_hwnd) InvalidateRect(sl->host_hwnd, NULL, TRUE);
    return 1;
}

uint8_t w32_webview_set_color_scheme(uint32_t wv_id, int32_t scheme) {
    wv_slot *sl = wv_get_slot(wv_id);
    if (!sl) return 0;
    sl->color_scheme = scheme;
    if (sl->webview) wv_apply_color_scheme(sl);
    return 1;
}

uint8_t w32_webview_set_zoom(uint32_t wv_id, double factor) {
    wv_slot *sl = wv_get_slot(wv_id);
    if (!sl) return 0;
    if (factor < 0.25) factor = 0.25;
    if (factor > 5.0) factor = 5.0;
    sl->zoom_factor = factor;
    if (sl->controller) {
        typedef HRESULT (STDMETHODCALLTYPE *pfn)(void *, double);
        return SUCCEEDED(((pfn)wv_vtbl(sl->controller)[8])(sl->controller, factor)) ? 1 : 0;
    }
    return 1;
}

double w32_webview_get_zoom(uint32_t wv_id) {
    wv_slot *sl = wv_get_slot(wv_id);
    if (!sl) return 1.0;
    if (sl->controller) {
        typedef HRESULT (STDMETHODCALLTYPE *pfn)(void *, double *);
        double z = sl->zoom_factor;
        if (SUCCEEDED(((pfn)wv_vtbl(sl->controller)[7])(sl->controller, &z))) {
            sl->zoom_factor = z;
        }
    }
    return sl->zoom_factor;
}

uint8_t w32_webview_set_devtools_enabled(uint32_t wv_id, uint8_t enabled) {
    wv_slot *sl = wv_get_slot(wv_id);
    if (!sl) return 0;
    sl->devtools_enabled = enabled ? 1 : 0;
    if (sl->settings) wv_apply_settings(sl);
    return 1;
}

uint8_t w32_webview_open_devtools(uint32_t wv_id) {
    wv_slot *sl = wv_get_slot(wv_id);
    if (!sl || sl->failed) return 0;
    if (sl->ready && sl->webview) {
        wv_exec_cmd_now(sl, (int)(wv_id - 1), W32_WV_CMD_OPEN_DEVTOOLS, 0, 0, NULL, NULL);
    } else {
        wv_queue_cmd(sl, W32_WV_CMD_OPEN_DEVTOOLS, 0, 0, NULL, NULL);
    }
    return 1;
}

uint8_t w32_webview_set_context_menu_enabled(uint32_t wv_id, uint8_t enabled) {
    wv_slot *sl = wv_get_slot(wv_id);
    if (!sl) return 0;
    sl->context_menu_enabled = enabled ? 1 : 0;
    if (sl->settings) wv_apply_settings(sl);
    return 1;
}

uint8_t w32_webview_set_status_bar_enabled(uint32_t wv_id, uint8_t enabled) {
    wv_slot *sl = wv_get_slot(wv_id);
    if (!sl) return 0;
    sl->status_bar_enabled = enabled ? 1 : 0;
    if (sl->settings) wv_apply_settings(sl);
    return 1;
}

uint8_t w32_webview_set_zoom_control_enabled(uint32_t wv_id, uint8_t enabled) {
    wv_slot *sl = wv_get_slot(wv_id);
    if (!sl) return 0;
    sl->zoom_control_enabled = enabled ? 1 : 0;
    if (sl->settings) wv_apply_settings(sl);
    return 1;
}

uint8_t w32_webview_set_accelerator_keys_enabled(uint32_t wv_id, uint8_t enabled) {
    wv_slot *sl = wv_get_slot(wv_id);
    if (!sl) return 0;
    sl->accel_keys_enabled = enabled ? 1 : 0;
    if (sl->settings) wv_apply_settings(sl);
    return 1;
}

uint8_t w32_webview_set_user_agent(uint32_t wv_id, w32_bytes ua) {
    wv_slot *sl = wv_get_slot(wv_id);
    if (!sl) return 0;
    free(sl->user_agent);
    sl->user_agent = wv_utf8_to_wide_alloc(ua, 0);
    if (sl->settings) wv_apply_settings(sl);
    return 1;
}

uint8_t w32_webview_set_new_window_policy(uint32_t wv_id, int32_t policy) {
    wv_slot *sl = wv_get_slot(wv_id);
    if (!sl) return 0;
    sl->new_window_policy = policy;
    return 1;
}

uint8_t w32_webview_set_virtual_host_mapping(uint32_t wv_id, w32_bytes host_name, w32_bytes folder_path, int32_t access_kind) {
    return wv_enqueue_or_run_w1(wv_id, W32_WV_CMD_SET_VHOST, 0, access_kind, host_name, folder_path);
}

uint8_t w32_webview_clear_virtual_host_mapping(uint32_t wv_id, w32_bytes host_name) {
    return wv_enqueue_or_run_w1(wv_id, W32_WV_CMD_CLEAR_VHOST, 0, 0, host_name, (w32_bytes){ NULL, 0 });
}

uint8_t w32_webview_set_muted(uint32_t wv_id, uint8_t muted) {
    wv_slot *sl = wv_get_slot(wv_id);
    if (!sl) return 0;
    sl->muted = muted ? 1 : 0;
    if (sl->webview) wv_apply_muted(sl);
    return 1;
}

uint8_t w32_webview_is_muted(uint32_t wv_id) {
    wv_slot *sl = wv_get_slot(wv_id);
    if (!sl) return 0;
    if (sl->webview) {
        void *wv8 = NULL;
        if (SUCCEEDED(wv_qi(sl->webview, &W32_IID_ICoreWebView2_8, &wv8)) && wv8) {
            typedef HRESULT (STDMETHODCALLTYPE *pfn_get_muted)(void *, BOOL *);
            BOOL m = FALSE;
            if (SUCCEEDED(((pfn_get_muted)wv_vtbl(wv8)[83])(wv8, &m))) {
                sl->muted = m ? 1 : 0;
            }
            wv_release(wv8);
        }
    }
    return sl->muted;
}

uint8_t w32_webview_is_playing_audio(uint32_t wv_id) {
    wv_slot *sl = wv_get_slot(wv_id);
    if (!sl || !sl->webview) return 0;
    uint8_t playing = 0;
    void *wv8 = NULL;
    if (SUCCEEDED(wv_qi(sl->webview, &W32_IID_ICoreWebView2_8, &wv8)) && wv8) {
        typedef HRESULT (STDMETHODCALLTYPE *pfn_get_aud)(void *, BOOL *);
        BOOL p = FALSE;
        if (SUCCEEDED(((pfn_get_aud)wv_vtbl(wv8)[87])(wv8, &p))) {
            playing = p ? 1 : 0;
        }
        wv_release(wv8);
    }
    return playing;
}
