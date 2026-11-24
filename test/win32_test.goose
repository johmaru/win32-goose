// Automated test for the win32 package (runs headless using a hidden window).
import std;
import win32 from "..";

fn test_utf16_roundtrip() {
    let samples: const u8[:][4] = [
        "",
        "Hello, Win32!",
        "こんにちは Win32 ウィンドウ",
        "Goose 🪿 + 日本語 + ASCII",
    ];
    for i in samples.len {
        let s = samples[i];
        let w = win32::utf8_to_utf16z(s);
        assert(w.len >= 1 && w[w.len - 1] == 0);
        let back = win32::utf16_to_utf8(w);
        assert(back == s);
    }
    print("[OK] utf8 <-> utf16 roundtrip");
}

fn test_pure_externs() {
    let pid = win32::GetCurrentProcessId();
    let tid = win32::GetCurrentThreadId();
    assert(pid != 0);
    assert(tid != 0);

    let tick = win32::tick_count_ms();
    let freq = win32::perf_frequency();
    let qpc = win32::perf_counter();
    assert(tick > 0);
    assert(freq > 0);
    assert(qpc > 0);

    win32::SetLastError(5); // ERROR_ACCESS_DENIED
    assert(win32::last_error() == 5);
    let err_text = win32::format_error(5);
    assert(err_text.len > 0);

    let screen_w = win32::GetSystemMetrics(win32::SM_CXSCREEN);
    let screen_h = win32::GetSystemMetrics(win32::SM_CYSCREEN);
    assert(screen_w > 0 && screen_h > 0);

    print("[OK] pure kernel32/user32 externs (pid=", pid, ", screen=", screen_w, "x", screen_h, ")");
}

fn test_window_lifecycle() {
    let win = win32::create_window(
        "win32-goose test",
        320,
        240,
        win32::WS_OVERLAPPEDWINDOW
    );
    assert(win.is_valid());
    assert(win.exists());
    assert(!win.is_visible());

    let rc = win.client_rect();
    assert(rc.width() == 320);
    assert(rc.height() == 240);

    // UTF-8 title round-trip through w32_set_window_text + w32_get_window_text.
    let expected_title = "日本語タイトル 🪿";
    assert(win.set_title(expected_title));
    let actual_title = win.title();
    assert(actual_title == expected_title);

    // Double-buffered GDI paint session.
    var paint_rc = win32::Rect {};
    let dc = win.begin_paint(paint_rc);
    assert(dc.is_valid());
    assert(paint_rc.width() == 320 && paint_rc.height() == 240);
    assert(dc.fill_rect(paint_rc, win32::rgb(24, 28, 36)));
    dc.set_bk_mode(win32::TRANSPARENT);
    dc.set_text_color(win32::rgb(240, 240, 240));
    var text_rc = paint_rc;
    let text_h = dc.draw_text("Hello GDI", text_rc, win32::DT_CENTER | win32::DT_VCENTER | win32::DT_SINGLELINE);
    assert(text_h > 0);
    win.end_paint();

    // Menu bar + submenu + Unicode labels + state toggles + WM_COMMAND dispatch.
    let bar = win32::create_menu();
    assert(bar.is_valid());
    let file_menu = win32::create_popup_menu();
    assert(file_menu.is_valid());
    assert(file_menu.append_item(101, "開く(&O)..."));
    assert(file_menu.append_item(102, "ダークテーマ(&D)", win32::MF_STRING | win32::MF_CHECKED));
    assert(file_menu.append_separator());
    assert(file_menu.append_item(103, "無効項目", win32::MF_STRING | win32::MF_GRAYED));
    assert(file_menu.item_count() == 4);
    assert(file_menu.item_text(101) == "開く(&O)...");
    assert(file_menu.is_checked(102));
    assert(!file_menu.is_enabled(103));
    assert(file_menu.set_checked(102, false));
    assert(!file_menu.is_checked(102));
    assert(file_menu.set_enabled(103, true));
    assert(file_menu.is_enabled(103));

    assert(bar.append_submenu(file_menu, "ファイル(&F)"));
    assert(bar.item_count() == 1);
    assert(win.set_menu(bar));
    assert(win.menu().raw == bar.raw);
    assert(win.draw_menu_bar());

    // Simulate a WM_COMMAND from menu item 101 (notify code 0) and verify Event decoding.
    assert(win.post_message(win32::WM_COMMAND, 101, 0));

    // Post WM_CLOSE -> handle in event loop -> destroy -> observe WM_DESTROY -> PostQuitMessage -> observe WM_QUIT.
    assert(win.close());
    var saw_cmd = false;
    var saw_close = false;
    var saw_destroy = false;
    var saw_quit = false;
    var ev = win32::Event {};
    for step in 64 {
        guard win32::poll_event(ev) else {
            if saw_quit { break; }
            win32::Sleep(1);
            continue;
        }
        if ev.msg == win32::WM_COMMAND && ev.hwnd == win.raw {
            assert(ev.command_id == 101);
            assert(ev.notify_code == 0);
            saw_cmd = true;
        } else if ev.msg == win32::WM_CLOSE && ev.hwnd == win.raw {
            saw_close = true;
            assert(win.destroy());
        } else if ev.msg == win32::WM_DESTROY && ev.hwnd == win.raw {
            saw_destroy = true;
            win32::post_quit_message(0);
        } else if ev.msg == win32::WM_QUIT {
            saw_quit = true;
            break;
        }
    }
    assert(saw_cmd);
    assert(saw_close);
    assert(saw_destroy);
    assert(saw_quit);
    assert(!win.exists());

    print("[OK] window lifecycle, menu bar, WM_COMMAND, GDI paint, and event loop");
}

fn test_clipboard() {
    // Preserve any existing text on the user's clipboard and restore it at the end.
    var saved: u8[>..] = [];
    let had_saved = win32::clipboard_get_text(saved);

    let seq0 = win32::clipboard_sequence_number();
    let sample = "Goose Win32 クリップボード 🪿";
    assert(win32::clipboard_set_text(sample));
    assert(win32::clipboard_has_text());
    assert(win32::clipboard_sequence_number() != seq0);

    let got = win32::clipboard_get_text();
    assert(got == sample);

    // Custom registered binary clipboard format.
    let fmt = win32::register_clipboard_format("Win32Goose.TestBinaryFormat");
    assert(fmt != 0);
    let payload: u8[6] = [0x47, 0x4F, 0x4F, 0x53, 0x45, 0x00];
    assert(win32::clipboard_set_bytes(fmt, payload));
    assert(win32::clipboard_has_format(fmt));
    var got_bin: u8[>..] = [];
    assert(win32::clipboard_get_bytes(fmt, got_bin));
    assert(got_bin.len >= 6);
    for i in 6 {
        assert(got_bin[i] == payload[i]);
    }

    assert(win32::clipboard_clear());
    assert(!win32::clipboard_has_text());

    if had_saved {
        win32::clipboard_set_text(saved);
    }

    print("[OK] clipboard (Unicode text, custom binary format, sequence, clear)");
}

