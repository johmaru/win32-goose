// Win32 kernel synchronization objects:
// Mutex, Semaphore, Event, Waitable Timer, WaitForSingleObject, WaitForMultipleObjects.
// Useful for inter-process synchronization (e.g. single-instance app guards via
// named mutexes), IPC signaling, and high-resolution waitable timers.
namespace win32;

import constants;
import types;
import raw;

extern fn shim_create_mutex(initial_owner: bool, name: const u8[:], already_exists: bool&) -> u64 = "w32_create_mutex";
extern fn shim_open_mutex(access: u32, inherit_handle: bool, name: const u8[:]) -> u64 = "w32_open_mutex";
extern fn shim_create_semaphore(initial_count: i32, max_count: i32, name: const u8[:], already_exists: bool&) -> u64 = "w32_create_semaphore";
extern fn shim_open_semaphore(access: u32, inherit_handle: bool, name: const u8[:]) -> u64 = "w32_open_semaphore";
extern fn shim_create_event(manual_reset: bool, initial_state: bool, name: const u8[:], already_exists: bool&) -> u64 = "w32_create_event";
extern fn shim_open_event(access: u32, inherit_handle: bool, name: const u8[:]) -> u64 = "w32_open_event";
extern fn shim_create_waitable_timer(manual_reset: bool, name: const u8[:], already_exists: bool&) -> u64 = "w32_create_waitable_timer";
extern fn shim_set_waitable_timer(htimer: u64, due_time_100ns: i64, period_ms: i32) -> bool = "w32_set_waitable_timer";
extern fn wait_multiple(handles: const u64[:], wait_all: bool = false, timeout_ms: u32 = INFINITE) -> u32 = "w32_wait_multiple";

fn wait_single(handle: u64, timeout_ms: u32 = INFINITE) -> u32 {
    return WaitForSingleObject(handle, timeout_ms);
}

// --- Hmutex -----------------------------------------------------------------

// Creates (or opens if `name` already exists) a Win32 mutex.
// Returns (handle, already_exists).
fn create_mutex(initial_owner: bool = false, name: const u8[:] = "") -> Hmutex, bool {
    var exists = false;
    let raw = shim_create_mutex(initial_owner, name, exists);
    return Hmutex { raw }, exists;
}

fn open_mutex(name: const u8[:], access: u32 = MUTEX_ALL_ACCESS, inherit_handle: bool = false) -> Hmutex {
    return Hmutex { raw: shim_open_mutex(access, inherit_handle, name) };
}

fn wait(m: Hmutex, timeout_ms: u32 = INFINITE) -> u32 {
    return WaitForSingleObject(m.raw, timeout_ms);
}

fn lock(m: Hmutex, timeout_ms: u32 = INFINITE) -> bool {
    let r = WaitForSingleObject(m.raw, timeout_ms);
    return r == WAIT_OBJECT_0 || r == WAIT_ABANDONED;
}

fn release(m: Hmutex) -> bool {
    return m.is_valid() && ReleaseMutex(m.raw) != 0;
}

fn close(m: Hmutex) -> bool {
    return m.is_valid() && CloseHandle(m.raw) != 0;
}

// --- Hsemaphore -------------------------------------------------------------

fn create_semaphore(initial_count: i32, max_count: i32, name: const u8[:] = "") -> Hsemaphore, bool {
    var exists = false;
    let raw = shim_create_semaphore(initial_count, max_count, name, exists);
    return Hsemaphore { raw }, exists;
}

fn open_semaphore(name: const u8[:], access: u32 = SEMAPHORE_ALL_ACCESS, inherit_handle: bool = false) -> Hsemaphore {
    return Hsemaphore { raw: shim_open_semaphore(access, inherit_handle, name) };
}

fn wait(s: Hsemaphore, timeout_ms: u32 = INFINITE) -> u32 {
    return WaitForSingleObject(s.raw, timeout_ms);
}

fn acquire(s: Hsemaphore, timeout_ms: u32 = INFINITE) -> bool {
    return WaitForSingleObject(s.raw, timeout_ms) == WAIT_OBJECT_0;
}

fn release(s: Hsemaphore, count: i32 = 1) -> bool {
    return s.is_valid() && ReleaseSemaphore(s.raw, count, 0) != 0;
}

fn release_prev(s: Hsemaphore, count: i32, prev_count: i32&) -> bool {
    return s.is_valid() && ReleaseSemaphorePrev(s.raw, count, prev_count) != 0;
}

fn close(s: Hsemaphore) -> bool {
    return s.is_valid() && CloseHandle(s.raw) != 0;
}

// --- Hevent -----------------------------------------------------------------

fn create_event(manual_reset: bool = true, initial_state: bool = false, name: const u8[:] = "") -> Hevent, bool {
    var exists = false;
    let raw = shim_create_event(manual_reset, initial_state, name, exists);
    return Hevent { raw }, exists;
}

fn open_event(name: const u8[:], access: u32 = EVENT_ALL_ACCESS, inherit_handle: bool = false) -> Hevent {
    return Hevent { raw: shim_open_event(access, inherit_handle, name) };
}

fn wait(e: Hevent, timeout_ms: u32 = INFINITE) -> u32 {
    return WaitForSingleObject(e.raw, timeout_ms);
}

fn set(e: Hevent) -> bool {
    return e.is_valid() && SetEvent(e.raw) != 0;
}

fn reset(e: Hevent) -> bool {
    return e.is_valid() && ResetEvent(e.raw) != 0;
}

fn pulse(e: Hevent) -> bool {
    return e.is_valid() && PulseEvent(e.raw) != 0;
}

fn close(e: Hevent) -> bool {
    return e.is_valid() && CloseHandle(e.raw) != 0;
}

// --- Htimer (Waitable Timer) ------------------------------------------------

fn create_waitable_timer(manual_reset: bool = true, name: const u8[:] = "") -> Htimer, bool {
    var exists = false;
    let raw = shim_create_waitable_timer(manual_reset, name, exists);
    return Htimer { raw }, exists;
}

// Schedules the timer relative to now (`delay_ms` >= 0). `period_ms` > 0 repeats.
fn set_relative_ms(t: Htimer, delay_ms: i64, period_ms: i32 = 0) -> bool {
    let due_100ns = -delay_ms * 10000;
    return t.is_valid() && shim_set_waitable_timer(t.raw, due_100ns, period_ms);
}

fn cancel(t: Htimer) -> bool {
    return t.is_valid() && CancelWaitableTimer(t.raw) != 0;
}

fn wait(t: Htimer, timeout_ms: u32 = INFINITE) -> u32 {
    return WaitForSingleObject(t.raw, timeout_ms);
}

fn close(t: Htimer) -> bool {
    return t.is_valid() && CloseHandle(t.raw) != 0;
}
