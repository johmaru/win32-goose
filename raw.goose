// Direct dynamic extern bindings to Windows system DLLs (no C shim needed).
// Only functions actually reached by a program are resolved via LoadLibraryW /
// GetProcAddress at startup (Goose spec §7.10).
namespace win32;

import types;

extern "kernel32" {
    fn GetLastError() -> u32;
    fn SetLastError(err_code: u32);
    fn GetTickCount64() -> u64;
    fn QueryPerformanceCounter(counter: i64&) -> i32;
    fn QueryPerformanceFrequency(freq: i64&) -> i32;
    fn GetCurrentProcessId() -> u32;
    fn GetCurrentThreadId() -> u32;
    fn Sleep(ms: u32);
    fn Beep(freq: u32, duration: u32) -> i32;
    fn GetModuleHandleW(module_name_null: u64) -> u64;
    fn CloseHandle(handle: u64) -> i32;
    fn ReadFile(hfile: u64, buf: u8&, nbytes: u32, read: u32&, overlapped_null: u64) -> i32;
    fn WriteFile(hfile: u64, buf: const u8&, nbytes: u32, written: u32&, overlapped_null: u64) -> i32;
    fn FlushFileBuffers(hfile: u64) -> i32;
    fn SetEndOfFile(hfile: u64) -> i32;
    fn GetFileSizeEx(hfile: u64, size: i64&) -> i32;
    fn SetFilePointerEx(hfile: u64, distance: i64, new_pos: i64&, move_method: u32) -> i32;
    fn FindClose(hfind: u64) -> i32;
    fn WaitForSingleObject(handle: u64, timeout_ms: u32) -> u32;
    fn ReleaseMutex(hmutex: u64) -> i32;
    fn ReleaseSemaphore(hsem: u64, release_count: i32, prev_count_null: u64) -> i32;
    fn ReleaseSemaphorePrev(hsem: u64, release_count: i32, prev_count: i32&) -> i32 = "ReleaseSemaphore";
    fn SetEvent(hevent: u64) -> i32;
    fn ResetEvent(hevent: u64) -> i32;
    fn PulseEvent(hevent: u64) -> i32;
    fn CancelWaitableTimer(htimer: u64) -> i32;
    fn GetLogicalDrives() -> u32;
    fn FindNextChangeNotification(hchange: u64) -> i32;
    fn FindCloseChangeNotification(hchange: u64) -> i32;
    fn TerminateProcess(hproc: u64, exit_code: u32) -> i32;
    fn FreeLibrary(hmod: u64) -> i32;
    fn ConnectNamedPipe(hpipe: u64, overlapped_null: u64) -> i32;
    fn DisconnectNamedPipe(hpipe: u64) -> i32;
}

