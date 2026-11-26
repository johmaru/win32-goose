<img src="assets/icon.svg" width="64" height="64" alt="">

# win32-goose

A [Goose](https://github.com/aardappel/goose) package providing Win32 API bindings for 64-bit Windows using a hybrid architecture:

1. **Direct DLL dynamic externs (`raw.goose`)** — calls `kernel32.dll`, `user32.dll`, and `gdi32.dll` directly via Goose's `extern "dll" { ... }` blocks with zero C wrapper overhead.
2. **Thin C shim (`src/win32_shim.c`, `include/win32_shim.h`)** — handles only what pure Goose `extern` cannot express:
   - `WndProc` callback & ring-buffer event queue (`poll_event` / `wait_event`)
   - `WNDCLASSEXW` / `CreateWindowExW` (struct-embedded pointers & UTF-8 → UTF-16 widening)
   - Flicker-free double-buffered GDI paint sessions (`begin_paint` / `end_paint`)
   - 2-byte aligned UTF-16 wrappers for `*W` APIs (`SetWindowTextW`, `MessageBoxW`, `DrawTextW`, `TextOutW`) — Goose's data stacks are 1-byte packed, whereas Windows `UNICODE_STRING` APIs silently ignore odd-aligned `LPCWSTR` pointers
   - `FormatMessageW` / `GetWindowTextW` into Goose `u8[>..]` builders (`gs_bld_append`)
   - Per-Monitor V2 DPI awareness

## Layout

- `win32.build` — Goose package sidecar (`c_sources_windows`, `c_libs_windows`, `main_thread_only`, `exclude`)
- `win32.goose` — package entry (`namespace win32;`), shim externs, and idiomatic `Hwnd` / `Hdc` wrappers
- `raw.goose` — pure dynamic `extern "kernel32"` / `"user32"` / `"gdi32"` declarations
- `menu.goose` — menu bars, submenus, check/enable state, `WM_COMMAND`, and context popup menus
- `dialog.goose` — Open/Save File, Folder Picker (`IFileOpenDialog`), Color Picker (`ChooseColorW`), and modal `input_box` (`DLGTEMPLATE`)
- `clipboard.goose` — Unicode clipboard text, custom binary formats, and sequence tracking
- `file.goose` — Win32 `Hfile` handle I/O, memory-mapped files (`Hfilemap`), disk/volume info, directory watch (`Hdirwatch`), and `FindFirstFileW` enumeration
- `sync.goose` — Win32 kernel synchronization objects (`Hmutex`, `Hsemaphore`, `Hevent`, `Htimer`, `wait_single`, `wait_multiple`)
- `system.goose` — system/memory info, time, env vars, process execution & stdout pipe capture, running process enumeration (`Toolhelp32`), dynamic DLL loading, virtual memory, power/sleep control, Named Pipes IPC, Windows Registry (`advapi32`), and console helpers
- `net.goose` — Winsock2 (TCP/UDP/DNS/`WSAPoll`) and WinHTTP (HTTP/HTTPS client with custom request/response headers via OS TLS)
- `display.goose` — display monitors, per-monitor DPI, display adapters, and supported display modes
- `desktop.goose` — DWM styling (dark titlebar / rounded corners / Mica), Known Folders, `ShellExecuteW`, and System Tray icons
- `controls.goose` — native Win32 controls (Button, Checkbox, Label, Edit, ListBox, ComboBox, ProgressBar, Trackbar, Tab Control, Status Bar), Drag & Drop (`WM_DROPFILES`), and Global Hotkeys (`WM_HOTKEY`)
- `wic.goose` — Windows Imaging Component (WIC) PNG/JPEG/BMP image decoding (BGRA/RGBA) and PNG/BMP encoding
- `dib.goose` — 32-bit software DIB pixel buffer (`StretchDIBits`, `xrgb`/`argb`, pixel drawing helpers)
- `d3d11.goose` — Direct3D 11 GPU device, swapchain, HLSL compilation (`D3DCompile`), vertex/index/constant buffers, `Texture2D`, draw/draw_indexed, and pixel readback
- `d2d.goose` — Direct2D + DirectWrite GPU-accelerated antialiased 2D vector shapes and text rendering
- `gl.goose` — OpenGL 3.3 Core (WGL) context, GLSL shader compilation, uniforms, VAO/VBO/EBO meshes, `Texture2D`, and direct `extern "opengl32"` bindings
- `game.goose` — XInput gamepad input & vibration + `winmm` high-resolution multimedia timers
- `audio.goose` — WASAPI audio output & capture/loopback enumeration and shared-mode PCM render/capture streams + `winmm` `PlaySoundW`
- `video.goose` — Media Foundation (`IMFSourceReader`) video decoding directly into RGB32 `u32[:]` frames
- `webview.goose` — zero-dependency Microsoft Edge WebView2 host (`WebView`, runtime discovery, navigation, JS bridge, virtual hosts, settings, clipped bounds, and pull event queue)
- `types.goose` — C-ABI-compatible flat structs (`Point`, `Size`, `Rect`, `Event`, `FileInfo`, `MonitorInfo`, `GamepadState`, `AudioFormat`, `VideoInfo`, `ProcessEntry`, `PowerStatus`, etc.) and handle wrappers
- `constants.goose` — Win32 constants (`WS_*`, `WM_*`, `SW_*`, `VK_*`, `IDC_*`, `MB_*`, `MF_*`, `TPM_*`, `DT_*`, `CF_*`, `GENERIC_*`, `FILE_*`, `WAIT_*`, `MEM_*`, `PAGE_*`, `ES_*`, `XINPUT_*`, etc.)
- `utf16.goose` — pure Goose UTF-8 ⇔ UTF-16LE conversion helpers
- `include/win32_shim.h`, `src/win32_*.c` — thin Win32 C shim (`shim`, `dialogs`, `io`, `sync`, `wic`, `dib`, `d3d11`, `d2d`, `gl`, `system`, `net`, `display`, `desktop`, `controls`, `game`, `audio`, `video`, `webview`)
- `examples/hello_window.goose` — interactive GDI window demo
- `examples/menu_and_dialogs.goose` — menu bar, context menu, common dialogs, clipboard, and file I/O demo
- `examples/controls_and_desktop.goose` — native Win32 controls, DWM dark titlebar, display info, drag & drop, and system tray demo
- `examples/sysinfo_and_net.goose` — system info, volume info, process pipe capture, and Winsock2 DNS demo
- `examples/dib_plasma.goose` — real-time software DIB pixel buffer demo
- `examples/d3d11_triangle.goose` — real-time Direct3D 11 + HLSL spinning triangle demo
- `examples/textured_quad.goose` — Direct3D 11 textured indexed quad + WIC PNG + borderless fullscreen (F11) + Raw Input demo
- `examples/d2d_canvas.goose` — real-time Direct2D + DirectWrite antialiased vector & text demo
- `examples/gl_triangle.goose` — real-time OpenGL 3.3 Core + GLSL spinning triangle demo
- `examples/audio_and_gamepad.goose` — real-time WASAPI audio synth + XInput gamepad visualizer demo
- `test/win32_test.goose` — automated headless test suite

## Running & Building

```powershell
# Run automated tests (JIT)
goose run test/win32_test.goose

# Run interactive demos (JIT)
goose run examples/hello_window.goose
goose run examples/menu_and_dialogs.goose
goose run examples/controls_and_desktop.goose
goose run examples/sysinfo_and_net.goose
goose run examples/dib_plasma.goose
goose run examples/d3d11_triangle.goose
goose run examples/d2d_canvas.goose
goose run examples/gl_triangle.goose
goose run examples/audio_and_gamepad.goose

# Build standalone native executable
goose build examples/audio_and_gamepad.goose
```
