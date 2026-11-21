#include <string.h>

#include "win32_internal.h"
#include <dwmapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <knownfolders.h>

/* ---- DWM window visual styling ------------------------------------------- */

uint8_t w32_dwm_set_dark_titlebar(uint64_t hwnd_u64, uint8_t enabled) {
    HWND hwnd = (HWND)(uintptr_t)hwnd_u64;
    if (!hwnd) return 0;
    BOOL val = enabled ? TRUE : FALSE;
    /* DWMWA_USE_IMMERSIVE_DARK_MODE == 20 (Windows 10 20H1+ / Windows 11). */
    HRESULT hr = DwmSetWindowAttribute(hwnd, 20, &val, sizeof(val));
    if (FAILED(hr)) {
        /* Older Windows 10 builds used attribute 19. */
        hr = DwmSetWindowAttribute(hwnd, 19, &val, sizeof(val));
    }
    return SUCCEEDED(hr) ? 1 : 0;
}

uint8_t w32_dwm_set_corner_preference(uint64_t hwnd_u64, uint32_t pref) {
    HWND hwnd = (HWND)(uintptr_t)hwnd_u64;
    if (!hwnd) return 0;
    DWORD v = (DWORD)pref;
    /* DWMWA_WINDOW_CORNER_PREFERENCE == 33 (Windows 11). */
    return SUCCEEDED(DwmSetWindowAttribute(hwnd, 33, &v, sizeof(v))) ? 1 : 0;
}

uint8_t w32_dwm_set_backdrop(uint64_t hwnd_u64, uint32_t backdrop) {
    HWND hwnd = (HWND)(uintptr_t)hwnd_u64;
    if (!hwnd) return 0;
    DWORD v = (DWORD)backdrop;
    /* DWMWA_SYSTEMBACKDROP_TYPE == 38 (Windows 11 22H2+). */
    return SUCCEEDED(DwmSetWindowAttribute(hwnd, 38, &v, sizeof(v))) ? 1 : 0;
}

uint8_t w32_dwm_flush(void) {
    return SUCCEEDED(DwmFlush()) ? 1 : 0;
}

/* ---- Known Folders & ShellExecute ---------------------------------------- */

uint8_t w32_known_folder_path(int32_t kind, gs_rref out) {
    const KNOWNFOLDERID *rfid = NULL;
    switch (kind) {
        case 0: rfid = &FOLDERID_Desktop; break;
        case 1: rfid = &FOLDERID_Documents; break;
        case 2: rfid = &FOLDERID_Downloads; break;
        case 3: rfid = &FOLDERID_RoamingAppData; break;
        case 4: rfid = &FOLDERID_LocalAppData; break;
        case 5: rfid = &FOLDERID_Pictures; break;
        case 6: rfid = &FOLDERID_Music; break;
        case 7: rfid = &FOLDERID_Videos; break;
        case 8: rfid = &FOLDERID_Profile; break;
        default: return 0;
    }

    PWSTR path = NULL;
    HRESULT hr = SHGetKnownFolderPath(rfid, 0, NULL, &path);
    if (FAILED(hr) || !path) return 0;
    uint8_t ok = w32_append_wide_as_utf8(path, -1, out);
    CoTaskMemFree(path);
    return ok;
}

uint8_t w32_shell_open(w32_bytes target, w32_bytes params, w32_bytes verb, int32_t show_cmd) {
    w32_wide_buf wtgt, wpar, wverb;
    if (!w32_wide_init(&wtgt, target) || wtgt.len == 0) {
        w32_wide_free(&wtgt);
        return 0;
    }
    w32_wide_init(&wpar, params);
    w32_wide_init(&wverb, verb);

    HINSTANCE res = ShellExecuteW(
        NULL,
        wverb.len > 0 ? wverb.ptr : L"open",
        wtgt.ptr,
        wpar.len > 0 ? wpar.ptr : NULL,
        NULL,
        (int)show_cmd
    );
    w32_wide_free(&wverb);
    w32_wide_free(&wpar);
    w32_wide_free(&wtgt);
    return (uintptr_t)res > 32 ? 1 : 0;
}

