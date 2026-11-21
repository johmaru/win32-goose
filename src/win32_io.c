#include <stdlib.h>
#include <string.h>

#include "win32_internal.h"

/* ---- Clipboard helpers --------------------------------------------------- */

static int w32_open_clipboard_retry(HWND owner) {
    /* Clipboard can be momentarily locked by another process (e.g. clipboard history). */
    for (int attempt = 0; attempt < 10; attempt++) {
        if (OpenClipboard(owner)) return 1;
        Sleep(5);
    }
    return 0;
}

uint8_t w32_clipboard_set_text(uint64_t owner, w32_bytes text) {
    w32_wide_buf wb;
    if (!w32_wide_init(&wb, text)) return 0;

    size_t bytes = (size_t)(wb.len + 1) * sizeof(wchar_t);
    HGLOBAL hmem = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!hmem) {
        w32_wide_free(&wb);
        return 0;
    }

    void *dst = GlobalLock(hmem);
    if (!dst) {
        GlobalFree(hmem);
        w32_wide_free(&wb);
        return 0;
    }
    memcpy(dst, wb.ptr, bytes);
    GlobalUnlock(hmem);
    w32_wide_free(&wb);

    if (!w32_open_clipboard_retry((HWND)(uintptr_t)owner)) {
        GlobalFree(hmem);
        return 0;
    }
    EmptyClipboard();
    if (!SetClipboardData(CF_UNICODETEXT, hmem)) {
        CloseClipboard();
        GlobalFree(hmem);
        return 0;
    }
    /* System owns hmem after SetClipboardData succeeds. */
    CloseClipboard();
    return 1;
}

uint8_t w32_clipboard_get_text(uint64_t owner, gs_rref out) {
    if (!IsClipboardFormatAvailable(CF_UNICODETEXT)) return 0;
    if (!w32_open_clipboard_retry((HWND)(uintptr_t)owner)) return 0;

    uint8_t ok = 0;
    HANDLE hdata = GetClipboardData(CF_UNICODETEXT);
    if (hdata) {
        const wchar_t *ws = (const wchar_t *)GlobalLock(hdata);
        if (ws) {
            ok = w32_append_wide_as_utf8(ws, -1, out);
            GlobalUnlock(hdata);
        }
    }
    CloseClipboard();
    return ok;
}

uint8_t w32_clipboard_set_bytes(uint64_t owner, uint32_t format, w32_bytes data) {
    if (format == 0) return 0;
    size_t n = data.len > 0 ? (size_t)data.len : 0;
    /* GlobalAlloc(0) is valid but allocating at least 1 byte avoids null lock ambiguity. */
    HGLOBAL hmem = GlobalAlloc(GMEM_MOVEABLE, n > 0 ? n : 1);
    if (!hmem) return 0;

    void *dst = GlobalLock(hmem);
    if (!dst) {
        GlobalFree(hmem);
        return 0;
    }
    if (n > 0 && data.data) {
        memcpy(dst, data.data, n);
    } else {
        ((uint8_t *)dst)[0] = 0;
    }
    GlobalUnlock(hmem);

    if (!w32_open_clipboard_retry((HWND)(uintptr_t)owner)) {
        GlobalFree(hmem);
        return 0;
    }
    EmptyClipboard();
    if (!SetClipboardData((UINT)format, hmem)) {
        CloseClipboard();
        GlobalFree(hmem);
        return 0;
    }
    CloseClipboard();
    return 1;
}

uint8_t w32_clipboard_get_bytes(uint64_t owner, uint32_t format, gs_rref out) {
    if (format == 0 || !IsClipboardFormatAvailable((UINT)format)) return 0;
    if (!w32_open_clipboard_retry((HWND)(uintptr_t)owner)) return 0;

    uint8_t ok = 0;
    HANDLE hdata = GetClipboardData((UINT)format);
    if (hdata) {
        SIZE_T sz = GlobalSize(hdata);
        const void *src = GlobalLock(hdata);
        if (src) {
            if (sz > 0) gs_bld_append(out, src, (int64_t)sz);
            GlobalUnlock(hdata);
            ok = 1;
        }
    }
    CloseClipboard();
    return ok;
}

