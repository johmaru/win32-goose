#include <stdlib.h>
#include <string.h>

#include "win32_internal.h"
#include <tlhelp32.h>

/* ---- System Info, Memory, Time, Identity, Environment -------------------- */

void w32_get_system_info(w32_system_info *out) {
    if (!out) return;
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    out->processor_count = (uint32_t)si.dwNumberOfProcessors;
    out->page_size = (uint32_t)si.dwPageSize;
    out->allocation_granularity = (uint32_t)si.dwAllocationGranularity;
    out->processor_arch = (uint16_t)si.wProcessorArchitecture;
}

uint8_t w32_get_memory_status(w32_memory_status *out) {
    if (!out) return 0;
    MEMORYSTATUSEX ms;
    memset(&ms, 0, sizeof(ms));
    ms.dwLength = sizeof(ms);
    if (!GlobalMemoryStatusEx(&ms)) return 0;
    out->memory_load = (uint32_t)ms.dwMemoryLoad;
    out->total_phys = (uint64_t)ms.ullTotalPhys;
    out->avail_phys = (uint64_t)ms.ullAvailPhys;
    out->total_page_file = (uint64_t)ms.ullTotalPageFile;
    out->avail_page_file = (uint64_t)ms.ullAvailPageFile;
    out->total_virtual = (uint64_t)ms.ullTotalVirtual;
    out->avail_virtual = (uint64_t)ms.ullAvailVirtual;
    return 1;
}

static void fill_time(const SYSTEMTIME *st, w32_system_time *out) {
    out->year = st->wYear;
    out->month = st->wMonth;
    out->day_of_week = st->wDayOfWeek;
    out->day = st->wDay;
    out->hour = st->wHour;
    out->minute = st->wMinute;
    out->second = st->wSecond;
    out->milliseconds = st->wMilliseconds;
}

void w32_get_local_time(w32_system_time *out) {
    if (!out) return;
    SYSTEMTIME st;
    GetLocalTime(&st);
    fill_time(&st, out);
}

void w32_get_system_time(w32_system_time *out) {
    if (!out) return;
    SYSTEMTIME st;
    GetSystemTime(&st);
    fill_time(&st, out);
}

uint8_t w32_get_computer_name(gs_rref out) {
    wchar_t buf[256];
    DWORD n = 256;
    if (!GetComputerNameW(buf, &n)) return 0;
    return w32_append_wide_as_utf8(buf, (int)n, out);
}

uint8_t w32_get_user_name(gs_rref out) {
    wchar_t buf[256];
    DWORD n = 256;
    if (!GetUserNameW(buf, &n) || n == 0) return 0;
    /* GetUserNameW count includes trailing NUL. */
    return w32_append_wide_as_utf8(buf, (int)(n - 1), out);
}

uint8_t w32_get_module_file_name(gs_rref out) {
    wchar_t buf[4096];
    DWORD n = GetModuleFileNameW(NULL, buf, 4096);
    if (n == 0 || n >= 4096) return 0;
    return w32_append_wide_as_utf8(buf, (int)n, out);
}

uint8_t w32_env_get(w32_bytes name, gs_rref out) {
    w32_wide_buf wname;
    if (!w32_wide_init(&wname, name) || wname.len == 0) {
        w32_wide_free(&wname);
        return 0;
    }
    SetLastError(0);
    DWORD needed = GetEnvironmentVariableW(wname.ptr, NULL, 0);
    if (needed == 0) {
        uint8_t ok = (GetLastError() == 0) ? 1 : 0;
        w32_wide_free(&wname);
        return ok;
    }
    uint8_t ok = 0;
    for (int attempt = 0; attempt < 4 && needed > 0; attempt++) {
        wchar_t *buf = (wchar_t *)malloc((size_t)needed * sizeof(wchar_t));
        if (!buf) break;
        SetLastError(0);
        DWORD got = GetEnvironmentVariableW(wname.ptr, buf, needed);
        if (got == 0) {
            ok = (GetLastError() == 0) ? 1 : 0;
            free(buf);
            break;
        }
        if (got < needed) {
            ok = w32_append_wide_as_utf8(buf, (int)got, out);
            free(buf);
            break;
        }
        free(buf);
        needed = got;
    }
    w32_wide_free(&wname);
    return ok;
}

