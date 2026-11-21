// Win32 System Services: System/Memory info, Time, Environment, Processes & Pipes,
// Windows Registry (advapi32), and Console helpers.
namespace win32;

import constants;
import types;
import raw;

extern fn system_info(out: SystemInfo&) = "w32_get_system_info";
extern fn memory_status(out: MemoryStatus&) -> bool = "w32_get_memory_status";
extern fn local_time(out: SystemTime&) = "w32_get_local_time";
extern fn system_time(out: SystemTime&) = "w32_get_system_time";
extern fn shim_computer_name(out: u8[>..]&) -> bool = "w32_get_computer_name";
extern fn shim_user_name(out: u8[>..]&) -> bool = "w32_get_user_name";
extern fn shim_module_file_name(out: u8[>..]&) -> bool = "w32_get_module_file_name";
extern fn env_get(name: const u8[:], out: u8[>..]&) -> bool = "w32_env_get";
extern fn env_set(name: const u8[:], value: const u8[:]) -> bool = "w32_env_set";
extern fn env_delete(name: const u8[:]) -> bool = "w32_env_delete";

extern fn run_process_capture(
    cmdline: const u8[:],
    out_stdout: u8[>..]&,
    out_exit_code: u32&,
    timeout_ms: u32 = INFINITE
) -> bool = "w32_run_process_capture";

extern fn shim_spawn_process(cmdline: const u8[:], working_dir: const u8[:], hide_window: bool) -> u64 = "w32_spawn_process";
extern fn shim_get_exit_code_process(hproc: u64, out_code: u32&) -> bool = "w32_get_exit_code_process";

extern fn shim_reg_create_key(root: u64, subkey: const u8[:], access: u32) -> u64 = "w32_reg_create_key";
extern fn shim_reg_open_key(root: u64, subkey: const u8[:], access: u32) -> u64 = "w32_reg_open_key";
extern fn reg_delete_key(root: u64, subkey: const u8[:]) -> bool = "w32_reg_delete_key";
extern fn shim_reg_close_key(hkey: u64) -> bool = "w32_reg_close_key";
extern fn shim_reg_set_string(hkey: u64, name: const u8[:], value: const u8[:]) -> bool = "w32_reg_set_string";
extern fn shim_reg_get_string(hkey: u64, name: const u8[:], out: u8[>..]&) -> bool = "w32_reg_get_string";
extern fn shim_reg_set_u32(hkey: u64, name: const u8[:], value: u32) -> bool = "w32_reg_set_u32";
extern fn shim_reg_get_u32(hkey: u64, name: const u8[:], out: u32&) -> bool = "w32_reg_get_u32";
extern fn shim_reg_set_u64(hkey: u64, name: const u8[:], value: u64) -> bool = "w32_reg_set_u64";
extern fn shim_reg_get_u64(hkey: u64, name: const u8[:], out: u64&) -> bool = "w32_reg_get_u64";
extern fn shim_reg_set_bytes(hkey: u64, name: const u8[:], data: const u8[:]) -> bool = "w32_reg_set_bytes";
extern fn shim_reg_get_bytes(hkey: u64, name: const u8[:], out: u8[>..]&) -> bool = "w32_reg_get_bytes";
extern fn shim_reg_delete_value(hkey: u64, name: const u8[:]) -> bool = "w32_reg_delete_value";

extern fn enable_console_vt() -> bool = "w32_enable_console_vt";
extern fn set_console_title(title: const u8[:]) -> bool = "w32_set_console_title";
extern fn console_size(out_cols: i32&, out_rows: i32&) -> bool = "w32_get_console_size";

// --- Convenience wrappers ---------------------------------------------------

fn computer_name() -> u8[>..] {
    var out: u8[>..] = [];
    shim_computer_name(out);
    return out;
}

fn user_name() -> u8[>..] {
    var out: u8[>..] = [];
    shim_user_name(out);
    return out;
}

fn module_file_name() -> u8[>..] {
    var out: u8[>..] = [];
    shim_module_file_name(out);
    return out;
}

// --- Hprocess methods -------------------------------------------------------

fn spawn_process(cmdline: const u8[:], working_dir: const u8[:] = "", hide_window: bool = false) -> Hprocess {
    return Hprocess { raw: shim_spawn_process(cmdline, working_dir, hide_window) };
}

fn wait(p: Hprocess, timeout_ms: u32 = INFINITE) -> u32 {
    return WaitForSingleObject(p.raw, timeout_ms);
}

fn exit_code(p: Hprocess, out_code: u32&) -> bool {
    return p.is_valid() && shim_get_exit_code_process(p.raw, out_code);
}

fn terminate(p: Hprocess, code: u32 = 1) -> bool {
    return p.is_valid() && TerminateProcess(p.raw, code) != 0;
}

fn close(p: Hprocess) -> bool {
    return p.is_valid() && CloseHandle(p.raw) != 0;
}

// --- Hregkey methods --------------------------------------------------------

fn reg_create_key(root: u64, subkey: const u8[:], access: u32 = KEY_ALL_ACCESS) -> Hregkey {
    return Hregkey { raw: shim_reg_create_key(root, subkey, access) };
}

fn reg_open_key(root: u64, subkey: const u8[:], access: u32 = KEY_READ) -> Hregkey {
    return Hregkey { raw: shim_reg_open_key(root, subkey, access) };
}