extern "user32" {
    fn ShowWindow(hwnd: u64, cmd_show: i32) -> i32;
    fn UpdateWindow(hwnd: u64) -> i32;
    fn DestroyWindow(hwnd: u64) -> i32;
    fn IsWindow(hwnd: u64) -> i32;
    fn IsWindowVisible(hwnd: u64) -> i32;
    fn PostQuitMessage(exit_code: i32);
    fn PostMessageW(hwnd: u64, msg: u32, wparam: u64, lparam: i64) -> i32;
    fn SendMessageW(hwnd: u64, msg: u32, wparam: u64, lparam: i64) -> i64;
    fn MoveWindow(hwnd: u64, x: i32, y: i32, w: i32, h: i32, repaint: i32) -> i32;
    fn GetClientRect(hwnd: u64, rect: Rect&) -> i32;
    fn GetWindowRect(hwnd: u64, rect: Rect&) -> i32;
    fn AdjustWindowRectEx(rect: Rect&, style: u32, has_menu: i32, ex_style: u32) -> i32;
    fn InvalidateRect(hwnd: u64, rect: const Rect&, erase: i32) -> i32;
    fn InvalidateRectAll(hwnd: u64, rect_null: u64, erase: i32) -> i32 = "InvalidateRect";
    fn SetWindowTextW(hwnd: u64, text: const u16&) -> i32;
    fn MessageBoxW(hwnd: u64, text: const u16&, caption: const u16&, utype: u32) -> i32;
    fn GetSystemMetrics(index: i32) -> i32;
    fn GetAsyncKeyState(vkey: i32) -> i16;
    fn GetCursorPos(pt: Point&) -> i32;
    fn ScreenToClient(hwnd: u64, pt: Point&) -> i32;
    fn ClientToScreen(hwnd: u64, pt: Point&) -> i32;
    fn SetTimer(hwnd: u64, id: u64, elapse_ms: u32, proc_null: u64) -> u64;
    fn KillTimer(hwnd: u64, id: u64) -> i32;
    fn GetDpiForWindow(hwnd: u64) -> u32;
    fn GetDC(hwnd: u64) -> u64;
    fn ReleaseDC(hwnd: u64, hdc: u64) -> i32;
    fn FillRect(hdc: u64, rect: const Rect&, hbrush: u64) -> i32;
    fn FrameRect(hdc: u64, rect: const Rect&, hbrush: u64) -> i32;
    fn DrawTextW(hdc: u64, text: const u16&, count: i32, rect: Rect&, format: u32) -> i32;
    fn CreateMenu() -> u64;
    fn CreatePopupMenu() -> u64;
    fn DestroyMenu(hmenu: u64) -> i32;
    fn SetMenu(hwnd: u64, hmenu: u64) -> i32;
    fn GetMenu(hwnd: u64) -> u64;
    fn DrawMenuBar(hwnd: u64) -> i32;
    fn EnableMenuItem(hmenu: u64, id: u32, flags: u32) -> i32;
    fn CheckMenuItem(hmenu: u64, id: u32, flags: u32) -> u32;
    fn GetMenuItemCount(hmenu: u64) -> i32;
    fn GetMenuState(hmenu: u64, id: u32, flags: u32) -> u32;
    fn TrackPopupMenu(hmenu: u64, flags: u32, x: i32, y: i32, reserved: i32, hwnd: u64, rect_null: u64) -> i32;
    fn OpenClipboard(hwnd_owner: u64) -> i32;
    fn CloseClipboard() -> i32;
    fn EmptyClipboard() -> i32;
    fn IsClipboardFormatAvailable(format: u32) -> i32;
    fn GetClipboardSequenceNumber() -> u32;
    fn EnableWindow(hwnd: u64, enable: i32) -> i32;
    fn IsWindowEnabled(hwnd: u64) -> i32;
    fn RegisterHotKey(hwnd: u64, id: i32, modifiers: u32, vk: u32) -> i32;
    fn UnregisterHotKey(hwnd: u64, id: i32) -> i32;
    fn ShowCursor(show: i32) -> i32;
    fn SetCapture(hwnd: u64) -> u64;
    fn ReleaseCapture() -> i32;
    fn GetForegroundWindow() -> u64;
    fn SetForegroundWindow(hwnd: u64) -> i32;
    fn SetFocus(hwnd: u64) -> u64;
}

extern "gdi32" {
    fn GetStockObject(index: i32) -> u64;
    fn CreateSolidBrush(color: u32) -> u64;
    fn CreatePen(style: i32, width: i32, color: u32) -> u64;
    fn SelectObject(hdc: u64, obj: u64) -> u64;
    fn DeleteObject(obj: u64) -> i32;
    fn SetTextColor(hdc: u64, color: u32) -> u32;
    fn SetBkColor(hdc: u64, color: u32) -> u32;
    fn SetBkMode(hdc: u64, mode: i32) -> i32;
    fn Rectangle(hdc: u64, left: i32, top: i32, right: i32, bottom: i32) -> i32;
    fn Ellipse(hdc: u64, left: i32, top: i32, right: i32, bottom: i32) -> i32;
    fn RoundRect(hdc: u64, left: i32, top: i32, right: i32, bottom: i32, w: i32, h: i32) -> i32;
    fn MoveToEx(hdc: u64, x: i32, y: i32, prev_null: u64) -> i32;
    fn LineTo(hdc: u64, x: i32, y: i32) -> i32;
    fn TextOutW(hdc: u64, x: i32, y: i32, text: const u16&, count: i32) -> i32;
}