uint32_t w32_register_clipboard_format(w32_bytes name) {
    w32_wide_buf wb;
    if (!w32_wide_init(&wb, name) || wb.len == 0) {
        w32_wide_free(&wb);
        return 0;
    }
    UINT fmt = RegisterClipboardFormatW(wb.ptr);
    w32_wide_free(&wb);
    return (uint32_t)fmt;
}

/* ---- Win32 File I/O helpers ---------------------------------------------- */

static uint64_t w32_filetime_u64(FILETIME ft) {
    return ((uint64_t)ft.dwHighDateTime << 32) | (uint64_t)ft.dwLowDateTime;
}

static void w32_fill_find_info(const WIN32_FIND_DATAW *fd, w32_file_info *out) {
    if (!out) return;
    out->size = (int64_t)(((uint64_t)fd->nFileSizeHigh << 32) | (uint64_t)fd->nFileSizeLow);
    out->attributes = (uint32_t)fd->dwFileAttributes;
    out->created_ft = w32_filetime_u64(fd->ftCreationTime);
    out->modified_ft = w32_filetime_u64(fd->ftLastWriteTime);
}

uint64_t w32_create_file(
    w32_bytes path,
    uint32_t desired_access,
    uint32_t share_mode,
    uint32_t creation_disposition,
    uint32_t flags_and_attributes
) {
    w32_wide_buf wb;
    if (!w32_wide_init(&wb, path) || wb.len == 0) {
        w32_wide_free(&wb);
        return 0;
    }
    HANDLE h = CreateFileW(
        wb.ptr,
        (DWORD)desired_access,
        (DWORD)share_mode,
        NULL,
        (DWORD)creation_disposition,
        (DWORD)flags_and_attributes,
        NULL
    );
    w32_wide_free(&wb);
    if (h == INVALID_HANDLE_VALUE) return 0;
    return (uint64_t)(uintptr_t)h;
}

uint8_t w32_get_file_info_by_handle(uint64_t hfile, w32_file_info *out) {
    if (!hfile || !out) return 0;
    BY_HANDLE_FILE_INFORMATION info;
    if (!GetFileInformationByHandle((HANDLE)(uintptr_t)hfile, &info)) return 0;
    out->size = (int64_t)(((uint64_t)info.nFileSizeHigh << 32) | (uint64_t)info.nFileSizeLow);
    out->attributes = (uint32_t)info.dwFileAttributes;
    out->created_ft = w32_filetime_u64(info.ftCreationTime);
    out->modified_ft = w32_filetime_u64(info.ftLastWriteTime);
    return 1;
}

uint32_t w32_get_file_attributes(w32_bytes path) {
    w32_wide_buf wb;
    if (!w32_wide_init(&wb, path) || wb.len == 0) {
        w32_wide_free(&wb);
        return INVALID_FILE_ATTRIBUTES;
    }
    DWORD attrs = GetFileAttributesW(wb.ptr);
    w32_wide_free(&wb);
    return (uint32_t)attrs;
}

uint8_t w32_delete_file(w32_bytes path) {
    w32_wide_buf wb;
    if (!w32_wide_init(&wb, path) || wb.len == 0) {
        w32_wide_free(&wb);
        return 0;
    }
    BOOL ok = DeleteFileW(wb.ptr);
    w32_wide_free(&wb);
    return ok ? 1 : 0;
}

uint8_t w32_move_file(w32_bytes from, w32_bytes to, uint8_t replace_existing) {
    w32_wide_buf wfrom, wto;
    if (!w32_wide_init(&wfrom, from) || wfrom.len == 0) {
        w32_wide_free(&wfrom);
        return 0;
    }
    if (!w32_wide_init(&wto, to) || wto.len == 0) {
        w32_wide_free(&wto);
        w32_wide_free(&wfrom);
        return 0;
    }
    DWORD flags = MOVEFILE_COPY_ALLOWED | (replace_existing ? MOVEFILE_REPLACE_EXISTING : 0);
    BOOL ok = MoveFileExW(wfrom.ptr, wto.ptr, flags);
    w32_wide_free(&wto);
    w32_wide_free(&wfrom);
    return ok ? 1 : 0;
}

