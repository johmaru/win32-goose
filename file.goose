// Win32 native File I/O, filesystem operations, and directory enumeration.
namespace win32;

import constants;
import types;
import raw;

extern fn shim_create_file(
    path: const u8[:],
    desired_access: u32,
    share_mode: u32,
    creation_disposition: u32,
    flags_and_attributes: u32
) -> u64 = "w32_create_file";

extern fn shim_get_file_info_by_handle(hfile: u64, out: FileInfo&) -> bool = "w32_get_file_info_by_handle";
extern fn file_attributes(path: const u8[:]) -> u32 = "w32_get_file_attributes";
extern fn delete_file(path: const u8[:]) -> bool = "w32_delete_file";
extern fn move_file(from: const u8[:], to: const u8[:], replace_existing: bool = true) -> bool = "w32_move_file";
extern fn copy_file(from: const u8[:], to: const u8[:], fail_if_exists: bool = false) -> bool = "w32_copy_file";
extern fn create_directory(path: const u8[:]) -> bool = "w32_create_directory";
extern fn remove_directory(path: const u8[:]) -> bool = "w32_remove_directory";
extern fn shim_get_temp_path(out: u8[>..]&) -> bool = "w32_get_temp_path";
extern fn shim_get_current_directory(out: u8[>..]&) -> bool = "w32_get_current_directory";
extern fn shim_get_full_path_name(path: const u8[:], out: u8[>..]&) -> bool = "w32_get_full_path_name";
extern fn shim_find_first_file(pattern: const u8[:], out_name: u8[>..]&, out_info: FileInfo&) -> u64 = "w32_find_first_file";
extern fn shim_find_next_file(hfind: u64, out_name: u8[>..]&, out_info: FileInfo&) -> bool = "w32_find_next_file";
extern fn read_file_bytes(path: const u8[:], out: u8[>..]&) -> bool = "w32_read_file_all";
extern fn write_file_bytes(path: const u8[:], data: const u8[:]) -> bool = "w32_write_file_all";

// --- Handle-based file API (Hfile) ------------------------------------------

fn create_file(
    path: const u8[:],
    access: u32 = GENERIC_READ,
    share: u32 = FILE_SHARE_READ,
    disposition: u32 = OPEN_EXISTING,
    flags_and_attrs: u32 = FILE_ATTRIBUTE_NORMAL
) -> Hfile {
    return Hfile { raw: shim_create_file(path, access, share, disposition, flags_and_attrs) };
}

fn open_file_read(path: const u8[:]) -> Hfile {
    return create_file(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, OPEN_EXISTING);
}

fn open_file_write(path: const u8[:]) -> Hfile {
    return create_file(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, CREATE_ALWAYS);
}

fn open_file_append(path: const u8[:]) -> Hfile {
    let f = create_file(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, OPEN_ALWAYS);
    if f.is_valid() {
        f.seek(0, FILE_END);
    }
    return f;
}

fn close(f: Hfile) -> bool {
    return f.is_valid() && CloseHandle(f.raw) != 0;
}

// Reads up to `buf.len` bytes into `buf` via direct kernel32!ReadFile.
fn read(f: Hfile, buf: u8[:]) -> i64, bool {
    guard f.is_valid() else { return 0, false; }
    if buf.len <= 0 { return 0, true; }
    let chunk: u32 = if buf.len > 0x40000000 { 0x40000000 } else { buf.len as! u32 };
    var got: u32 = 0;
    let ok = ReadFile(f.raw, buf[0], chunk, got, 0) != 0;
    return got, ok;
}

// Writes up to `data.len` bytes via direct kernel32!WriteFile.
fn write(f: Hfile, data: const u8[:]) -> i64, bool {
    guard f.is_valid() else { return 0, false; }
    if data.len <= 0 { return 0, true; }
    let chunk: u32 = if data.len > 0x40000000 { 0x40000000 } else { data.len as! u32 };
    var written: u32 = 0;
    let ok = WriteFile(f.raw, data[0], chunk, written, 0) != 0;
    return written, ok;
}

fn write_all(f: Hfile, data: const u8[:]) -> bool {
    var off: i64 = 0;
    while off < data.len {
        let n, ok = f.write(data[off..]);
        guard ok && n > 0 else { return false; }
        off += n;
    }
    return true;
}

fn seek(f: Hfile, offset: i64, origin: u32 = FILE_BEGIN) -> i64, bool {
    guard f.is_valid() else { return 0, false; }
    var pos: i64 = 0;
    let ok = SetFilePointerEx(f.raw, offset, pos, origin) != 0;
    return pos, ok;
}

fn tell(f: Hfile) -> i64, bool {
    return f.seek(0, FILE_CURRENT);
}