fn test_file_io() {
    let tmp = win32::temp_dir();
    assert(tmp.len > 0);
    let cwd = win32::current_dir();
    assert(cwd.len > 0);
    let abs = win32::full_path(".");
    assert(abs.len > 0);

    var dir: u8[>..] = [];
    format(dir, tmp, "win32_goose_test_", win32::GetCurrentProcessId());
    if !win32::dir_exists(dir) {
        assert(win32::create_directory(dir));
    }
    assert(win32::dir_exists(dir));
    assert(!win32::file_exists(dir));

    // Handle-based streaming I/O (CreateFileW + direct ReadFile/WriteFile/Seek/Truncate).
    var p1: u8[>..] = [];
    format(p1, dir, "\\stream_テスト_🪿.bin");

    let wf = win32::open_file_write(p1);
    assert(wf.is_valid());
    assert(wf.write_all("Hello Win32 File I/O! 日本語"));
    assert(wf.flush());
    let sz1, sz1_ok = wf.size();
    assert(sz1_ok && sz1 == 31);

    // Seek & overwrite in place, then truncate.
    let pos1, pos1_ok = wf.seek(6, win32::FILE_BEGIN);
    assert(pos1_ok && pos1 == 6);
    assert(wf.write_all("Goose"));
    let cur, cur_ok = wf.tell();
    assert(cur_ok && cur == 11);
    let end_pos, end_ok = wf.seek(-9, win32::FILE_END);
    assert(end_ok && end_pos == 22);
    assert(wf.truncate());

    var fi = win32::FileInfo {};
    assert(wf.info(fi));
    assert(fi.size == 22);
    assert(!fi.is_directory());
    assert(wf.close());

    assert(win32::file_exists(p1));

    // Read back through Hfile.read().
    let rf = win32::open_file_read(p1);
    assert(rf.is_valid());
    var rbuf: u8[64] = [0; 64];
    let nread, rok = rf.read(rbuf);
    assert(rok && nread == 22);
    assert(rbuf[..nread] == "Hello Goose File I/O! ");
    assert(rf.close());

    // Whole-file helpers + copy/move.
    var p2: u8[>..] = [];
    format(p2, dir, "\\copy.txt");
    var p3: u8[>..] = [];
    format(p3, dir, "\\moved.txt");

    assert(win32::copy_file(p1, p2, false));
    assert(win32::file_exists(p2));
    assert(win32::move_file(p2, p3, true));
    assert(!win32::file_exists(p2));
    assert(win32::file_exists(p3));

    var all_bytes: u8[>..] = [];
    assert(win32::read_file_bytes(p3, all_bytes));
    assert(all_bytes == "Hello Goose File I/O! ");

    assert(win32::write_file_bytes(p3, "overwritten"));
    all_bytes.clear();
    assert(win32::read_file_bytes(p3, all_bytes));
    assert(all_bytes == "overwritten");

    // Directory enumeration via FindFirstFileW / FindNextFileW.
    var pat: u8[>..] = [];
    format(pat, dir, "\\*");
    var entry_name: u8[>..] = [];
    var entry_info = win32::FileInfo {};
    let hf = win32::find_first_file(pat, entry_name, entry_info);
    assert(hf.is_valid());
    var saw_stream = false;
    var saw_moved = false;
    while true {
        if entry_name == "stream_テスト_🪿.bin" {
            assert(entry_info.size == 22);
            saw_stream = true;
        } else if entry_name == "moved.txt" {
            assert(entry_info.size == 11);
            saw_moved = true;
        }
        entry_name.clear();
        guard hf.next(entry_name, entry_info) else { break; }
    }
    assert(hf.close());
    assert(saw_stream && saw_moved);

    // Cleanup temp files and directory.
    assert(win32::delete_file(p1));
    assert(win32::delete_file(p3));
    assert(win32::remove_directory(dir));
    assert(!win32::dir_exists(dir));

    print("[OK] file I/O (Hfile read/write/seek/truncate, Unicode paths, copy/move, FindFirstFileW)");
}

fn test_sync_objects() {
    let pid = win32::GetCurrentProcessId();

    // 1. Named Mutex (single-instance check + OpenMutexW + recursive lock/release).
    var mname: u8[>..] = [];
    format(mname, "Local\\Win32GooseTestMutex_", pid);
    let m1, m1_exists = win32::create_mutex(true, mname);
    assert(m1.is_valid());
    assert(!m1_exists);

    let m2, m2_exists = win32::create_mutex(false, mname);
    assert(m2.is_valid());
    assert(m2_exists);

    let m3 = win32::open_mutex(mname);
    assert(m3.is_valid());
    assert(m3.lock(0)); // same thread owns mutex, recursive acquire succeeds
    assert(m3.release());
    assert(m1.release());
    assert(m3.close());
    assert(m2.close());
    assert(m1.close());

    // 2. Named Semaphore (initial=1, max=3, acquire/timeout/release_prev).
    var sname: u8[>..] = [];
    format(sname, "Local\\Win32GooseTestSem_", pid);
    let s1, s1_exists = win32::create_semaphore(1, 3, sname);
    assert(s1.is_valid() && !s1_exists);
    let s2 = win32::open_semaphore(sname);
    assert(s2.is_valid());

    assert(s1.acquire(0)); // count: 1 -> 0
    assert(s1.wait(0) == win32::WAIT_TIMEOUT); // count == 0
    var prev: i32 = -1;
    assert(s2.release_prev(2, prev)); // count: 0 -> 2
    assert(prev == 0);
    assert(s1.acquire(0)); // 2 -> 1
    assert(s1.acquire(0)); // 1 -> 0
    assert(s1.wait(0) == win32::WAIT_TIMEOUT);
    assert(s2.close());
    assert(s1.close());

    // 3. Manual-reset and Auto-reset Events + WaitForMultipleObjects.
    var ename: u8[>..] = [];
    format(ename, "Local\\Win32GooseTestEvent_", pid);
    let ev_manual, ev_exists = win32::create_event(true, false, ename);
    assert(ev_manual.is_valid() && !ev_exists);
    let ev_open = win32::open_event(ename);
    assert(ev_open.is_valid());
    let ev_auto, _ = win32::create_event(false, false);
    assert(ev_auto.is_valid());

    assert(ev_manual.wait(0) == win32::WAIT_TIMEOUT);
    assert(ev_open.set());
    // Manual reset stays signaled across multiple waits.
    assert(ev_manual.wait(0) == win32::WAIT_OBJECT_0);
    assert(ev_manual.wait(0) == win32::WAIT_OBJECT_0);

    // WaitForMultipleObjects: wait_all=false sees index 0 (ev_manual); wait_all=true times out until ev_auto is set.
    let handles: u64[2] = [ev_manual.raw, ev_auto.raw];
    assert(win32::wait_multiple(handles, false, 0) == win32::WAIT_OBJECT_0);
    assert(win32::wait_multiple(handles, true, 0) == win32::WAIT_TIMEOUT);
    assert(ev_auto.set());
    assert(win32::wait_multiple(handles, true, 0) == win32::WAIT_OBJECT_0);
    // Auto-reset event was automatically reset by the wait.
    assert(ev_auto.wait(0) == win32::WAIT_TIMEOUT);
    assert(ev_manual.reset());
    assert(ev_manual.wait(0) == win32::WAIT_TIMEOUT);
    assert(ev_auto.close());
    assert(ev_open.close());
    assert(ev_manual.close());

    // 4. Waitable Timer (15ms relative delay, measured via QueryPerformanceCounter).
    let timer, _ = win32::create_waitable_timer(true);
    assert(timer.is_valid());
    let freq = win32::perf_frequency();
    let t0 = win32::perf_counter();
    assert(timer.set_relative_ms(15));
    assert(timer.wait(1000) == win32::WAIT_OBJECT_0);
    let elapsed_ms = ((win32::perf_counter() - t0) * 1000) / freq;
    assert(elapsed_ms >= 5);
    assert(timer.cancel());
    assert(timer.close());

    print("[OK] sync objects (Hmutex, Hsemaphore, Hevent, Htimer, WaitForMultipleObjects)");
}