/* ---- System Tray Icon (Shell_NotifyIconW) -------------------------------- */

static void copy_wide_clamped(wchar_t *dst, size_t dst_cap, const wchar_t *src, int src_len) {
    if (dst_cap == 0) return;
    size_t n = src_len > 0 ? (size_t)src_len : 0;
    if (n >= dst_cap) n = dst_cap - 1;
    if (n > 0 && src) memcpy(dst, src, n * sizeof(wchar_t));
    dst[n] = L'\0';
}

uint8_t w32_tray_add(uint64_t hwnd_u64, uint32_t id, w32_bytes tooltip) {
    HWND hwnd = (HWND)(uintptr_t)hwnd_u64;
    if (!hwnd) return 0;
    w32_wide_buf wtip;
    w32_wide_init(&wtip, tooltip);

    NOTIFYICONDATAW nid;
    memset(&nid, 0, sizeof(nid));
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = (UINT)id;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid.uCallbackMessage = WM_APP + 1;
    nid.hIcon = LoadIconW(NULL, MAKEINTRESOURCEW(32512)); /* IDI_APPLICATION */
    copy_wide_clamped(nid.szTip, sizeof(nid.szTip) / sizeof(wchar_t), wtip.ptr, wtip.len);

    BOOL ok = Shell_NotifyIconW(NIM_ADD, &nid);
    w32_wide_free(&wtip);
    return ok ? 1 : 0;
}

uint8_t w32_tray_set_tooltip(uint64_t hwnd_u64, uint32_t id, w32_bytes tooltip) {
    HWND hwnd = (HWND)(uintptr_t)hwnd_u64;
    if (!hwnd) return 0;
    w32_wide_buf wtip;
    w32_wide_init(&wtip, tooltip);

    NOTIFYICONDATAW nid;
    memset(&nid, 0, sizeof(nid));
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = (UINT)id;
    nid.uFlags = NIF_TIP;
    copy_wide_clamped(nid.szTip, sizeof(nid.szTip) / sizeof(wchar_t), wtip.ptr, wtip.len);

    BOOL ok = Shell_NotifyIconW(NIM_MODIFY, &nid);
    w32_wide_free(&wtip);
    return ok ? 1 : 0;
}

uint8_t w32_tray_balloon(uint64_t hwnd_u64, uint32_t id, w32_bytes title, w32_bytes message, uint32_t info_flags) {
    HWND hwnd = (HWND)(uintptr_t)hwnd_u64;
    if (!hwnd) return 0;
    w32_wide_buf wtitle, wmsg;
    w32_wide_init(&wtitle, title);
    w32_wide_init(&wmsg, message);

    NOTIFYICONDATAW nid;
    memset(&nid, 0, sizeof(nid));
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = (UINT)id;
    nid.uFlags = NIF_INFO;
    nid.dwInfoFlags = (DWORD)info_flags;
    copy_wide_clamped(nid.szInfoTitle, sizeof(nid.szInfoTitle) / sizeof(wchar_t), wtitle.ptr, wtitle.len);
    copy_wide_clamped(nid.szInfo, sizeof(nid.szInfo) / sizeof(wchar_t), wmsg.ptr, wmsg.len);

    BOOL ok = Shell_NotifyIconW(NIM_MODIFY, &nid);
    w32_wide_free(&wmsg);
    w32_wide_free(&wtitle);
    return ok ? 1 : 0;
}

uint8_t w32_tray_remove(uint64_t hwnd_u64, uint32_t id) {
    HWND hwnd = (HWND)(uintptr_t)hwnd_u64;
    if (!hwnd) return 0;
    NOTIFYICONDATAW nid;
    memset(&nid, 0, sizeof(nid));
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = (UINT)id;
    return Shell_NotifyIconW(NIM_DELETE, &nid) ? 1 : 0;
}