fn size(f: Hfile) -> i64, bool {
    guard f.is_valid() else { return 0, false; }
    var sz: i64 = 0;
    let ok = GetFileSizeEx(f.raw, sz) != 0;
    return sz, ok;
}

fn truncate(f: Hfile) -> bool {
    return f.is_valid() && SetEndOfFile(f.raw) != 0;
}

fn flush(f: Hfile) -> bool {
    return f.is_valid() && FlushFileBuffers(f.raw) != 0;
}

fn info(f: Hfile, out: FileInfo&) -> bool {
    return f.is_valid() && shim_get_file_info_by_handle(f.raw, out);
}

// --- Filesystem & path helpers ----------------------------------------------

fn file_exists(path: const u8[:]) -> bool {
    let attrs = file_attributes(path);
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

fn dir_exists(path: const u8[:]) -> bool {
    let attrs = file_attributes(path);
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

fn temp_dir() -> u8[>..] {
    var out: u8[>..] = [];
    shim_get_temp_path(out);
    return out;
}

fn current_dir() -> u8[>..] {
    var out: u8[>..] = [];
    shim_get_current_directory(out);
    return out;
}

fn full_path(path: const u8[:]) -> u8[>..] {
    var out: u8[>..] = [];
    shim_get_full_path_name(path, out);
    return out;
}

// --- Directory enumeration (FindFirstFileW / FindNextFileW) -----------------

fn find_first_file(pattern: const u8[:], out_name: u8[>..]&, out_info: FileInfo&) -> Hfind {
    return Hfind { raw: shim_find_first_file(pattern, out_name, out_info) };
}

fn next(hf: Hfind, out_name: u8[>..]&, out_info: FileInfo&) -> bool {
    return hf.is_valid() && shim_find_next_file(hf.raw, out_name, out_info);
}

fn close(hf: Hfind) -> bool {
    return hf.is_valid() && FindClose(hf.raw) != 0;
}

// --- Storage extensions: Memory-mapped files, Disk/Volume, Dir watch --------

extern fn shim_file_map_open(hfile: u64, writable: bool, max_size: u64) -> u32 = "w32_file_map_open";
extern fn shim_file_map_size(map_id: u32) -> i64 = "w32_file_map_size";
extern fn shim_file_map_read(map_id: u32, offset: i64, out: u8[:]) -> i64 = "w32_file_map_read";
extern fn shim_file_map_write(map_id: u32, offset: i64, data: const u8[:]) -> i64 = "w32_file_map_write";
extern fn shim_file_map_flush(map_id: u32, offset: i64, len: i64) -> bool = "w32_file_map_flush";
extern fn shim_file_map_close(map_id: u32) = "w32_file_map_close";
extern fn disk_free_space(path: const u8[:], out: DiskSpace&) -> bool = "w32_get_disk_free_space";
extern fn volume_info(root_path: const u8[:], out_label: u8[>..]&, out_fs: u8[>..]&, out_serial: u32&) -> bool = "w32_get_volume_info";
extern fn shim_watch_dir_open(path: const u8[:], watch_subtree: bool, filter: u32) -> u64 = "w32_watch_dir_open";

fn file_map_open(f: Hfile, writable: bool = false, max_size: u64 = 0) -> Hfilemap {
    guard f.is_valid() else { return Hfilemap {}; }
    return Hfilemap { id: shim_file_map_open(f.raw, writable, max_size) };
}

fn size(m: Hfilemap) -> i64 { shim_file_map_size(m.id) }
fn read(m: Hfilemap, offset: i64, out: u8[:]) -> i64 { shim_file_map_read(m.id, offset, out) }
fn write(m: Hfilemap, offset: i64, data: const u8[:]) -> i64 { shim_file_map_write(m.id, offset, data) }
fn flush(m: Hfilemap, offset: i64 = 0, len: i64 = 0) -> bool { shim_file_map_flush(m.id, offset, len) }
fn close(m: Hfilemap) { shim_file_map_close(m.id); }

fn logical_drives_mask() -> u32 { GetLogicalDrives() }

fn watch_dir_open(
    path: const u8[:],
    watch_subtree: bool = false,
    filter: u32 = FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE
) -> Hdirwatch {
    return Hdirwatch { raw: shim_watch_dir_open(path, watch_subtree, filter) };
}

fn wait(w: Hdirwatch, timeout_ms: u32 = 0) -> u32 {
    return WaitForSingleObject(w.raw, timeout_ms);
}

fn next(w: Hdirwatch) -> bool {
    return w.is_valid() && FindNextChangeNotification(w.raw) != 0;
}

fn close(w: Hdirwatch) -> bool {
    return w.is_valid() && FindCloseChangeNotification(w.raw) != 0;
}