fn test_dib_and_d3d11() {
    // 1. DIB software pixel buffer + StretchDIBits blit into hidden window DC.
    let win = win32::create_window("win32-goose gfx test", 128, 128, win32::WS_OVERLAPPEDWINDOW);
    assert(win.is_valid());

    var pixels: u32[64 * 64] = [0; 64 * 64];
    win32::clear_pixels(pixels, win32::xrgb(10, 20, 30));
    win32::fill_pixel_rect(pixels, 64, 64, 8, 8, 16, 16, win32::xrgb(250, 120, 40));
    win32::fill_pixel_circle(pixels, 64, 64, 32, 32, 10, win32::xrgb(50, 220, 100));
    assert(pixels[0] == win32::xrgb(10, 20, 30));
    assert(pixels[10 * 64 + 10] == win32::xrgb(250, 120, 40));
    assert(pixels[32 * 64 + 32] == win32::xrgb(50, 220, 100));

    var paint_rc = win32::Rect {};
    let dc = win.begin_paint(paint_rc);
    assert(dc.is_valid());
    assert(dc.blit_pixels(pixels, 64, 64, 0, 0, paint_rc.width(), paint_rc.height()));
    win.end_paint();
    assert(win32::present_pixels(win, pixels, 64, 64));

    // 2. Direct3D 11 device + HLSL shader + GPU draw + staging pixel readback!
    let dev = win32::d3d11_create(win, false);
    assert(dev.is_valid());
    let adapter = dev.adapter_name();
    assert(adapter.len > 0);

    let hlsl = """
struct VSIn { float3 pos : POSITION; float4 col : COLOR; };
struct PSIn { float4 pos : SV_POSITION; float4 col : COLOR; };
PSIn vs_main(VSIn v) { PSIn o; o.pos = float4(v.pos, 1.0); o.col = v.col; return o; }
float4 ps_main(PSIn p) : SV_TARGET { return p.col; }
""";
    var err: u8[>..] = [];
    let pipe = dev.create_pipeline(hlsl, err);
    assert(pipe.is_valid());

    // Fullscreen green triangle (0, 255, 0, 255) over a red clear background.
    let verts: win32::D3dVertex[3] = [
        win32::D3dVertex { x: -1.0, y: -1.0, z: 0.0, r: 0.0, g: 1.0, b: 0.0, a: 1.0 },
        win32::D3dVertex { x: -1.0, y:  3.0, z: 0.0, r: 0.0, g: 1.0, b: 0.0, a: 1.0 },
        win32::D3dVertex { x:  3.0, y: -1.0, z: 0.0, r: 0.0, g: 1.0, b: 0.0, a: 1.0 },
    ];
    let vbuf = dev.create_vertex_buffer(verts, false);
    assert(vbuf.is_valid());

    dev.begin_frame(1.0, 0.0, 0.0, 1.0);
    dev.bind_pipeline(pipe);
    dev.bind_vertex_buffer(vbuf);
    dev.draw(3);

    // Verify center pixel rendered by the HLSL pixel shader is bright green (R~0, G~255, B~0).
    var px: u32 = 0;
    assert(dev.readback_pixel(64, 64, px));
    let r = px & 0xFF;
    let g = (px >> 8) & 0xFF;
    let b = (px >> 16) & 0xFF;
    assert(r < 8 && g > 245 && b < 8);

    assert(dev.end_frame());
    assert(dev.resize(160, 120));

    vbuf.destroy();
    pipe.destroy();
    dev.destroy();

    // 3. Direct2D + DirectWrite GPU 2D vector & text rendering on the hidden window.
    let d2d = win32::d2d_create(win, false);
    assert(d2d.is_valid());
    let font = d2d.create_font("Segoe UI", 16.0, 600);
    assert(font.is_valid());
    font.set_align(win32::D2D_ALIGN_CENTER, win32::D2D_VALIGN_CENTER);

    d2d.begin_draw();
    d2d.set_identity();
    d2d.clear(win32::colorf_rgb(15, 23, 42));
    d2d.fill_rounded_rect(8.0, 8.0, 112.0, 112.0, 12.0, 12.0, win32::colorf(0.2, 0.4, 0.8, 0.9));
    d2d.draw_rounded_rect(8.0, 8.0, 112.0, 112.0, 12.0, 12.0, win32::colorf(1.0, 1.0, 1.0, 1.0), 2.0);
    d2d.fill_ellipse(64.0, 64.0, 24.0, 24.0, win32::colorf(0.9, 0.8, 0.1, 0.8));
    d2d.draw_line(0.0, 0.0, 128.0, 128.0, win32::colorf(1.0, 1.0, 1.0, 0.5), 1.5);
    d2d.draw_text("Direct2D 日本語", font, 0.0, 0.0, 128.0, 128.0, win32::colorf(1.0, 1.0, 1.0, 1.0));
    assert(d2d.end_draw());
    assert(d2d.resize(160, 120));

    font.destroy();
    d2d.destroy();
    assert(win.destroy());

    // 4. OpenGL 3.3 Core (WGL) + GLSL GPU render & glReadPixels verification on a fresh hidden window.
    let gl_win = win32::create_window("win32-goose gl test", 128, 128, win32::WS_OVERLAPPEDWINDOW);
    assert(gl_win.is_valid());
    let gl = win32::gl_create(gl_win, false);
    assert(gl.is_valid());
    let gl_ver = gl.version();
    assert(gl_ver.len > 0);
    assert(gl.renderer_name().len > 0);

    let vs_src = """
#version 330 core
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec4 a_col;
out vec4 v_col;
void main() { gl_Position = vec4(a_pos, 1.0); v_col = a_col; }
""";
    let fs_src = """
#version 330 core
in vec4 v_col;
uniform float u_scale;
out vec4 frag_color;
void main() { frag_color = vec4(v_col.rgb * u_scale, v_col.a); }
""";
    var gl_err: u8[>..] = [];
    let prog = gl.create_program(vs_src, fs_src, gl_err);
    assert(prog.is_valid());

    // Fullscreen bright-blue triangle (0, 0, 255, 255) over a red clear.
    let gl_verts: win32::D3dVertex[3] = [
        win32::D3dVertex { x: -1.0, y: -1.0, z: 0.0, r: 0.0, g: 0.0, b: 1.0, a: 1.0 },
        win32::D3dVertex { x: -1.0, y:  3.0, z: 0.0, r: 0.0, g: 0.0, b: 1.0, a: 1.0 },
        win32::D3dVertex { x:  3.0, y: -1.0, z: 0.0, r: 0.0, g: 0.0, b: 1.0, a: 1.0 },
    ];
    let mesh = gl.create_mesh(gl_verts, false);
    assert(mesh.is_valid());

    gl.begin_frame(1.0, 0.0, 0.0, 1.0);
    gl.use_program(prog);
    prog.set_uniform("u_scale", 1.0);
    gl.bind_mesh(mesh);
    gl.draw(3);

    var gl_px: u32 = 0;
    assert(gl.readback_pixel(64, 64, gl_px));
    let gl_r = gl_px & 0xFF;
    let gl_g = (gl_px >> 8) & 0xFF;
    let gl_b = (gl_px >> 16) & 0xFF;
    assert(gl_r < 8 && gl_g < 8 && gl_b > 245);
    assert(gl.end_frame());

    mesh.destroy();
    prog.destroy();
    gl.destroy();
    assert(gl_win.destroy());

    print("[OK] DIB + D3D11 + Direct2D/DirectWrite + OpenGL ", gl_ver, " (", adapter, ")");
}

