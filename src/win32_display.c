#include <string.h>

#include "win32_internal.h"
#include <shellscalingapi.h>

#define W32_MAX_MONITORS 32

typedef struct {
    HMONITOR handles[W32_MAX_MONITORS];
    int32_t count;
} w32_mon_list;

static BOOL CALLBACK enum_monitors_cb(HMONITOR hm, HDC hdc, LPRECT rc, LPARAM data) {
    (void)hdc;
    (void)rc;
    w32_mon_list *list = (w32_mon_list *)data;
    if (list->count < W32_MAX_MONITORS) {
        list->handles[list->count++] = hm;
    }
    return TRUE;
}

static void collect_monitors(w32_mon_list *list) {
    memset(list, 0, sizeof(*list));
    EnumDisplayMonitors(NULL, NULL, enum_monitors_cb, (LPARAM)list);
}

int32_t w32_monitor_count(void) {
    w32_mon_list list;
    collect_monitors(&list);
    return list.count;
}

uint8_t w32_get_monitor_info(int32_t index, w32_monitor_info *out, gs_rref out_device_name) {
    if (!out || index < 0) return 0;
    w32_mon_list list;
    collect_monitors(&list);
    if (index >= list.count) return 0;

    MONITORINFOEXW mi;
    memset(&mi, 0, sizeof(mi));
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoW(list.handles[index], (LPMONITORINFO)&mi)) return 0;

    out->left = (int32_t)mi.rcMonitor.left;
    out->top = (int32_t)mi.rcMonitor.top;
    out->right = (int32_t)mi.rcMonitor.right;
    out->bottom = (int32_t)mi.rcMonitor.bottom;
    out->work_left = (int32_t)mi.rcWork.left;
    out->work_top = (int32_t)mi.rcWork.top;
    out->work_right = (int32_t)mi.rcWork.right;
    out->work_bottom = (int32_t)mi.rcWork.bottom;
    out->is_primary = (mi.dwFlags & MONITORINFOF_PRIMARY) ? 1 : 0;

    UINT dx = 96, dy = 96;
    if (FAILED(GetDpiForMonitor(list.handles[index], MDT_EFFECTIVE_DPI, &dx, &dy))) {
        dx = 96;
        dy = 96;
    }
    out->dpi_x = (uint32_t)dx;
    out->dpi_y = (uint32_t)dy;

    w32_append_wide_as_utf8(mi.szDevice, -1, out_device_name);
    return 1;
}

uint8_t w32_get_display_device(int32_t index, gs_rref out_name, gs_rref out_desc, uint8_t *out_active) {
    if (index < 0) return 0;
    DISPLAY_DEVICEW dd;
    memset(&dd, 0, sizeof(dd));
    dd.cb = sizeof(dd);
    if (!EnumDisplayDevicesW(NULL, (DWORD)index, &dd, 0)) return 0;

    w32_append_wide_as_utf8(dd.DeviceName, -1, out_name);
    w32_append_wide_as_utf8(dd.DeviceString, -1, out_desc);
    if (out_active) {
        *out_active = (dd.StateFlags & DISPLAY_DEVICE_ACTIVE) ? 1 : 0;
    }
    return 1;
}

static uint8_t query_display_mode(w32_bytes device_name, DWORD mode_num, w32_display_mode *out) {
    if (!out) return 0;
    w32_wide_buf wdev;
    const wchar_t *dev_ptr = NULL;
    if (device_name.data && device_name.len > 0) {
        if (!w32_wide_init(&wdev, device_name)) return 0;
        dev_ptr = wdev.ptr;
    } else {
        wdev.ptr = NULL;
        wdev.len = 0;
    }

    DEVMODEW dm;
    memset(&dm, 0, sizeof(dm));
    dm.dmSize = sizeof(dm);
    BOOL ok = EnumDisplaySettingsW(dev_ptr, mode_num, &dm);
    if (dev_ptr) w32_wide_free(&wdev);
    if (!ok) return 0;

    out->width = (uint32_t)dm.dmPelsWidth;
    out->height = (uint32_t)dm.dmPelsHeight;
    out->bits_per_pixel = (uint32_t)dm.dmBitsPerPel;
    out->frequency = (uint32_t)dm.dmDisplayFrequency;
    return 1;
}

uint8_t w32_get_current_display_mode(w32_bytes device_name, w32_display_mode *out) {
    return query_display_mode(device_name, ENUM_CURRENT_SETTINGS, out);
}

uint8_t w32_get_display_mode_at(w32_bytes device_name, uint32_t mode_index, w32_display_mode *out) {
    return query_display_mode(device_name, (DWORD)mode_index, out);
}