uint8_t w32_copy_file(w32_bytes from, w32_bytes to, uint8_t fail_if_exists) {
    w32_wide_buf wfrom, wto;
    if (!w32_wide_init(&wfrom, from) || wfrom.len == 0) {
        w32_wide_free(&wfrom);
        return 0;
    }
    if (!w32_wide_init(&wto, to) || wto.len == 0) {
        w32_wide_free(&wto);
        w32_wide_free(&wfrom);
        return 0;
    }
    BOOL ok = CopyFileW(wfrom.ptr, wto.ptr, fail_if_exists ? TRUE : FALSE);
    w32_wide_free(&wto);
    w32_wide_free(&wfrom);
    return ok ? 1 : 0;
}

uint8_t w32_create_directory(w32_bytes path) {
    w32_wide_buf wb;
    if (!w32_wide_init(&wb, path) || wb.len == 0) {
        w32_wide_free(&wb);
        return 0;
    }
    BOOL ok = CreateDirectoryW(wb.ptr, NULL);
    w32_wide_free(&wb);
    return ok ? 1 : 0;
}

uint8_t w32_remove_directory(w32_bytes path) {
    w32_wide_buf wb;
    if (!w32_wide_init(&wb, path) || wb.len == 0) {
        w32_wide_free(&wb);
        return 0;
    }
    BOOL ok = RemoveDirectoryW(wb.ptr);
    w32_wide_free(&wb);
    return ok ? 1 : 0;
}

uint8_t w32_get_temp_path(gs_rref out) {
    DWORD cap = GetTempPathW(0, NULL);
    for (int attempt = 0; attempt < 4 && cap > 0; attempt++) {
        wchar_t *buf = (wchar_t *)malloc((size_t)(cap + 1) * sizeof(wchar_t));
        if (!buf) return 0;
        DWORD len = GetTempPathW(cap + 1, buf);
        if (len == 0) {
            free(buf);
            return 0;
        }
        if (len < cap + 1) {
            uint8_t ok = w32_append_wide_as_utf8(buf, (int)len, out);
            free(buf);
            return ok;
        }
        free(buf);
        cap = len;
    }
    return 0;
}

uint8_t w32_get_current_directory(gs_rref out) {
    DWORD cap = GetCurrentDirectoryW(0, NULL);
    for (int attempt = 0; attempt < 4 && cap > 0; attempt++) {
        wchar_t *buf = (wchar_t *)malloc((size_t)(cap + 1) * sizeof(wchar_t));
        if (!buf) return 0;
        DWORD len = GetCurrentDirectoryW(cap + 1, buf);
        if (len == 0) {
            free(buf);
            return 0;
        }
        if (len < cap + 1) {
            uint8_t ok = w32_append_wide_as_utf8(buf, (int)len, out);
            free(buf);
            return ok;
        }
        free(buf);
        cap = len;
    }
    return 0;
}

uint8_t w32_get_full_path_name(w32_bytes path, gs_rref out) {
    w32_wide_buf wb;
    if (!w32_wide_init(&wb, path) || wb.len == 0) {
        w32_wide_free(&wb);
        return 0;
    }
    DWORD cap = GetFullPathNameW(wb.ptr, 0, NULL, NULL);
    uint8_t ok = 0;
    for (int attempt = 0; attempt < 4 && cap > 0; attempt++) {
        wchar_t *buf = (wchar_t *)malloc((size_t)(cap + 1) * sizeof(wchar_t));
        if (!buf) break;
        DWORD len = GetFullPathNameW(wb.ptr, cap + 1, buf, NULL);
        if (len == 0) {
            free(buf);
            break;
        }
        if (len < cap + 1) {
            ok = w32_append_wide_as_utf8(buf, (int)len, out);
            free(buf);
            break;
        }
        free(buf);
        cap = len;
    }
    w32_wide_free(&wb);
    return ok;
}

uint64_t w32_find_first_file(w32_bytes pattern, gs_rref out_name, w32_file_info *out_info) {
    w32_wide_buf wb;
    if (!w32_wide_init(&wb, pattern) || wb.len == 0) {
        w32_wide_free(&wb);
        return 0;
    }
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(wb.ptr, &fd);
    w32_wide_free(&wb);
    if (h == INVALID_HANDLE_VALUE) return 0;
    w32_append_wide_as_utf8(fd.cFileName, -1, out_name);
    w32_fill_find_info(&fd, out_info);
    return (uint64_t)(uintptr_t)h;
}