uint8_t w32_env_set(w32_bytes name, w32_bytes value) {
    w32_wide_buf wname, wval;
    if (!w32_wide_init(&wname, name) || wname.len == 0) {
        w32_wide_free(&wname);
        return 0;
    }
    if (!w32_wide_init(&wval, value)) {
        w32_wide_free(&wname);
        return 0;
    }
    BOOL ok = SetEnvironmentVariableW(wname.ptr, wval.ptr);
    w32_wide_free(&wval);
    w32_wide_free(&wname);
    return ok ? 1 : 0;
}

uint8_t w32_env_delete(w32_bytes name) {
    w32_wide_buf wname;
    if (!w32_wide_init(&wname, name) || wname.len == 0) {
        w32_wide_free(&wname);
        return 0;
    }
    BOOL ok = SetEnvironmentVariableW(wname.ptr, NULL);
    w32_wide_free(&wname);
    return ok ? 1 : 0;
}

/* ---- Process execution & pipe capture ------------------------------------ */

uint8_t w32_run_process_capture(
    w32_bytes cmdline,
    gs_rref out_stdout,
    uint32_t *out_exit_code,
    uint32_t timeout_ms
) {
    w32_wide_buf wcmd;
    if (!w32_wide_init(&wcmd, cmdline) || wcmd.len == 0) {
        w32_wide_free(&wcmd);
        return 0;
    }
    /* CreateProcessW requires a writable command line buffer. */
    wchar_t *mutable_cmd = (wchar_t *)malloc((size_t)(wcmd.len + 1) * sizeof(wchar_t));
    if (!mutable_cmd) {
        w32_wide_free(&wcmd);
        return 0;
    }
    memcpy(mutable_cmd, wcmd.ptr, (size_t)(wcmd.len + 1) * sizeof(wchar_t));
    w32_wide_free(&wcmd);

    SECURITY_ATTRIBUTES sa;
    memset(&sa, 0, sizeof(sa));
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE read_pipe = NULL, write_pipe = NULL;
    if (!CreatePipe(&read_pipe, &write_pipe, &sa, 0)) {
        free(mutable_cmd);
        return 0;
    }
    SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = write_pipe;
    si.hStdError = write_pipe;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi;
    memset(&pi, 0, sizeof(pi));

    BOOL created = CreateProcessW(
        NULL, mutable_cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi
    );
    free(mutable_cmd);
    CloseHandle(write_pipe);
    if (!created) {
        CloseHandle(read_pipe);
        return 0;
    }

    uint8_t chunk[4096];
    uint64_t start_ms = GetTickCount64();
    int timed_out = 0;

    for (;;) {
        DWORD avail = 0;
        if (!PeekNamedPipe(read_pipe, NULL, 0, NULL, &avail, NULL)) {
            break;
        }
        if (avail > 0) {
            DWORD to_read = avail > sizeof(chunk) ? (DWORD)sizeof(chunk) : avail;
            DWORD got = 0;
            if (!ReadFile(read_pipe, chunk, to_read, &got, NULL) || got == 0) {
                break;
            }
            gs_bld_append(out_stdout, chunk, (int64_t)got);
            continue;
        }

        if (WaitForSingleObject(pi.hProcess, 0) == WAIT_OBJECT_0) {
            /* Process exited; drain any remaining buffered pipe bytes without blocking. */
            while (PeekNamedPipe(read_pipe, NULL, 0, NULL, &avail, NULL) && avail > 0) {
                DWORD to_read = avail > sizeof(chunk) ? (DWORD)sizeof(chunk) : avail;
                DWORD got = 0;
                if (!ReadFile(read_pipe, chunk, to_read, &got, NULL) || got == 0) break;
                gs_bld_append(out_stdout, chunk, (int64_t)got);
            }
            break;
        }

        DWORD wait_slice = 10;
        if (timeout_ms != INFINITE) {
            uint64_t elapsed = GetTickCount64() - start_ms;
            if (elapsed >= (uint64_t)timeout_ms) {
                timed_out = 1;
                break;
            }
            uint64_t rem = (uint64_t)timeout_ms - elapsed;
            if (rem < wait_slice) wait_slice = (DWORD)rem;
        }
        WaitForSingleObject(pi.hProcess, wait_slice);
    }
    CloseHandle(read_pipe);

    DWORD wait_r = WAIT_TIMEOUT;
    DWORD exit_code = 0;
    if (!timed_out) {
        DWORD rem_ms = INFINITE;
        if (timeout_ms != INFINITE) {
            uint64_t elapsed = GetTickCount64() - start_ms;
            rem_ms = elapsed >= (uint64_t)timeout_ms ? 0 : (DWORD)((uint64_t)timeout_ms - elapsed);
        }
        wait_r = WaitForSingleObject(pi.hProcess, rem_ms);
    }
    if (wait_r == WAIT_OBJECT_0) {
        GetExitCodeProcess(pi.hProcess, &exit_code);
    } else {
        TerminateProcess(pi.hProcess, 1);
        WaitForSingleObject(pi.hProcess, 1000);
        exit_code = 1;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    if (out_exit_code) *out_exit_code = (uint32_t)exit_code;
    return (!timed_out && wait_r == WAIT_OBJECT_0) ? 1 : 0;
}

uint64_t w32_spawn_process(w32_bytes cmdline, w32_bytes working_dir, uint8_t hide_window) {
    w32_wide_buf wcmd, wdir;
    if (!w32_wide_init(&wcmd, cmdline) || wcmd.len == 0) {
        w32_wide_free(&wcmd);
        return 0;
    }
    wchar_t *mutable_cmd = (wchar_t *)malloc((size_t)(wcmd.len + 1) * sizeof(wchar_t));
    if (!mutable_cmd) {
        w32_wide_free(&wcmd);
        return 0;
    }
    memcpy(mutable_cmd, wcmd.ptr, (size_t)(wcmd.len + 1) * sizeof(wchar_t));
    w32_wide_free(&wcmd);

    w32_wide_init(&wdir, working_dir);
    STARTUPINFOW si;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi;
    memset(&pi, 0, sizeof(pi));

    DWORD flags = hide_window ? CREATE_NO_WINDOW : 0;
    BOOL ok = CreateProcessW(
        NULL, mutable_cmd, NULL, NULL, FALSE, flags, NULL,
        wdir.len > 0 ? wdir.ptr : NULL, &si, &pi
    );
    free(mutable_cmd);
    w32_wide_free(&wdir);
    if (!ok) return 0;

    CloseHandle(pi.hThread);
    return (uint64_t)(uintptr_t)pi.hProcess;
}

uint8_t w32_get_exit_code_process(uint64_t hproc, uint32_t *out_code) {
    if (!hproc || !out_code) return 0;
    DWORD code = 0;
    if (!GetExitCodeProcess((HANDLE)(uintptr_t)hproc, &code)) return 0;
    *out_code = (uint32_t)code;
    return 1;
}

uint64_t w32_spawn_process_to_file(
    w32_bytes cmdline,
    w32_bytes working_dir,
    w32_bytes log_path,
    uint8_t append,
    uint8_t hide_window
) {
    w32_wide_buf wcmd, wdir, wlog;
    if (!w32_wide_init(&wcmd, cmdline) || wcmd.len == 0) {
        w32_wide_free(&wcmd);
        return 0;
    }
    if (!w32_wide_init(&wlog, log_path) || wlog.len == 0) {
        w32_wide_free(&wlog);
        w32_wide_free(&wcmd);
        return 0;
    }

    wchar_t *mutable_cmd = (wchar_t *)malloc((size_t)(wcmd.len + 1) * sizeof(wchar_t));
    if (!mutable_cmd) {
        w32_wide_free(&wlog);
        w32_wide_free(&wcmd);
        return 0;
    }
    memcpy(mutable_cmd, wcmd.ptr, (size_t)(wcmd.len + 1) * sizeof(wchar_t));
    w32_wide_free(&wcmd);

    SECURITY_ATTRIBUTES sa;
    memset(&sa, 0, sizeof(sa));
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    DWORD access = append ? (FILE_APPEND_DATA | SYNCHRONIZE) : GENERIC_WRITE;
    DWORD disp = append ? OPEN_ALWAYS : CREATE_ALWAYS;
    HANDLE hlog = CreateFileW(
        wlog.ptr,
        access,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        &sa,
        disp,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    w32_wide_free(&wlog);
    if (hlog == INVALID_HANDLE_VALUE) {
        free(mutable_cmd);
        return 0;
    }

    HANDLE hnul = CreateFileW(
        L"NUL",
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        &sa,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (!w32_wide_init(&wdir, working_dir)) {
        wdir.len = 0;
        wdir.ptr = wdir.stack;
    }

    STARTUPINFOW si;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hlog;
    si.hStdError = hlog;
    si.hStdInput = (hnul != INVALID_HANDLE_VALUE) ? hnul : GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi;
    memset(&pi, 0, sizeof(pi));

    DWORD flags = hide_window ? CREATE_NO_WINDOW : 0;
    BOOL ok = CreateProcessW(
        NULL, mutable_cmd, NULL, NULL, TRUE, flags, NULL,
        wdir.len > 0 ? wdir.ptr : NULL, &si, &pi
    );

    free(mutable_cmd);
    w32_wide_free(&wdir);
    CloseHandle(hlog);
    if (hnul != INVALID_HANDLE_VALUE) CloseHandle(hnul);
    if (!ok) return 0;

    CloseHandle(pi.hThread);
    return (uint64_t)(uintptr_t)pi.hProcess;
}

uint32_t w32_get_process_id(uint64_t hproc) {
    if (!hproc) return 0;
    return (uint32_t)GetProcessId((HANDLE)(uintptr_t)hproc);
}

uint8_t w32_decode_text_to_utf8(w32_bytes data, gs_rref out) {
    if (!data.data || data.len <= 0) return 1;
    const uint8_t *ptr = data.data;
    int64_t len = data.len;
    if (len >= 3 && ptr[0] == 0xEF && ptr[1] == 0xBB && ptr[2] == 0xBF) {
        ptr += 3;
        len -= 3;
    }
    if (len <= 0) return 1;
    if (len > 0x7FFFFFFFLL) len = 0x7FFFFFFFLL;

    int wn_utf8 = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, (const char *)ptr, (int)len, NULL, 0);
    if (wn_utf8 > 0) {
        gs_bld_append(out, ptr, len);
        return 1;
    }

    int wn = MultiByteToWideChar(CP_ACP, 0, (const char *)ptr, (int)len, NULL, 0);
    if (wn <= 0) {
        gs_bld_append(out, ptr, len);
        return 1;
    }
    wchar_t *wbuf = (wchar_t *)malloc((size_t)(wn + 1) * sizeof(wchar_t));
    if (!wbuf) return 0;
    MultiByteToWideChar(CP_ACP, 0, (const char *)ptr, (int)len, wbuf, wn);
    wbuf[wn] = L'\0';
    uint8_t ok = w32_append_wide_as_utf8(wbuf, wn, out);
    free(wbuf);
    return ok;
}

/* ---- Windows Registry (advapi32) ----------------------------------------- */

uint64_t w32_reg_create_key(uint64_t root, w32_bytes subkey, uint32_t access) {
    w32_wide_buf wsub;
    if (!w32_wide_init(&wsub, subkey) || wsub.len == 0) {
        w32_wide_free(&wsub);
        return 0;
    }
    HKEY hkey = NULL;
    LSTATUS st = RegCreateKeyExW(
        (HKEY)(uintptr_t)root, wsub.ptr, 0, NULL, 0, (REGSAM)access, NULL, &hkey, NULL
    );
    w32_wide_free(&wsub);
    return st == ERROR_SUCCESS ? (uint64_t)(uintptr_t)hkey : 0;
}

uint64_t w32_reg_open_key(uint64_t root, w32_bytes subkey, uint32_t access) {
    w32_wide_buf wsub;
    if (!w32_wide_init(&wsub, subkey)) return 0;
    HKEY hkey = NULL;
    LSTATUS st = RegOpenKeyExW((HKEY)(uintptr_t)root, wsub.ptr, 0, (REGSAM)access, &hkey);
    w32_wide_free(&wsub);
    return st == ERROR_SUCCESS ? (uint64_t)(uintptr_t)hkey : 0;
}

uint8_t w32_reg_delete_key(uint64_t root, w32_bytes subkey) {
    w32_wide_buf wsub;
    if (!w32_wide_init(&wsub, subkey) || wsub.len == 0) {
        w32_wide_free(&wsub);
        return 0;
    }
    LSTATUS st = RegDeleteKeyW((HKEY)(uintptr_t)root, wsub.ptr);
    w32_wide_free(&wsub);
    return st == ERROR_SUCCESS ? 1 : 0;
}

uint8_t w32_reg_close_key(uint64_t hkey) {
    if (!hkey) return 0;
    return RegCloseKey((HKEY)(uintptr_t)hkey) == ERROR_SUCCESS ? 1 : 0;
}

uint8_t w32_reg_set_string(uint64_t hkey, w32_bytes name, w32_bytes value) {
    if (!hkey) return 0;
    w32_wide_buf wname, wval;
    if (!w32_wide_init(&wname, name)) return 0;
    if (!w32_wide_init(&wval, value)) {
        w32_wide_free(&wname);
        return 0;
    }
    DWORD bytes = (DWORD)((wval.len + 1) * sizeof(wchar_t));
    LSTATUS st = RegSetValueExW(
        (HKEY)(uintptr_t)hkey, wname.len > 0 ? wname.ptr : NULL,
        0, REG_SZ, (const BYTE *)wval.ptr, bytes
    );
    w32_wide_free(&wval);
    w32_wide_free(&wname);
    return st == ERROR_SUCCESS ? 1 : 0;
}

uint8_t w32_reg_get_string(uint64_t hkey, w32_bytes name, gs_rref out) {
    if (!hkey) return 0;
    w32_wide_buf wname;
    if (!w32_wide_init(&wname, name)) return 0;
    DWORD type = 0, cb = 0;
    LSTATUS st = RegQueryValueExW(
        (HKEY)(uintptr_t)hkey, wname.len > 0 ? wname.ptr : NULL, NULL, &type, NULL, &cb
    );
    if (st != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ)) {
        w32_wide_free(&wname);
        return 0;
    }
    if (cb == 0) {
        w32_wide_free(&wname);
        return 1;
    }
    DWORD alloc_cb = cb;
    wchar_t *buf = (wchar_t *)malloc((size_t)alloc_cb + sizeof(wchar_t));
    if (!buf) {
        w32_wide_free(&wname);
        return 0;
    }
    st = RegQueryValueExW(
        (HKEY)(uintptr_t)hkey, wname.len > 0 ? wname.ptr : NULL, NULL, &type, (BYTE *)buf, &cb
    );
    uint8_t ok = 0;
    if (st == ERROR_SUCCESS && (type == REG_SZ || type == REG_EXPAND_SZ) && cb <= alloc_cb) {
        size_t wlen = cb / sizeof(wchar_t);
        buf[wlen] = L'\0';
        ok = w32_append_wide_as_utf8(buf, -1, out);
    }
    free(buf);
    w32_wide_free(&wname);
    return ok;
}

uint8_t w32_reg_set_u32(uint64_t hkey, w32_bytes name, uint32_t value) {
    if (!hkey) return 0;
    w32_wide_buf wname;
    if (!w32_wide_init(&wname, name)) return 0;
    DWORD v = (DWORD)value;
    LSTATUS st = RegSetValueExW(
        (HKEY)(uintptr_t)hkey, wname.len > 0 ? wname.ptr : NULL,
        0, REG_DWORD, (const BYTE *)&v, sizeof(v)
    );
    w32_wide_free(&wname);
    return st == ERROR_SUCCESS ? 1 : 0;
}

uint8_t w32_reg_get_u32(uint64_t hkey, w32_bytes name, uint32_t *out) {
    if (!hkey || !out) return 0;
    w32_wide_buf wname;
    if (!w32_wide_init(&wname, name)) return 0;
    DWORD type = 0, v = 0, cb = sizeof(v);
    LSTATUS st = RegQueryValueExW(
        (HKEY)(uintptr_t)hkey, wname.len > 0 ? wname.ptr : NULL, NULL, &type, (BYTE *)&v, &cb
    );
    w32_wide_free(&wname);
    if (st != ERROR_SUCCESS || type != REG_DWORD) return 0;
    *out = (uint32_t)v;
    return 1;
}

uint8_t w32_reg_set_u64(uint64_t hkey, w32_bytes name, uint64_t value) {
    if (!hkey) return 0;
    w32_wide_buf wname;
    if (!w32_wide_init(&wname, name)) return 0;
    LSTATUS st = RegSetValueExW(
        (HKEY)(uintptr_t)hkey, wname.len > 0 ? wname.ptr : NULL,
        0, REG_QWORD, (const BYTE *)&value, sizeof(value)
    );
    w32_wide_free(&wname);
    return st == ERROR_SUCCESS ? 1 : 0;
}

uint8_t w32_reg_get_u64(uint64_t hkey, w32_bytes name, uint64_t *out) {
    if (!hkey || !out) return 0;
    w32_wide_buf wname;
    if (!w32_wide_init(&wname, name)) return 0;
    DWORD type = 0, cb = sizeof(uint64_t);
    uint64_t v = 0;
    LSTATUS st = RegQueryValueExW(
        (HKEY)(uintptr_t)hkey, wname.len > 0 ? wname.ptr : NULL, NULL, &type, (BYTE *)&v, &cb
    );
    w32_wide_free(&wname);
    if (st != ERROR_SUCCESS || type != REG_QWORD) return 0;
    *out = v;
    return 1;
}

uint8_t w32_reg_set_bytes(uint64_t hkey, w32_bytes name, w32_bytes data) {
    if (!hkey) return 0;
    w32_wide_buf wname;
    if (!w32_wide_init(&wname, name)) return 0;
    LSTATUS st = RegSetValueExW(
        (HKEY)(uintptr_t)hkey, wname.len > 0 ? wname.ptr : NULL,
        0, REG_BINARY, data.data, (DWORD)(data.len > 0 ? data.len : 0)
    );
    w32_wide_free(&wname);
    return st == ERROR_SUCCESS ? 1 : 0;
}

uint8_t w32_reg_get_bytes(uint64_t hkey, w32_bytes name, gs_rref out) {
    if (!hkey) return 0;
    w32_wide_buf wname;
    if (!w32_wide_init(&wname, name)) return 0;
    DWORD type = 0, cb = 0;
    LSTATUS st = RegQueryValueExW(
        (HKEY)(uintptr_t)hkey, wname.len > 0 ? wname.ptr : NULL, NULL, &type, NULL, &cb
    );
    if (st != ERROR_SUCCESS) {
        w32_wide_free(&wname);
        return 0;
    }
    if (cb == 0) {
        w32_wide_free(&wname);
        return 1;
    }
    uint8_t *buf = (uint8_t *)malloc((size_t)cb);
    if (!buf) {
        w32_wide_free(&wname);
        return 0;
    }
    st = RegQueryValueExW(
        (HKEY)(uintptr_t)hkey, wname.len > 0 ? wname.ptr : NULL, NULL, &type, buf, &cb
    );
    uint8_t ok = 0;
    if (st == ERROR_SUCCESS) {
        gs_bld_append(out, buf, (int64_t)cb);
        ok = 1;
    }
    free(buf);
    w32_wide_free(&wname);
    return ok;
}

uint8_t w32_reg_delete_value(uint64_t hkey, w32_bytes name) {
    if (!hkey) return 0;
    w32_wide_buf wname;
    if (!w32_wide_init(&wname, name)) return 0;
    LSTATUS st = RegDeleteValueW((HKEY)(uintptr_t)hkey, wname.len > 0 ? wname.ptr : NULL);
    w32_wide_free(&wname);
    return st == ERROR_SUCCESS ? 1 : 0;
}

/* ---- Console ------------------------------------------------------------- */

uint8_t w32_enable_console_vt(void) {
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (!h || h == INVALID_HANDLE_VALUE) return 0;
    DWORD mode = 0;
    if (!GetConsoleMode(h, &mode)) return 0;
    return SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING) ? 1 : 0;
}

