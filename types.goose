// Core Win32 structs and typed handle wrappers.
// Every struct here is naturally aligned without padding, matching both
// Goose's packed layout and the Windows x64 C ABI.
namespace win32;

struct Point {
    x: i32 = 0,
    y: i32 = 0,
}

struct Size {
    cx: i32 = 0,
    cy: i32 = 0,
}

struct Rect {
    left: i32 = 0,
    top: i32 = 0,
    right: i32 = 0,
    bottom: i32 = 0,
}

fn width(r: const Rect&) -> i32 { r.right - r.left }
fn height(r: const Rect&) -> i32 { r.bottom - r.top }

// Must match w32_event in include/win32_shim.h (packed).
struct Event {
    hwnd: u64 = 0,
    msg: u32 = 0,
    wparam: u64 = 0,
    lparam: i64 = 0,
    x: i32 = 0,
    y: i32 = 0,
    width: i32 = 0,
    height: i32 = 0,
    wheel_delta: i32 = 0,
    key: u32 = 0,
    codepoint: u32 = 0,
    command_id: u32 = 0,
    notify_code: u32 = 0,
}

// Must match w32_file_info in include/win32_shim.h (packed).
struct FileInfo {
    size: i64 = 0,
    attributes: u32 = 0,
    created_ft: u64 = 0,
    modified_ft: u64 = 0,
}

struct DiskSpace {
    avail_bytes: u64 = 0,
    total_bytes: u64 = 0,
    free_bytes: u64 = 0,
}

struct SystemInfo {
    processor_count: u32 = 0,
    page_size: u32 = 0,
    allocation_granularity: u32 = 0,
    processor_arch: u16 = 0,
}

struct MemoryStatus {
    memory_load: u32 = 0,
    total_phys: u64 = 0,
    avail_phys: u64 = 0,
    total_page_file: u64 = 0,
    avail_page_file: u64 = 0,
    total_virtual: u64 = 0,
    avail_virtual: u64 = 0,
}

struct SystemTime {
    year: u16 = 0,
    month: u16 = 0,
    day_of_week: u16 = 0,
    day: u16 = 0,
    hour: u16 = 0,
    minute: u16 = 0,
    second: u16 = 0,
    milliseconds: u16 = 0,
}

struct MonitorInfo {
    left: i32 = 0,
    top: i32 = 0,
    right: i32 = 0,
    bottom: i32 = 0,
    work_left: i32 = 0,
    work_top: i32 = 0,
    work_right: i32 = 0,
    work_bottom: i32 = 0,
    dpi_x: u32 = 96,
    dpi_y: u32 = 96,
    is_primary: bool = false,
}

fn width(m: const MonitorInfo&) -> i32 { m.right - m.left }
fn height(m: const MonitorInfo&) -> i32 { m.bottom - m.top }

struct DisplayMode {
    width: u32 = 0,
    height: u32 = 0,
    bits_per_pixel: u32 = 0,
    frequency: u32 = 0,
}

struct GamepadState {
    connected: bool = false,
    buttons: u16 = 0,
    left_trigger: u8 = 0,
    right_trigger: u8 = 0,
    thumb_lx: i16 = 0,
    thumb_ly: i16 = 0,
    thumb_rx: i16 = 0,
    thumb_ry: i16 = 0,
    packet_number: u32 = 0,
}

struct AudioFormat {
    sample_rate: u32 = 0,
    channels: u16 = 0,
    bits_per_sample: u16 = 0,
    is_float: bool = false,
    buffer_frames: u32 = 0,
}

struct VideoInfo {
    width: u32 = 0,
    height: u32 = 0,
    fps_num: u32 = 0,
    fps_den: u32 = 1,
    duration_100ns: i64 = 0,
}

struct ProcessEntry {
    pid: u32 = 0,
    parent_pid: u32 = 0,
    thread_count: u32 = 0,
    base_priority: i32 = 0,
}

