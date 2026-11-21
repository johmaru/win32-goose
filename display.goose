// Win32 Display Devices Reference: monitors, per-monitor DPI, display adapters,
// and supported display modes (EnumDisplayMonitors / EnumDisplayDevicesW / EnumDisplaySettingsW).
namespace win32;

import types;

extern fn monitor_count() -> i32 = "w32_monitor_count";
extern fn monitor_info(index: i32, out: MonitorInfo&, out_device_name: u8[>..]&) -> bool = "w32_get_monitor_info";
extern fn display_device(index: i32, out_name: u8[>..]&, out_desc: u8[>..]&, out_active: bool&) -> bool = "w32_get_display_device";
extern fn current_display_mode(device_name: const u8[:], out: DisplayMode&) -> bool = "w32_get_current_display_mode";
extern fn display_mode_at(device_name: const u8[:], mode_index: u32, out: DisplayMode&) -> bool = "w32_get_display_mode_at";