uint8_t w32_find_next_file(uint64_t hfind, gs_rref out_name, w32_file_info *out_info) {
    if (!hfind) return 0;
    WIN32_FIND_DATAW fd;
    if (!FindNextFileW((HANDLE)(uintptr_t)hfind, &fd)) return 0;
    w32_append_wide_as_utf8(fd.cFileName, -1, out_name);
    w32_fill_find_info(&fd, out_info);
    return 1;
}

uint8_t w32_read_file_all(w32_bytes path, gs_rref out) {
    uint64_t h_u64 = w32_create_file(
        path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN
    );
    if (!h_u64) return 0;
    HANDLE h = (HANDLE)(uintptr_t)h_u64;

    uint8_t chunk[16384];
    DWORD got = 0;
    uint8_t ok = 1;
    for (;;) {
        if (!ReadFile(h, chunk, (DWORD)sizeof(chunk), &got, NULL)) {
            ok = 0;
            break;
        }
        if (got == 0) break;
        gs_bld_append(out, chunk, (int64_t)got);
    }
    CloseHandle(h);
    return ok;
}

uint8_t w32_write_file_all(w32_bytes path, w32_bytes data) {
    uint64_t h_u64 = w32_create_file(
        path, GENERIC_WRITE, 0,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL
    );
    if (!h_u64) return 0;
    HANDLE h = (HANDLE)(uintptr_t)h_u64;

    int64_t offset = 0;
    uint8_t ok = 1;
    while (offset < data.len) {
        int64_t rem = data.len - offset;
        DWORD to_write = rem > 0x40000000LL ? 0x40000000u : (DWORD)rem;
        DWORD written = 0;
        if (!WriteFile(h, data.data + offset, to_write, &written, NULL) || written == 0) {
            ok = 0;
            break;
        }
        offset += (int64_t)written;
    }
    CloseHandle(h);
    return ok;
}

/* ---- Storage extensions: Memory-mapped files, Disk/Volume, Dir watch ----- */

#define W32_MAX_FILE_MAPS 64

typedef struct {
    int used;
    HANDLE mapping;
    uint8_t *view;
    int64_t size;
    uint8_t writable;
} w32_file_map;

static w32_file_map g_file_maps[W32_MAX_FILE_MAPS];

static w32_file_map *get_file_map(uint32_t id) {
    if (id == 0 || id > W32_MAX_FILE_MAPS) return NULL;
    return g_file_maps[id - 1].used ? &g_file_maps[id - 1] : NULL;
}

uint32_t w32_file_map_open(uint64_t hfile, uint8_t writable, uint64_t max_size) {
    if (!hfile || hfile == (uint64_t)(uintptr_t)INVALID_HANDLE_VALUE) return 0;
    int slot = -1;
    for (int i = 0; i < W32_MAX_FILE_MAPS; i++) {
        if (!g_file_maps[i].used) { slot = i; break; }
    }
    if (slot < 0) return 0;

    HANDLE hf = (HANDLE)(uintptr_t)hfile;
    LARGE_INTEGER fsz;
    fsz.QuadPart = 0;
    if (!GetFileSizeEx(hf, &fsz) && max_size == 0) return 0;
    uint64_t map_len = max_size > 0 ? max_size : (uint64_t)(fsz.QuadPart > 0 ? fsz.QuadPart : 0);
    if (map_len == 0 || map_len > (uint64_t)INT64_MAX) return 0;

    DWORD protect = writable ? PAGE_READWRITE : PAGE_READONLY;
    DWORD access = writable ? FILE_MAP_ALL_ACCESS : FILE_MAP_READ;
    HANDLE hmap = CreateFileMappingW(
        hf, NULL, protect,
        (DWORD)(map_len >> 32), (DWORD)(map_len & 0xFFFFFFFFu),
        NULL
    );
    if (!hmap) return 0;

    void *view = MapViewOfFile(hmap, access, 0, 0, (SIZE_T)map_len);
    if (!view) {
        CloseHandle(hmap);
        return 0;
    }

    w32_file_map *m = &g_file_maps[slot];
    m->used = 1;
    m->mapping = hmap;
    m->view = (uint8_t *)view;
    m->size = (int64_t)map_len;
    m->writable = writable ? 1 : 0;
    return (uint32_t)(slot + 1);
}