struct PowerStatus {
    ac_line_status: u8 = 255,
    battery_flag: u8 = 255,
    battery_life_percent: u8 = 255,
    power_saver: u8 = 0,
    battery_life_sec: u32 = 0xFFFFFFFF,
    battery_full_life_sec: u32 = 0xFFFFFFFF,
}

fn is_directory(fi: const FileInfo&) -> bool { (fi.attributes & 0x00000010) != 0 }
fn is_readonly(fi: const FileInfo&) -> bool { (fi.attributes & 0x00000001) != 0 }
fn is_hidden(fi: const FileInfo&) -> bool { (fi.attributes & 0x00000002) != 0 }

// Opaque Win32 handle wrappers (carry the raw 64-bit HANDLE/HWND/HDC value).
struct Hwnd { raw: u64 = 0 }
struct Hdc { raw: u64 = 0 }
struct Hmenu { raw: u64 = 0 }
struct Hfile { raw: u64 = 0 }
struct Hfind { raw: u64 = 0 }
struct Hfilemap { id: u32 = 0 }
struct Hdirwatch { raw: u64 = 0 }
struct Hprocess { raw: u64 = 0 }
struct Hregkey { raw: u64 = 0 }
struct Hsocket { raw: u64 = 0 }
struct Haudio { id: u32 = 0 }
struct HaudioIn { id: u32 = 0 }
struct Hvideo { id: u32 = 0 }
struct Hmodule { raw: u64 = 0 }
struct Hprocsnap { raw: u64 = 0 }
struct Hmutex { raw: u64 = 0 }
struct Hsemaphore { raw: u64 = 0 }
struct Hevent { raw: u64 = 0 }
struct Htimer { raw: u64 = 0 }
struct Hbrush { raw: u64 = 0 }
struct Hpen { raw: u64 = 0 }
struct Hfont { raw: u64 = 0 }

fn is_valid(h: Hwnd) -> bool { h.raw != 0 }
fn is_valid(h: Hdc) -> bool { h.raw != 0 }
fn is_valid(h: Hmenu) -> bool { h.raw != 0 }
fn is_valid(h: Hfile) -> bool { h.raw != 0 && h.raw != 0xFFFFFFFFFFFFFFFF }
fn is_valid(h: Hfind) -> bool { h.raw != 0 && h.raw != 0xFFFFFFFFFFFFFFFF }
fn is_valid(h: Hfilemap) -> bool { h.id != 0 }
fn is_valid(h: Hdirwatch) -> bool { h.raw != 0 && h.raw != 0xFFFFFFFFFFFFFFFF }
fn is_valid(h: Hprocess) -> bool { h.raw != 0 }
fn is_valid(h: Hregkey) -> bool { h.raw != 0 }
fn is_valid(h: Hsocket) -> bool { h.raw != 0 && h.raw != 0xFFFFFFFFFFFFFFFF }
fn is_valid(h: Haudio) -> bool { h.id != 0 }
fn is_valid(h: HaudioIn) -> bool { h.id != 0 }
fn is_valid(h: Hvideo) -> bool { h.id != 0 }
fn is_valid(h: Hmodule) -> bool { h.raw != 0 }
fn is_valid(h: Hprocsnap) -> bool { h.raw != 0 && h.raw != 0xFFFFFFFFFFFFFFFF }
fn is_valid(h: Hmutex) -> bool { h.raw != 0 }
fn is_valid(h: Hsemaphore) -> bool { h.raw != 0 }
fn is_valid(h: Hevent) -> bool { h.raw != 0 }
fn is_valid(h: Htimer) -> bool { h.raw != 0 }
fn is_valid(h: Hbrush) -> bool { h.raw != 0 }
fn is_valid(h: Hpen) -> bool { h.raw != 0 }
fn is_valid(h: Hfont) -> bool { h.raw != 0 }

// Win32 COLORREF (0x00BBGGRR).
fn rgb(r: u8, g: u8, b: u8) -> u32 {
    r | ((g as u32) << 8) | ((b as u32) << 16)
}