fn test_storage_extensions() {
    // 1. Disk & volume info.
    assert(win32::logical_drives_mask() != 0);
    var ds = win32::DiskSpace {};
    assert(win32::disk_free_space("C:\\", ds));
    assert(ds.total_bytes > 0 && ds.free_bytes <= ds.total_bytes);

    var label: u8[>..] = [];
    var fs: u8[>..] = [];
    var serial: u32 = 0;
    assert(win32::volume_info("C:\\", label, fs, serial));
    assert(fs.len > 0);

    // 2. Memory-mapped file + Directory change notification (using `defer`!).
    let tmp = win32::temp_dir();
    var dir: u8[>..] = [];
    format(dir, tmp, "win32_goose_phase_a_", win32::GetCurrentProcessId());
    if !win32::dir_exists(dir) { assert(win32::create_directory(dir)); }
    defer win32::remove_directory(dir);

    let watch = win32::watch_dir_open(dir, false, win32::FILE_NOTIFY_CHANGE_FILE_NAME | win32::FILE_NOTIFY_CHANGE_LAST_WRITE);
    assert(watch.is_valid());
    defer watch.close();

    var mpath: u8[>..] = [];
    format(mpath, dir, "\\mmap.bin");
    defer win32::delete_file(mpath);
    {
        let f = win32::open_file_write(mpath);
        assert(f.is_valid());
        defer f.close();
        assert(f.write_all("0123456789ABCDEF"));
        let mm = win32::file_map_open(f, true);
        assert(mm.is_valid());
        defer mm.close();
        assert(mm.size() == 16);
        assert(mm.write(4, "GOOSE") == 5);
        assert(mm.flush());
        var out: u8[16] = [0; 16];
        assert(mm.read(0, out) == 16);
        assert(out == "0123GOOSE9ABCDEF");
    }

    // Creating/writing mmap.bin inside `dir` must trigger the directory watcher.
    assert(watch.wait(500) == win32::WAIT_OBJECT_0);
    assert(watch.next());

    print("[OK] storage extensions (memory-mapped file, disk/volume ", fs, ", dir watch)");
}

fn test_system_services() {
    var si = win32::SystemInfo {};
    win32::system_info(si);
    assert(si.processor_count >= 1 && si.page_size >= 4096);

    var ms = win32::MemoryStatus {};
    assert(win32::memory_status(ms));
    assert(ms.total_phys > 0);

    var lt = win32::SystemTime {};
    var st = win32::SystemTime {};
    win32::local_time(lt);
    win32::system_time(st);
    assert(lt.year >= 2024 && st.year >= 2024);

    assert(win32::computer_name().len > 0);
    assert(win32::user_name().len > 0);
    assert(win32::module_file_name().len > 0);

    // Environment variables.
    assert(win32::env_set("WIN32_GOOSE_TEST_ENV", "日本語_env_42"));
    var env_val: u8[>..] = [];
    assert(win32::env_get("WIN32_GOOSE_TEST_ENV", env_val));
    assert(env_val == "日本語_env_42");
    assert(win32::env_delete("WIN32_GOOSE_TEST_ENV"));
    env_val.clear();
    assert(!win32::env_get("WIN32_GOOSE_TEST_ENV", env_val));

    // Process execution + stdout pipe capture.
    var captured: u8[>..] = [];
    var exit_code: u32 = 99;
    assert(win32::run_process_capture("cmd.exe /c echo win32-goose-ok", captured, exit_code, 5000));
    assert(exit_code == 0);
    assert(captured.len >= 14);

    let proc = win32::spawn_process("cmd.exe /c exit 7", "", true);
    assert(proc.is_valid());
    defer proc.close();
    assert(proc.wait(5000) == win32::WAIT_OBJECT_0);
    var pcode: u32 = 0;
    assert(proc.exit_code(pcode) && pcode == 7);

    // Registry under HKCU\Software\Win32GooseTest_<pid> (cleaned up via defer).
    var subkey: u8[>..] = [];
    format(subkey, "Software\\Win32GooseTest_", win32::GetCurrentProcessId());
    defer win32::reg_delete_key(win32::HKEY_CURRENT_USER, subkey);
    {
        let k = win32::reg_create_key(win32::HKEY_CURRENT_USER, subkey);
        assert(k.is_valid());
        defer k.close();
        assert(k.set_string("Greeting", "こんにちは Registry 🪿"));
        assert(k.set_u32("Answer", 42));
        assert(k.set_u64("Big", 0x123456789ABCDEF0));
        let raw_b: u8[4] = [1, 2, 3, 4];
        assert(k.set_bytes("Bin", raw_b));
    }
    {
        let k = win32::reg_open_key(win32::HKEY_CURRENT_USER, subkey, win32::KEY_ALL_ACCESS);
        assert(k.is_valid());
        defer k.close();
        var s: u8[>..] = [];
        assert(k.get_string("Greeting", s) && s == "こんにちは Registry 🪿");
        var v32: u32 = 0;
        assert(k.get_u32("Answer", v32) && v32 == 42);
        var v64: u64 = 0;
        assert(k.get_u64("Big", v64) && v64 == 0x123456789ABCDEF0);
        var b: u8[>..] = [];
        assert(k.get_bytes("Bin", b) && b.len == 4 && b[3] == 4);
        assert(k.delete_value("Bin"));
    }

    // Running process enumeration (Toolhelp32): current PID must appear.
    {
        let my_pid = win32::GetCurrentProcessId();
        let snap = win32::process_snapshot_open();
        assert(snap.is_valid());
        defer snap.close();
        var pe = win32::ProcessEntry {};
        var exe: u8[>..] = [];
        var found_self = false;
        var count: i32 = 0;
        if snap.first_process(pe, exe) {
            while true {
                count += 1;
                if pe.pid == my_pid {
                    assert(exe.len > 0);
                    found_self = true;
                }
                exe.clear();
                if !snap.next_process(pe, exe) { break; }
            }
        }
        assert(count > 0 && found_self);
    }

    // Dynamic DLL loading (LoadLibraryW / GetProcAddress / FreeLibrary).
    {
        let k32 = win32::load_library("kernel32.dll");
        assert(k32.is_valid());
        defer k32.free();
        assert(k32.proc_address("GetCurrentProcessId") != 0);
        assert(k32.proc_address("NonExistentWin32GooseSymbol_XYZ") == 0);
    }

    // Virtual memory (VirtualAlloc / VirtualProtect / VirtualFree).
    {
        let mem = win32::virtual_alloc(4096);
        assert(mem != 0);
        defer win32::virtual_free(mem);
        var old_prot: u32 = 0;
        assert(win32::virtual_protect(mem, 4096, win32::PAGE_READONLY, old_prot));
        assert(old_prot == win32::PAGE_READWRITE);
    }

    // Power status & sleep prevention.
    {
        var ps = win32::PowerStatus {};
        assert(win32::power_status(ps));
        assert(win32::prevent_sleep(false));
        defer win32::allow_sleep();
    }

    // Named Pipe IPC (nonblocking single-process duplex round-trip).
    {
        var pipe_name: u8[>..] = [];
        format(pipe_name, "\\\\.\\pipe\\win32_goose_test_", win32::GetCurrentProcessId());
        let srv = win32::named_pipe_create(pipe_name, 1, 4096, 4096, true);
        assert(srv.is_valid());
        defer srv.close();

        let cli = win32::named_pipe_open(pipe_name, 1000);
        assert(cli.is_valid());
        defer cli.close();

        assert(cli.write_all("pipe hello!"));
        var pbuf: u8[64] = [0; 64];
        let n1, ok1 = srv.read(pbuf);
        assert(ok1 && n1 == 11 && pbuf[..n1] == "pipe hello!");
        assert(srv.write_all("pipe reply"));
        let n2, ok2 = cli.read(pbuf);
        assert(ok2 && n2 == 10 && pbuf[..n2] == "pipe reply");
        srv.pipe_disconnect();
    }

    print("[OK] system services (sysinfo, memory, env, process+pipe, registry, toolhelp, dll, vmem, power, named-pipe)");
}