uint8_t w32_set_console_title(w32_bytes title) {
    w32_wide_buf wb;
    if (!w32_wide_init(&wb, title)) return 0;
    BOOL ok = SetConsoleTitleW(wb.ptr);
    w32_wide_free(&wb);
    return ok ? 1 : 0;
}

uint8_t w32_get_console_size(int32_t *out_cols, int32_t *out_rows) {
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (!h || h == INVALID_HANDLE_VALUE) return 0;
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (!GetConsoleScreenBufferInfo(h, &info)) return 0;
    if (out_cols) *out_cols = (int32_t)(info.srWindow.Right - info.srWindow.Left + 1);
    if (out_rows) *out_rows = (int32_t)(info.srWindow.Bottom - info.srWindow.Top + 1);
    return 1;
}

/* ---- Process Enumeration (Toolhelp32) ------------------------------------ */

uint64_t w32_process_snapshot_open(void) {
    HANDLE h = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (h == INVALID_HANDLE_VALUE) return (uint64_t)(uintptr_t)INVALID_HANDLE_VALUE;
    return (uint64_t)(uintptr_t)h;
}

uint8_t w32_process_snapshot_next(uint64_t hsnap, uint8_t first, w32_process_entry *out, gs_rref out_exe_name) {
    HANDLE h = (HANDLE)(uintptr_t)hsnap;
    if (!h || h == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe;
    memset(&pe, 0, sizeof(pe));
    pe.dwSize = sizeof(pe);
    BOOL ok = first ? Process32FirstW(h, &pe) : Process32NextW(h, &pe);
    if (!ok) return 0;
    if (out) {
        out->pid = (uint32_t)pe.th32ProcessID;
        out->parent_pid = (uint32_t)pe.th32ParentProcessID;
        out->thread_count = (uint32_t)pe.cntThreads;
        out->base_priority = (int32_t)pe.pcPriClassBase;
    }
    w32_append_wide_as_utf8(pe.szExeFile, -1, out_exe_name);
    return 1;
}

/* ---- Dynamic DLL Loading ------------------------------------------------- */

uint64_t w32_load_library(w32_bytes path) {
    w32_wide_buf wb;
    if (!w32_wide_init(&wb, path) || wb.len == 0) {
        w32_wide_free(&wb);
        return 0;
    }
    HMODULE hm = LoadLibraryW(wb.ptr);
    w32_wide_free(&wb);
    return (uint64_t)(uintptr_t)hm;
}

uint64_t w32_get_proc_address(uint64_t hmod, w32_bytes proc_name) {
    if (!hmod || !proc_name.data || proc_name.len <= 0) return 0;
    char *name = (char *)malloc((size_t)proc_name.len + 1);
    if (!name) return 0;
    memcpy(name, proc_name.data, (size_t)proc_name.len);
    name[proc_name.len] = '\0';
    FARPROC p = GetProcAddress((HMODULE)(uintptr_t)hmod, name);
    free(name);
    return (uint64_t)(uintptr_t)p;
}

/* ---- Virtual Memory ------------------------------------------------------ */

uint64_t w32_virtual_alloc(uint64_t size, uint32_t alloc_type, uint32_t protect) {
    if (size == 0) return 0;
    void *p = VirtualAlloc(NULL, (SIZE_T)size, (DWORD)alloc_type, (DWORD)protect);
    return (uint64_t)(uintptr_t)p;
}

uint8_t w32_virtual_protect(uint64_t addr, uint64_t size, uint32_t new_protect, uint32_t *out_old_protect) {
    if (!addr || size == 0) return 0;
    DWORD old_prot = 0;
    BOOL ok = VirtualProtect((LPVOID)(uintptr_t)addr, (SIZE_T)size, (DWORD)new_protect, &old_prot);
    if (ok && out_old_protect) *out_old_protect = (uint32_t)old_prot;
    return ok ? 1 : 0;
}

uint8_t w32_virtual_free(uint64_t addr) {
    if (!addr) return 0;
    return VirtualFree((LPVOID)(uintptr_t)addr, 0, MEM_RELEASE) ? 1 : 0;
}

/* ---- Power & Sleep Control ----------------------------------------------- */

uint8_t w32_get_power_status(w32_power_status *out) {
    if (!out) return 0;
    SYSTEM_POWER_STATUS sps;
    if (!GetSystemPowerStatus(&sps)) return 0;
    out->ac_line_status = sps.ACLineStatus;
    out->battery_flag = sps.BatteryFlag;
    out->battery_life_percent = sps.BatteryLifePercent;
    out->power_saver = sps.SystemStatusFlag;
    out->battery_life_sec = (uint32_t)sps.BatteryLifeTime;
    out->battery_full_life_sec = (uint32_t)sps.BatteryFullLifeTime;
    return 1;
}

uint32_t w32_set_thread_execution_state(uint32_t flags) {
    return (uint32_t)SetThreadExecutionState((EXECUTION_STATE)flags);
}

/* ---- Named Pipes IPC ----------------------------------------------------- */

uint64_t w32_named_pipe_create(
    w32_bytes name,
    uint32_t max_instances,
    uint32_t out_buf_size,
    uint32_t in_buf_size,
    uint8_t nonblocking
) {
    w32_wide_buf wb;
    if (!w32_wide_init(&wb, name) || wb.len == 0) {
        w32_wide_free(&wb);
        return (uint64_t)(uintptr_t)INVALID_HANDLE_VALUE;
    }
    DWORD mode = PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | (nonblocking ? PIPE_NOWAIT : PIPE_WAIT);
    DWORD inst = max_instances == 0 ? PIPE_UNLIMITED_INSTANCES : (DWORD)max_instances;
    HANDLE h = CreateNamedPipeW(
        wb.ptr,
        PIPE_ACCESS_DUPLEX,
        mode,
        inst,
        (DWORD)(out_buf_size ? out_buf_size : 4096),
        (DWORD)(in_buf_size ? in_buf_size : 4096),
        0,
        NULL
    );
    w32_wide_free(&wb);
    return (uint64_t)(uintptr_t)h;
}

uint64_t w32_named_pipe_open(w32_bytes name, uint32_t timeout_ms) {
    w32_wide_buf wb;
    if (!w32_wide_init(&wb, name) || wb.len == 0) {
        w32_wide_free(&wb);
        return (uint64_t)(uintptr_t)INVALID_HANDLE_VALUE;
    }
    HANDLE h = CreateFileW(
        wb.ptr,
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );
    if (h == INVALID_HANDLE_VALUE && GetLastError() == ERROR_PIPE_BUSY && timeout_ms > 0) {
        if (WaitNamedPipeW(wb.ptr, (DWORD)timeout_ms)) {
            h = CreateFileW(wb.ptr, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        }
    }
    w32_wide_free(&wb);
    return (uint64_t)(uintptr_t)h;
}
