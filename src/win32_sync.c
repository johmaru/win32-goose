#include <string.h>

#include "win32_internal.h"

/* ---- Mutex --------------------------------------------------------------- */

uint64_t w32_create_mutex(uint8_t initial_owner, w32_bytes name, uint8_t *out_already_exists) {
    w32_wide_buf wb;
    const wchar_t *wname = NULL;
    if (name.data && name.len > 0) {
        if (!w32_wide_init(&wb, name)) return 0;
        wname = wb.ptr;
    } else {
        wb.ptr = NULL;
        wb.len = 0;
    }

    SetLastError(0);
    HANDLE h = CreateMutexW(NULL, initial_owner ? TRUE : FALSE, wname);
    DWORD err = GetLastError();
    if (out_already_exists) {
        *out_already_exists = (h != NULL && err == ERROR_ALREADY_EXISTS) ? 1 : 0;
    }
    if (wname) w32_wide_free(&wb);
    return (uint64_t)(uintptr_t)h;
}

uint64_t w32_open_mutex(uint32_t desired_access, uint8_t inherit_handle, w32_bytes name) {
    w32_wide_buf wb;
    if (!w32_wide_init(&wb, name) || wb.len == 0) {
        w32_wide_free(&wb);
        return 0;
    }
    HANDLE h = OpenMutexW((DWORD)desired_access, inherit_handle ? TRUE : FALSE, wb.ptr);
    w32_wide_free(&wb);
    return (uint64_t)(uintptr_t)h;
}

/* ---- Semaphore ----------------------------------------------------------- */

uint64_t w32_create_semaphore(
    int32_t initial_count,
    int32_t max_count,
    w32_bytes name,
    uint8_t *out_already_exists
) {
    w32_wide_buf wb;
    const wchar_t *wname = NULL;
    if (name.data && name.len > 0) {
        if (!w32_wide_init(&wb, name)) return 0;
        wname = wb.ptr;
    } else {
        wb.ptr = NULL;
        wb.len = 0;
    }

    SetLastError(0);
    HANDLE h = CreateSemaphoreW(NULL, (LONG)initial_count, (LONG)max_count, wname);
    DWORD err = GetLastError();
    if (out_already_exists) {
        *out_already_exists = (h != NULL && err == ERROR_ALREADY_EXISTS) ? 1 : 0;
    }
    if (wname) w32_wide_free(&wb);
    return (uint64_t)(uintptr_t)h;
}

uint64_t w32_open_semaphore(uint32_t desired_access, uint8_t inherit_handle, w32_bytes name) {
    w32_wide_buf wb;
    if (!w32_wide_init(&wb, name) || wb.len == 0) {
        w32_wide_free(&wb);
        return 0;
    }
    HANDLE h = OpenSemaphoreW((DWORD)desired_access, inherit_handle ? TRUE : FALSE, wb.ptr);
    w32_wide_free(&wb);
    return (uint64_t)(uintptr_t)h;
}

/* ---- Event --------------------------------------------------------------- */

uint64_t w32_create_event(
    uint8_t manual_reset,
    uint8_t initial_state,
    w32_bytes name,
    uint8_t *out_already_exists
) {
    w32_wide_buf wb;
    const wchar_t *wname = NULL;
    if (name.data && name.len > 0) {
        if (!w32_wide_init(&wb, name)) return 0;
        wname = wb.ptr;
    } else {
        wb.ptr = NULL;
        wb.len = 0;
    }

    SetLastError(0);
    HANDLE h = CreateEventW(NULL, manual_reset ? TRUE : FALSE, initial_state ? TRUE : FALSE, wname);
    DWORD err = GetLastError();
    if (out_already_exists) {
        *out_already_exists = (h != NULL && err == ERROR_ALREADY_EXISTS) ? 1 : 0;
    }
    if (wname) w32_wide_free(&wb);
    return (uint64_t)(uintptr_t)h;
}

uint64_t w32_open_event(uint32_t desired_access, uint8_t inherit_handle, w32_bytes name) {
    w32_wide_buf wb;
    if (!w32_wide_init(&wb, name) || wb.len == 0) {
        w32_wide_free(&wb);
        return 0;
    }
    HANDLE h = OpenEventW((DWORD)desired_access, inherit_handle ? TRUE : FALSE, wb.ptr);
    w32_wide_free(&wb);
    return (uint64_t)(uintptr_t)h;
}

/* ---- Waitable Timer ------------------------------------------------------ */

uint64_t w32_create_waitable_timer(
    uint8_t manual_reset,
    w32_bytes name,
    uint8_t *out_already_exists
) {
    w32_wide_buf wb;
    const wchar_t *wname = NULL;
    if (name.data && name.len > 0) {
        if (!w32_wide_init(&wb, name)) return 0;
        wname = wb.ptr;
    } else {
        wb.ptr = NULL;
        wb.len = 0;
    }

    SetLastError(0);
    HANDLE h = CreateWaitableTimerW(NULL, manual_reset ? TRUE : FALSE, wname);
    DWORD err = GetLastError();
    if (out_already_exists) {
        *out_already_exists = (h != NULL && err == ERROR_ALREADY_EXISTS) ? 1 : 0;
    }
    if (wname) w32_wide_free(&wb);
    return (uint64_t)(uintptr_t)h;
}

uint8_t w32_set_waitable_timer(uint64_t htimer, int64_t due_time_100ns, int32_t period_ms) {
    if (!htimer) return 0;
    LARGE_INTEGER li;
    li.QuadPart = (LONGLONG)due_time_100ns;
    return SetWaitableTimer((HANDLE)(uintptr_t)htimer, &li, (LONG)period_ms, NULL, NULL, FALSE) ? 1 : 0;
}

/* ---- Aligned WaitForMultipleObjects -------------------------------------- */

uint32_t w32_wait_multiple(w32_u64_slice handles, uint8_t wait_all, uint32_t timeout_ms) {
    if (!handles.data || handles.len <= 0 || handles.len > MAXIMUM_WAIT_OBJECTS) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return WAIT_FAILED;
    }
    HANDLE aligned[MAXIMUM_WAIT_OBJECTS];
    for (int64_t i = 0; i < handles.len; i++) {
        uint64_t raw = 0;
        memcpy(&raw, handles.data + i, sizeof(raw));
        aligned[i] = (HANDLE)(uintptr_t)raw;
    }
    return (uint32_t)WaitForMultipleObjects(
        (DWORD)handles.len,
        aligned,
        wait_all ? TRUE : FALSE,
        (DWORD)timeout_ms
    );
}