fn test_networking() {
    assert(win32::net_init());

    // 1. DNS resolve localhost.
    var ip: u8[>..] = [];
    assert(win32::dns_resolve_ipv4("localhost", ip));
    assert(ip == "127.0.0.1");

    // 2. TCP loopback server/client on ephemeral port.
    let srv = win32::tcp_listen("127.0.0.1", 0);
    assert(srv.is_valid());
    defer srv.close();
    let port = srv.local_port();
    assert(port != 0);

    let cli = win32::tcp_connect("127.0.0.1", port, 2000);
    assert(cli.is_valid());
    defer cli.close();
    let peer = srv.accept();
    assert(peer.is_valid());
    defer peer.close();

    assert(cli.set_timeout(2000, 2000));
    assert(peer.set_timeout(2000, 2000));
    assert(cli.send_all("ping from goose"));
    var rbuf: u8[64] = [0; 64];
    let n1 = peer.recv(rbuf);
    assert(n1 == 15 && rbuf[..n1] == "ping from goose");
    assert(peer.send_all("pong!"));
    let n2 = cli.recv(rbuf);
    assert(n2 == 5 && rbuf[..n2] == "pong!");

    // 2b. WSAPoll readability / writability check.
    assert(cli.poll_writable(100) > 0);
    assert(peer.poll_readable(0) == 0);
    assert(cli.send_all("poll!"));
    assert(peer.poll_readable(1000) > 0);
    let np = peer.recv(rbuf);
    assert(np == 5 && rbuf[..np] == "poll!");

    // 3. UDP loopback send_to / recv_from.
    let u1 = win32::udp_bind("127.0.0.1", 0);
    let u2 = win32::udp_bind("127.0.0.1", 0);
    assert(u1.is_valid() && u2.is_valid());
    defer u1.close();
    defer u2.close();
    assert(u2.set_timeout(2000, 2000));
    let u2_port = u2.local_port();
    assert(u1.send_to("127.0.0.1", u2_port, "udp hello") == 9);
    var from_host: u8[>..] = [];
    var from_port: u16 = 0;
    let n3 = u2.recv_from(rbuf, from_host, from_port);
    assert(n3 == 9 && rbuf[..n3] == "udp hello");
    assert(from_host == "127.0.0.1" && from_port == u1.local_port());

    // 4. WinHTTP offline sanity check (invalid URL returns false cleanly; verifies linkage).
    var hbody: u8[>..] = [];
    var hstatus: u32 = 0;
    assert(!win32::http_get("not-a-valid-url", hbody, hstatus, 500));
    var resp_hdrs: u8[>..] = [];
    assert(!win32::http_request_ex("GET", "not-a-valid-url", "X-Test: 1\r\n", "", hbody, hstatus, resp_hdrs, 500));

    print("[OK] networking (DNS, Winsock2 TCP/UDP/WSAPoll loopback, WinHTTP+ex)");
}

fn test_display_devices() {
    let n = win32::monitor_count();
    assert(n >= 1);

    var mi = win32::MonitorInfo {};
    var dev_name: u8[>..] = [];
    assert(win32::monitor_info(0, mi, dev_name));
    assert(mi.width() > 0 && mi.height() > 0);
    assert(mi.dpi_x >= 96 && mi.dpi_y >= 96);
    assert(dev_name.len > 0);

    var dname: u8[>..] = [];
    var ddesc: u8[>..] = [];
    var active = false;
    assert(win32::display_device(0, dname, ddesc, active));
    assert(dname.len > 0 && ddesc.len > 0);

    var cur_mode = win32::DisplayMode {};
    assert(win32::current_display_mode(dev_name, cur_mode));
    assert(cur_mode.width > 0 && cur_mode.height > 0 && cur_mode.frequency > 0);

    var mode0 = win32::DisplayMode {};
    assert(win32::display_mode_at(dev_name, 0, mode0));
    assert(mode0.width > 0 && mode0.height > 0);

    print("[OK] display devices (", n, " monitor(s), ", cur_mode.width, "x", cur_mode.height, "@", cur_mode.frequency, "Hz, DPI=", mi.dpi_x, ")");
}