fn close(k: Hregkey) -> bool { k.is_valid() && shim_reg_close_key(k.raw) }
fn set_string(k: Hregkey, name: const u8[:], value: const u8[:]) -> bool { k.is_valid() && shim_reg_set_string(k.raw, name, value) }
fn get_string(k: Hregkey, name: const u8[:], out: u8[>..]&) -> bool { k.is_valid() && shim_reg_get_string(k.raw, name, out) }
fn set_u32(k: Hregkey, name: const u8[:], value: u32) -> bool { k.is_valid() && shim_reg_set_u32(k.raw, name, value) }
fn get_u32(k: Hregkey, name: const u8[:], out: u32&) -> bool { k.is_valid() && shim_reg_get_u32(k.raw, name, out) }
fn set_u64(k: Hregkey, name: const u8[:], value: u64) -> bool { k.is_valid() && shim_reg_set_u64(k.raw, name, value) }
fn get_u64(k: Hregkey, name: const u8[:], out: u64&) -> bool { k.is_valid() && shim_reg_get_u64(k.raw, name, out) }
fn set_bytes(k: Hregkey, name: const u8[:], data: const u8[:]) -> bool { k.is_valid() && shim_reg_set_bytes(k.raw, name, data) }
fn get_bytes(k: Hregkey, name: const u8[:], out: u8[>..]&) -> bool { k.is_valid() && shim_reg_get_bytes(k.raw, name, out) }
fn delete_value(k: Hregkey, name: const u8[:]) -> bool { k.is_valid() && shim_reg_delete_value(k.raw, name) }

// --- Process Enumeration (Toolhelp32) ---------------------------------------

extern fn shim_process_snapshot_open() -> u64 = "w32_process_snapshot_open";
extern fn shim_process_snapshot_next(hsnap: u64, first: bool, out: ProcessEntry&, out_exe: u8[>..]&) -> bool = "w32_process_snapshot_next";

fn process_snapshot_open() -> Hprocsnap {
    return Hprocsnap { raw: shim_process_snapshot_open() };
}

fn first_process(s: Hprocsnap, out_entry: ProcessEntry&, out_exe: u8[>..]&) -> bool {
    return s.is_valid() && shim_process_snapshot_next(s.raw, true, out_entry, out_exe);
}

fn next_process(s: Hprocsnap, out_entry: ProcessEntry&, out_exe: u8[>..]&) -> bool {
    return s.is_valid() && shim_process_snapshot_next(s.raw, false, out_entry, out_exe);
}

fn close(s: Hprocsnap) -> bool {
    return s.is_valid() && CloseHandle(s.raw) != 0;
}

// --- Dynamic DLL Loading ----------------------------------------------------

extern fn shim_load_library(path: const u8[:]) -> u64 = "w32_load_library";
extern fn shim_get_proc_address(hmod: u64, proc_name: const u8[:]) -> u64 = "w32_get_proc_address";

fn load_library(path: const u8[:]) -> Hmodule {
    return Hmodule { raw: shim_load_library(path) };
}

fn proc_address(m: Hmodule, proc_name: const u8[:]) -> u64 {
    guard m.is_valid() else { return 0; }
    return shim_get_proc_address(m.raw, proc_name);
}

fn free(m: Hmodule) -> bool {
    return m.is_valid() && FreeLibrary(m.raw) != 0;
}

// --- Virtual Memory ---------------------------------------------------------

extern fn shim_virtual_alloc(size: u64, alloc_type: u32, protect: u32) -> u64 = "w32_virtual_alloc";
extern fn virtual_protect(addr: u64, size: u64, new_protect: u32, out_old_protect: u32&) -> bool = "w32_virtual_protect";
extern fn virtual_free(addr: u64) -> bool = "w32_virtual_free";

fn virtual_alloc(size: u64, alloc_type: u32 = MEM_COMMIT | MEM_RESERVE, protect: u32 = PAGE_READWRITE) -> u64 {
    return shim_virtual_alloc(size, alloc_type, protect);
}

// --- Power & Sleep Prevention -----------------------------------------------

extern fn power_status(out: PowerStatus&) -> bool = "w32_get_power_status";
extern fn set_thread_execution_state(flags: u32) -> u32 = "w32_set_thread_execution_state";

fn prevent_sleep(keep_display: bool = false) -> bool {
    let flags = ES_CONTINUOUS | ES_SYSTEM_REQUIRED | (if keep_display { ES_DISPLAY_REQUIRED } else { 0 });
    return set_thread_execution_state(flags) != 0;
}

fn allow_sleep() -> bool {
    return set_thread_execution_state(ES_CONTINUOUS) != 0;
}

// --- Named Pipes IPC --------------------------------------------------------

extern fn shim_named_pipe_create(
    name: const u8[:],
    max_instances: u32,
    out_buf_size: u32,
    in_buf_size: u32,
    nonblocking: bool
) -> u64 = "w32_named_pipe_create";
extern fn shim_named_pipe_open(name: const u8[:], timeout_ms: u32) -> u64 = "w32_named_pipe_open";

fn named_pipe_create(
    name: const u8[:],
    max_instances: u32 = 1,
    out_buf_size: u32 = 4096,
    in_buf_size: u32 = 4096,
    nonblocking: bool = false
) -> Hfile {
    return Hfile { raw: shim_named_pipe_create(name, max_instances, out_buf_size, in_buf_size, nonblocking) };
}

fn named_pipe_open(name: const u8[:], timeout_ms: u32 = 5000) -> Hfile {
    return Hfile { raw: shim_named_pipe_open(name, timeout_ms) };
}

fn pipe_connect(p: Hfile) -> bool {
    guard p.is_valid() else { return false; }
    if ConnectNamedPipe(p.raw, 0) != 0 { return true; }
    return GetLastError() == 535; // ERROR_PIPE_CONNECTED
}

fn pipe_disconnect(p: Hfile) -> bool {
    return p.is_valid() && DisconnectNamedPipe(p.raw) != 0;
}