int64_t w32_file_map_size(uint32_t map_id) {
    w32_file_map *m = get_file_map(map_id);
    return m ? m->size : 0;
}

int64_t w32_file_map_read(uint32_t map_id, int64_t offset, w32_mut_bytes out) {
    w32_file_map *m = get_file_map(map_id);
    if (!m || !out.data || offset < 0 || out.len <= 0 || offset >= m->size) return 0;
    int64_t avail = m->size - offset;
    int64_t n = out.len < avail ? out.len : avail;
    memcpy(out.data, m->view + offset, (size_t)n);
    return n;
}

int64_t w32_file_map_write(uint32_t map_id, int64_t offset, w32_bytes data) {
    w32_file_map *m = get_file_map(map_id);
    if (!m || !m->writable || !data.data || offset < 0 || data.len <= 0 || offset >= m->size) return 0;
    int64_t avail = m->size - offset;
    int64_t n = data.len < avail ? data.len : avail;
    memcpy(m->view + offset, data.data, (size_t)n);
    return n;
}

uint8_t w32_file_map_flush(uint32_t map_id, int64_t offset, int64_t len) {
    w32_file_map *m = get_file_map(map_id);
    if (!m || offset < 0 || offset >= m->size) return 0;
    int64_t rem = m->size - offset;
    SIZE_T n = (len > 0 && len <= rem) ? (SIZE_T)len : (SIZE_T)rem;
    return FlushViewOfFile(m->view + offset, n) ? 1 : 0;
}

void w32_file_map_close(uint32_t map_id) {
    w32_file_map *m = get_file_map(map_id);
    if (!m) return;
    if (m->view) UnmapViewOfFile(m->view);
    if (m->mapping) CloseHandle(m->mapping);
    memset(m, 0, sizeof(*m));
}

uint8_t w32_get_disk_free_space(w32_bytes path, w32_disk_space *out) {
    if (!out) return 0;
    w32_wide_buf wb;
    const wchar_t *wpath = NULL;
    if (path.data && path.len > 0) {
        if (!w32_wide_init(&wb, path)) return 0;
        wpath = wb.ptr;
    } else {
        wb.ptr = NULL;
        wb.len = 0;
    }
    ULARGE_INTEGER avail, total, free_b;
    BOOL ok = GetDiskFreeSpaceExW(wpath, &avail, &total, &free_b);
    if (wpath) w32_wide_free(&wb);
    if (!ok) return 0;
    out->avail_bytes = avail.QuadPart;
    out->total_bytes = total.QuadPart;
    out->free_bytes = free_b.QuadPart;
    return 1;
}

uint8_t w32_get_volume_info(w32_bytes root_path, gs_rref out_label, gs_rref out_fs, uint32_t *out_serial) {
    w32_wide_buf wb;
    const wchar_t *wroot = NULL;
    if (root_path.data && root_path.len > 0) {
        if (!w32_wide_init(&wb, root_path)) return 0;
        wroot = wb.ptr;
    } else {
        wb.ptr = NULL;
        wb.len = 0;
    }
    wchar_t label[MAX_PATH + 1];
    wchar_t fs[MAX_PATH + 1];
    DWORD serial = 0, max_comp = 0, flags = 0;
    BOOL ok = GetVolumeInformationW(wroot, label, MAX_PATH + 1, &serial, &max_comp, &flags, fs, MAX_PATH + 1);
    if (wroot) w32_wide_free(&wb);
    if (!ok) return 0;
    w32_append_wide_as_utf8(label, -1, out_label);
    w32_append_wide_as_utf8(fs, -1, out_fs);
    if (out_serial) *out_serial = (uint32_t)serial;
    return 1;
}

uint64_t w32_watch_dir_open(w32_bytes path, uint8_t watch_subtree, uint32_t filter) {
    w32_wide_buf wb;
    if (!w32_wide_init(&wb, path) || wb.len == 0) {
        w32_wide_free(&wb);
        return 0;
    }
    HANDLE h = FindFirstChangeNotificationW(wb.ptr, watch_subtree ? TRUE : FALSE, (DWORD)filter);
    w32_wide_free(&wb);
    if (h == INVALID_HANDLE_VALUE) return 0;
    return (uint64_t)(uintptr_t)h;
}