fn test_desktop_and_controls() {
    // 1. Known folders.
    let desktop = win32::known_folder_path(win32::KNOWN_FOLDER_DESKTOP);
    let docs = win32::known_folder_path(win32::KNOWN_FOLDER_DOCUMENTS);
    let local_app = win32::known_folder_path(win32::KNOWN_FOLDER_LOCAL_APPDATA);
    assert(desktop.len > 0 && win32::dir_exists(desktop));
    assert(docs.len > 0 && win32::dir_exists(docs));
    assert(local_app.len > 0 && win32::dir_exists(local_app));

    // 2. Hidden window + DWM + Tray + Controls + Drag&Drop + Hotkey.
    assert(win32::init_common_controls());
    let win = win32::create_window("win32-goose controls test", 400, 320, win32::WS_OVERLAPPEDWINDOW);
    assert(win.is_valid());
    defer win.destroy();

    // DWM styling (dark titlebar + rounded corners + flush).
    win.set_dark_titlebar(true);
    win.set_corner_preference(win32::DWMWCP_ROUND);
    win32::dwm_flush();

    // System tray add/update/remove (may be restricted in non-interactive sessions).
    if win.tray_add(1, "win32-goose test tray") {
        defer win.tray_remove(1);
        assert(win.tray_set_tooltip(1, "日本語トレイ 🪿"));
    }

    // Standard controls.
    let btn = win.create_button(201, "押す(&P)", 10, 10, 100, 28);
    let cb = win.create_checkbox(202, "ダークモード", 120, 10, 120, 28, true);
    let lbl = win.create_label(203, "ラベル", 10, 46, 200, 20);
    let edit = win.create_edit(204, "初期テキスト", 10, 72, 240, 24);
    let lb = win.create_listbox(205, 10, 104, 180, 100);
    let combo = win.create_combobox(206, 200, 104, 160, 120);
    let pb = win.create_progressbar(207, 10, 214, 350, 20, 100);
    assert(btn.is_valid() && cb.is_valid() && lbl.is_valid() && edit.is_valid());
    assert(lb.is_valid() && combo.is_valid() && pb.is_valid());

    assert(cb.is_checked());
    cb.set_checked(false);
    assert(!cb.is_checked());

    assert(edit.title() == "初期テキスト");
    assert(edit.set_title("更新済み 🪿"));
    assert(edit.title() == "更新済み 🪿");

    assert(lb.listbox_add("項目A") == 0);
    assert(lb.listbox_add("項目B 日本語") == 1);
    assert(lb.listbox_count() == 2);
    lb.listbox_set_selected(1);
    assert(lb.listbox_selected() == 1);
    assert(lb.listbox_item_text(1) == "項目B 日本語");

    assert(combo.combobox_add("選択肢1") == 0);
    assert(combo.combobox_add("選択肢2") == 1);
    assert(combo.combobox_count() == 2);
    combo.combobox_set_selected(0);
    assert(combo.combobox_selected() == 0);
    assert(combo.combobox_item_text(0) == "選択肢1");

    pb.progress_set_pos(65);
    assert(pb.progress_pos() == 65);

    // Trackbar / Slider, Tab Control & Status Bar.
    let tb = win.create_trackbar(208, 10, 240, 200, 30, 0, 100);
    let tab = win.create_tabcontrol(209, 10, 275, 220, 40);
    let sb = win.create_statusbar(210, "準備完了");
    assert(tb.is_valid() && tab.is_valid() && sb.is_valid());

    tb.trackbar_set_pos(42);
    assert(tb.trackbar_pos() == 42);

    assert(tab.tab_add("一般") == 0);
    assert(tab.tab_add("詳細設定") == 1);
    assert(tab.tab_count() == 2);
    tab.tab_set_selected(1);
    assert(tab.tab_selected() == 1);

    assert(sb.statusbar_set_text("ステータス更新 🪿"));
    sb.statusbar_auto_size();

    // Enable/disable + Drag&Drop + Global Hotkey (Ctrl+Alt+Shift+F12).
    btn.enable(false);
    assert(!btn.is_enabled());
    btn.enable(true);
    assert(btn.is_enabled());

    win.accept_drop_files(true);
    assert(win32::dropped_file_count() == 0);

    let hk_ok = win.register_hotkey(77, win32::MOD_CONTROL | win32::MOD_ALT | win32::MOD_SHIFT | win32::MOD_NOREPEAT, win32::VK_F12);
    if hk_ok {
        assert(win.unregister_hotkey(77));
    }

    print("[OK] desktop (DWM, known folders, tray) + UI controls + drag&drop + hotkey");
}

fn test_phase_c() {
    // 1. Gaming: winmm timer + XInput polling.
    assert(win32::timeBeginPeriod(1) == 0);
    defer win32::timeEndPeriod(1);
    assert(win32::timeGetTime() > 0);

    var connected_pads: i32 = 0;
    for i in 4 {
        var gp = win32::GamepadState {};
        if win32::xinput_get_state(i as! u32, gp) {
            assert(gp.connected);
            let lx = gp.left_stick_x();
            assert(lx >= -1.0 && lx <= 1.0);
            connected_pads += 1;
        }
    }

    // 2. Audio: WASAPI device enumeration & silent PCM render stream.
    let dev_cnt = win32::audio_output_device_count();
    var default_dev: u8[>..] = [];
    win32::audio_default_output_name(default_dev);
    if dev_cnt > 0 {
        var first_dev: u8[>..] = [];
        assert(win32::audio_output_device_name(0, first_dev));
        assert(first_dev.len > 0);

        var fmt = win32::AudioFormat {};
        let audio = win32::wasapi_open_default(fmt);
        if audio.is_valid() {
            defer audio.close();
            assert(fmt.sample_rate >= 8000 && fmt.channels >= 1 && fmt.buffer_frames > 0);
            let avail = audio.available_frames();
            if avail > 0 {
                // Feed silent frames (0.0) so automated tests stay quiet.
                var silence: f32[256] = [0.0; 256];
                let wrote = audio.write_f32(silence);
                assert(wrote > 0);
            }
            audio.stop();
            audio.start();
        }
    }

    // 2b. WASAPI audio capture enumeration + loopback capture test.
    let in_cnt = win32::audio_input_device_count();
    let _ = win32::audio_default_input_name();
    if dev_cnt > 0 {
        var cap_fmt = win32::AudioFormat {};
        let cap = win32::wasapi_capture_open(cap_fmt, true);
        if cap.is_valid() {
            defer cap.close();
            assert(cap_fmt.sample_rate >= 8000 && cap_fmt.channels >= 1);
            win32::Sleep(15);
            var cap_samples: f32[>..] = [];
            let _ = cap.read_f32(cap_samples);
            cap.stop();
            cap.start();
        }
    }

    // 3. Video: Media Foundation startup, error path, and shutdown.
    assert(win32::mf_startup());
    defer win32::mf_shutdown();
    var vinfo = win32::VideoInfo {};
    let bad_vid = win32::video_open("Z:\\nonexistent_win32_goose_video.mp4", vinfo);
    assert(!bad_vid.is_valid());

    print("[OK] gaming/audio/video (XInput pads=", connected_pads, ", WASAPI out=", dev_cnt, " in=", in_cnt, ", default=", default_dev, ", MF ready)");
}

fn test_wic_and_gpu_textures() {
    // 1. WIC PNG & BMP save/load round-trip.
    let tmp = win32::temp_dir();
    assert(tmp.len > 0);
    var png_path: u8[>..] = [];
    var bmp_path: u8[>..] = [];
    format(png_path, tmp, "win32_goose_tex_", win32::GetCurrentProcessId(), ".png");
    format(bmp_path, tmp, "win32_goose_tex_", win32::GetCurrentProcessId(), ".bmp");
    defer win32::delete_file(png_path);
    defer win32::delete_file(bmp_path);

    // BGRA: A=0xFF, R=0xE0, G=0x50, B=0x20 -> 0xFFE05020.
    var src_bgra: u32[256] = [0xFFE05020; 256];
    assert(win32::image_save_png(png_path, src_bgra, 16, 16));
    assert(win32::image_save_bmp(bmp_path, src_bgra, 16, 16));

    var loaded_bgra: u32[>..] = [];
    var lw: i32 = 0;
    var lh: i32 = 0;
    assert(win32::image_load_bgra(png_path, loaded_bgra, lw, lh));
    assert(lw == 16 && lh == 16 && loaded_bgra.len == 256);
    assert(loaded_bgra[0] == 0xFFE05020 && loaded_bgra[255] == 0xFFE05020);

    var loaded_rgba: u32[>..] = [];
    lw = 0;
    lh = 0;
    assert(win32::image_load_rgba(bmp_path, loaded_rgba, lw, lh));
    assert(lw == 16 && lh == 16 && loaded_rgba.len == 256);
    // Little-endian RGBA8 bytes [0xE0, 0x50, 0x20, 0xFF] == 0xFF2050E0.
    assert(loaded_rgba[0] == 0xFF2050E0);

    let quad: win32::D3dTexVertex[4] = [
        win32::D3dTexVertex { x: -1.0, y:  1.0, z: 0.0, u: 0.0, v: 0.0 },
        win32::D3dTexVertex { x:  1.0, y:  1.0, z: 0.0, u: 1.0, v: 0.0 },
        win32::D3dTexVertex { x:  1.0, y: -1.0, z: 0.0, u: 1.0, v: 1.0 },
        win32::D3dTexVertex { x: -1.0, y: -1.0, z: 0.0, u: 0.0, v: 1.0 },
    ];
    let indices: u32[6] = [0, 1, 2, 0, 2, 3];
    // Updated texture: RGBA green (R=0x20, G=0xD0, B=0x30, A=0xFF) -> 0xFF30D020.
    var green_rgba: u32[256] = [0xFF30D020; 256];

    // 2. D3D11 Texture2D + IndexBuffer + DrawIndexed readback verification.
    {
        let win = win32::create_window("win32-goose d3d11 tex test", 128, 128);
        assert(win.is_valid());
        defer win.destroy();

        let dev = win32::d3d11_create(win, false);
        assert(dev.is_valid());
        defer dev.destroy();

        let hlsl: const u8[:] = "Texture2D tex0 : register(t0);\nSamplerState samp0 : register(s0);\nstruct VSIn { float3 pos : POSITION; float2 uv : TEXCOORD; float4 col : COLOR; };\nstruct PSIn { float4 pos : SV_POSITION; float2 uv : TEXCOORD; float4 col : COLOR; };\nPSIn vs_main(VSIn i) { PSIn o; o.pos = float4(i.pos, 1.0); o.uv = i.uv; o.col = i.col; return o; }\nfloat4 ps_main(PSIn i) : SV_TARGET { return tex0.Sample(samp0, i.uv) * i.col; }\n";

        var err: u8[>..] = [];
        let pipe = dev.create_pipeline_textured(hlsl, err);
        assert(pipe.is_valid());
        defer pipe.destroy();

        let vb = dev.create_tex_vertex_buffer(quad);
        let ib = dev.create_index_buffer(indices);
        let tex = dev.create_texture_rgba(16, 16, loaded_rgba);
        assert(vb.is_valid() && ib.is_valid() && tex.is_valid());
        defer vb.destroy();
        defer ib.destroy();
        defer tex.destroy();

        dev.begin_frame(0.0, 0.0, 0.0, 1.0);
        dev.bind_pipeline(pipe);
        dev.bind_tex_vertex_buffer(vb);
        dev.bind_index_buffer(ib);
        dev.bind_texture(0, tex);
        dev.draw_indexed(6);

        var px: u32 = 0;
        assert(dev.readback_pixel(64, 64, px));
        let r = px & 0xFF;
        let g = (px >> 8) & 0xFF;
        let b = (px >> 16) & 0xFF;
        assert(r >= 210 && g >= 65 && g <= 95 && b >= 20 && b <= 45);
        assert(dev.end_frame());

        // Dynamic texture update verification.
        assert(dev.update_texture_rgba(tex, green_rgba));
        dev.begin_frame(0.0, 0.0, 0.0, 1.0);
        dev.bind_pipeline(pipe);
        dev.bind_tex_vertex_buffer(vb);
        dev.bind_index_buffer(ib);
        dev.bind_texture(0, tex);
        dev.draw_indexed(6);
        assert(dev.readback_pixel(64, 64, px));
        let g2 = (px >> 8) & 0xFF;
        assert(g2 >= 195);
        assert(dev.end_frame());
    }

    // 3. OpenGL 3.3 Core Texture2D + Indexed Mesh (EBO) readback verification.
    {
        let win = win32::create_window("win32-goose gl tex test", 128, 128);
        assert(win.is_valid());
        defer win.destroy();

        let gl = win32::gl_create(win, false);
        assert(gl.is_valid());
        defer gl.destroy();

        let vs_src: const u8[:] = "#version 330 core\nlayout(location=0) in vec3 a_pos;\nlayout(location=1) in vec2 a_uv;\nlayout(location=2) in vec4 a_col;\nout vec2 v_uv;\nout vec4 v_col;\nvoid main() { gl_Position = vec4(a_pos, 1.0); v_uv = a_uv; v_col = a_col; }\n";
        let fs_src: const u8[:] = "#version 330 core\nuniform sampler2D u_tex;\nin vec2 v_uv;\nin vec4 v_col;\nout vec4 o_col;\nvoid main() { o_col = texture(u_tex, v_uv) * v_col; }\n";

        var err: u8[>..] = [];
        let prog = gl.create_program(vs_src, fs_src, err);
        assert(prog.is_valid());
        defer prog.destroy();

        let mesh = gl.create_indexed_mesh(quad, indices);
        let tex = gl.create_texture_rgba(16, 16, loaded_rgba);
        assert(mesh.is_valid() && tex.is_valid());
        defer mesh.destroy();
        defer tex.destroy();

        gl.begin_frame(0.0, 0.0, 0.0, 1.0);
        gl.use_program(prog);
        prog.set_uniform_i32("u_tex", 0);
        gl.bind_texture(0, tex);
        gl.bind_mesh(mesh);
        gl.draw_indexed(6);

        var px: u32 = 0;
        assert(gl.readback_pixel(64, 64, px));
        let r = px & 0xFF;
        let g = (px >> 8) & 0xFF;
        let b = (px >> 16) & 0xFF;
        assert(r >= 210 && g >= 65 && g <= 95 && b >= 20 && b <= 45);

        assert(gl.update_texture_rgba(tex, green_rgba));
        gl.begin_frame(0.0, 0.0, 0.0, 1.0);
        gl.use_program(prog);
        prog.set_uniform_i32("u_tex", 0);
        gl.bind_texture(0, tex);
        gl.bind_mesh(mesh);
        gl.draw_indexed(6);
        assert(gl.readback_pixel(64, 64, px));
        let g2 = (px >> 8) & 0xFF;
        assert(g2 >= 195);
        assert(gl.end_frame());
    }

    print("[OK] WIC (PNG/BMP save/load) + D3D11 & OpenGL 3.3 Texture2D + IndexBuffer");
}

fn test_cursor_fullscreen_raw_input() {
    let win = win32::create_window("win32-goose input test", 320, 240);
    assert(win.is_valid());
    defer win.destroy();

    assert(win32::set_system_cursor(win32::IDC_ARROW));
    assert(win.enable_raw_mouse(true));
    assert(win.enable_raw_mouse(false));

    assert(win.set_borderless_fullscreen(true));
    assert(win.set_borderless_fullscreen(false));

    assert(win32::unclip_cursor());
    let _ = win32::foreground_window();
    let _ = win.set_capture();
    let _ = win32::release_capture();

    print("[OK] cursor, borderless fullscreen, mouse capture & raw mouse input");
}

fn test_bugfix_regressions() {
    // 1. UTF-8 / UTF-16 invalid sequences: overlong 4-byte UTF-8 and lone surrogates -> U+FFFD.
    let overlong4: u8[4] = [0xF0, 0x80, 0x80, 0x80];
    let w_over = win32::utf8_to_utf16z(overlong4);
    assert(w_over.len == 5 && w_over[0] == 0xFFFD && w_over[4] == 0);

    let lone_hi: u16[1] = [0xD800];
    let u_hi = win32::utf16_to_utf8(lone_hi);
    assert(u_hi.len == 3 && u_hi[0] == 0xEF && u_hi[1] == 0xBF && u_hi[2] == 0xBD);

    let lone_lo: u16[1] = [0xDC00];
    let u_lo = win32::utf16_to_utf8(lone_lo);
    assert(u_lo.len == 3 && u_lo[0] == 0xEF && u_lo[1] == 0xBF && u_lo[2] == 0xBD);

    // 2. 64-bit HKEY_* constants are sign-extended per winreg.h.
    assert(win32::HKEY_CURRENT_USER == 0xFFFFFFFF80000001);

    // 3. run_process_capture must respect timeout_ms even when child keeps stdout open.
    {
        var out: u8[>..] = [];
        var code: u32 = 0;
        let t0 = win32::tick_count_ms();
        let ok = win32::run_process_capture("cmd.exe /c ping -n 6 127.0.0.1", out, code, 150);
        let elapsed = win32::tick_count_ms() - t0;
        assert(!ok);
        assert(elapsed < 2500);
    }

    // 4. file_map_open must reject INVALID_HANDLE_VALUE (must not create pagefile mapping).
    {
        let bad_f = win32::Hfile { raw: win32::INVALID_HANDLE_VALUE };
        let bad_mm = win32::file_map_open(bad_f, true, 4096);
        assert(!bad_mm.is_valid());
    }

    // 5. Empty REG_SZ registry string round-trip.
    {
        var subkey: u8[>..] = [];
        format(subkey, "Software\\Win32GooseRegEmpty_", win32::GetCurrentProcessId());
        defer win32::reg_delete_key(win32::HKEY_CURRENT_USER, subkey);
        let k = win32::reg_create_key(win32::HKEY_CURRENT_USER, subkey);
        assert(k.is_valid());
        defer k.close();
        assert(k.set_string("Empty", ""));
        var got: u8[>..] = [];
        assert(k.get_string("Empty", got) && got.len == 0);
    }

    // 6. WSAPoll on closed socket must report error (< 0), not ready (> 0).
    {
        let s = win32::udp_bind("127.0.0.1", 0);
        assert(s.is_valid());
        let raw_s = s;
        assert(s.close());
        assert(raw_s.poll_readable(0) < 0);
    }

    // 7. play_wav_bytes rejects truncated / non-RIFF buffers safely.
    {
        let tiny: u8[8] = [0x52, 0x49, 0x46, 0x46, 0xFF, 0xFF, 0x00, 0x00];
        assert(!win32::play_wav_bytes(tiny, false));
        assert(!win32::play_wav_bytes(tiny, true));
    }

    // 8. DIB helpers guard against out-of-bounds dimensions and large radius overflow.
    {
        var px: u32[16] = [0; 16];
        win32::fill_pixel_rect(px, 100, 100, -10, -10, 200, 200, 0xFFFFFFFF);
        win32::fill_pixel_circle(px, 4, 4, 2, 2, 100000, 0xFF112233);
        assert(px[0] == 0xFF112233);
    }

    // 9. D3D11 partial update of a non-dynamic (DEFAULT) vertex buffer + multiple fullscreen windows.
    {
        let w1 = win32::create_window("win32-goose reg1", 128, 128);
        let w2 = win32::create_window("win32-goose reg2", 128, 128);
        assert(w1.is_valid() && w2.is_valid());
        assert(w1.set_borderless_fullscreen(true));
        assert(w2.set_borderless_fullscreen(true));
        assert(w1.set_borderless_fullscreen(false));
        assert(w2.set_borderless_fullscreen(false));

        let dev = win32::d3d11_create(w1, false);
        assert(dev.is_valid());
        let v3: win32::D3dVertex[3] = [
            win32::D3dVertex { x: 0.0, y: 0.0, z: 0.0 },
            win32::D3dVertex { x: 1.0, y: 0.0, z: 0.0 },
            win32::D3dVertex { x: 0.0, y: 1.0, z: 0.0 },
        ];
        let vb = dev.create_vertex_buffer(v3, false);
        assert(vb.is_valid());
        let v1: win32::D3dVertex[1] = [win32::D3dVertex { x: 0.5, y: 0.5, z: 0.0 }];
        assert(dev.update_vertex_buffer(vb, v1));
        vb.destroy();
        dev.destroy();

        let sb = w1.create_statusbar(301, "ok");
        assert(sb.is_valid());
        assert(!sb.statusbar_set_text("bad", 256));

        w1.destroy();
        w2.destroy();
    }

    print("[OK] bugfix regressions (utf16, timeout, filemap, reg, poll, wav, dib, d3d11, fullscreen)");
}

fn test_gl2d_sdf_and_text_atlas() {
    let win = win32::create_window("win32-goose gl2d test", 240, 160);
    assert(win.is_valid());
    defer win.destroy();

    let g = win32::gl2d_create(win, false);
    assert(g.is_valid());
    defer g.destroy();

    let f = g.create_font("Segoe UI", 16.0, 600, false);
    assert(f.is_valid());
    defer f.destroy();

    g.begin_draw(win32::colorf_rgb(16, 18, 24, 1.0));
    g.fill_rounded_rect(12.0, 12.0, 100.0, 60.0, 8.0, win32::colorf_rgb(76, 194, 255, 1.0));
    g.fill_ellipse(170.0, 42.0, 24.0, 24.0, win32::colorf_rgb(34, 211, 238, 1.0));
    g.draw_line(12.0, 90.0, 220.0, 90.0, win32::colorf_rgb(139, 92, 246, 1.0), 2.0);
    g.push_clip(12.0, 98.0, 200.0, 40.0);
    g.draw_text("日本語テスト GL2D", f, 14.0, 100.0, 196.0, 32.0, win32::GL2D_ALIGN_START, win32::GL2D_ALIGN_CENTER, win32::colorf_rgb(245, 245, 245, 1.0));
    g.pop_clip();

    var px: u32 = 0;
    assert(g.readback_pixel(40, 40, px));
    let r = px & 0xFF;
    let gch = (px >> 8) & 0xFF;
    let b = (px >> 16) & 0xFF;
    assert(r <= 85 && gch >= 185 && b >= 245);
    assert(g.end_draw());

    print("[OK] OpenGL 3.3 Core 2D SDF + GPU VRAM text atlas (win32_gl2d)");
}

// Ensures every interactive/file-backed shim symbol resolves and links under both JIT and AOT.
fn verify_dialog_linkage(run_ui: bool) {
    guard run_ui;
    var buf: u8[>..] = [];
    var col: u32 = 0;
    win32::open_file_dialog(buf);
    win32::save_file_dialog(buf);
    win32::browse_folder_dialog(buf);
    win32::choose_color_dialog(col);
    win32::input_box(buf);
    win32::shell_open("https://example.com");
    win32::xinput_set_vibration(0, 0, 0);
    win32::play_sound_file("dummy.wav", false);
    win32::play_sound_stop();
    var px: u32[4] = [0; 4];
    var ts: i64 = 0;
    let v = win32::Hvideo {};
    v.read_frame_rgb32(px, ts);
    v.seek_100ns(0);
    v.close();
}

fn main() {
    test_utf16_roundtrip();
    test_pure_externs();
    test_window_lifecycle();
    test_clipboard();
    test_file_io();
    test_sync_objects();
    test_dib_and_d3d11();
    test_storage_extensions();
    test_system_services();
    test_networking();
    test_display_devices();
    test_desktop_and_controls();
    test_phase_c();
    test_wic_and_gpu_textures();
    test_cursor_fullscreen_raw_input();
    test_bugfix_regressions();
    test_gl2d_sdf_and_text_atlas();
    verify_dialog_linkage(false);
    print("ALL WIN32 TESTS PASSED");
}
