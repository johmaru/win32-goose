#ifndef WIN32_GOOSE_SHIM_H
#define WIN32_GOOSE_SHIM_H

#include <stdint.h>
#include "goose.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Goose structs are packed (spec C.2); mirror the exact field layouts here. */
#pragma pack(push, 1)

typedef struct {
    const uint8_t *data;
    int64_t len;
} w32_bytes;

typedef struct {
    int32_t left;
    int32_t top;
    int32_t right;
    int32_t bottom;
} w32_rect;

typedef struct {
    uint64_t hwnd;
    uint32_t msg;
    uint64_t wparam;
    int64_t lparam;
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
    int32_t wheel_delta;
    uint32_t key;
    uint32_t codepoint;
    uint32_t command_id;
    uint32_t notify_code;
} w32_event;

typedef struct {
    int64_t size;
    uint32_t attributes;
    uint64_t created_ft;
    uint64_t modified_ft;
} w32_file_info;

typedef struct {
    const uint32_t *data;
    int64_t len;
} w32_u32_slice;

typedef struct {
    const uint64_t *data;
    int64_t len;
} w32_u64_slice;

typedef struct {
    float x, y, z;
    float r, g, b, a;
} w32_d3d_vertex;

typedef struct {
    const w32_d3d_vertex *data;
    int64_t len;
} w32_d3d_vertex_slice;

typedef struct {
    float x, y, z;
    float u, v;
    float r, g, b, a;
} w32_tex_vertex;

typedef struct {
    const w32_tex_vertex *data;
    int64_t len;
} w32_tex_vertex_slice;

typedef struct {
    float r, g, b, a;
} w32_colorf;

typedef struct {
    uint8_t *data;
    int64_t len;
} w32_mut_bytes;

typedef struct {
    uint64_t avail_bytes;
    uint64_t total_bytes;
    uint64_t free_bytes;
} w32_disk_space;

typedef struct {
    uint32_t processor_count;
    uint32_t page_size;
    uint32_t allocation_granularity;
    uint16_t processor_arch;
} w32_system_info;

typedef struct {
    uint32_t memory_load;
    uint64_t total_phys;
    uint64_t avail_phys;
    uint64_t total_page_file;
    uint64_t avail_page_file;
    uint64_t total_virtual;
    uint64_t avail_virtual;
} w32_memory_status;

typedef struct {
    uint16_t year;
    uint16_t month;
    uint16_t day_of_week;
    uint16_t day;
    uint16_t hour;
    uint16_t minute;
    uint16_t second;
    uint16_t milliseconds;
} w32_system_time;

typedef struct {
    int32_t left;
    int32_t top;
    int32_t right;
    int32_t bottom;
    int32_t work_left;
    int32_t work_top;
    int32_t work_right;
    int32_t work_bottom;
    uint32_t dpi_x;
    uint32_t dpi_y;
    uint8_t is_primary;
} w32_monitor_info;

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t bits_per_pixel;
    uint32_t frequency;
} w32_display_mode;

typedef struct {
    const float *data;
    int64_t len;
} w32_f32_slice;

typedef struct {
    uint32_t *data;
    int64_t len;
} w32_mut_u32_slice;

typedef struct {
    uint8_t connected;
    uint16_t buttons;
    uint8_t left_trigger;
    uint8_t right_trigger;
    int16_t thumb_lx;
    int16_t thumb_ly;
    int16_t thumb_rx;
    int16_t thumb_ry;
    uint32_t packet_number;
} w32_gamepad_state;

typedef struct {
    uint32_t sample_rate;
    uint16_t channels;
    uint16_t bits_per_sample;
    uint8_t is_float;
    uint32_t buffer_frames;
} w32_audio_format;

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t fps_num;
    uint32_t fps_den;
    int64_t duration_100ns;
} w32_video_info;

typedef struct {
    uint32_t pid;
    uint32_t parent_pid;
    uint32_t thread_count;
    int32_t base_priority;
} w32_process_entry;

typedef struct {
    uint8_t ac_line_status;
    uint8_t battery_flag;
    uint8_t battery_life_percent;
    uint8_t power_saver;
    uint32_t battery_life_sec;
    uint32_t battery_full_life_sec;
} w32_power_status;

#pragma pack(pop)

/* DPI awareness (Per-Monitor V2 when available). */
uint8_t w32_enable_dpi_awareness(void);

/* Register a Win32 window class backed by the shim's WndProc. */
uint8_t w32_register_class(w32_bytes class_name, uint32_t style);

/* Create a Win32 window using UTF-8 class name and title. Returns HWND as u64 (0 on failure). */
uint64_t w32_create_window(
    w32_bytes class_name,
    w32_bytes title,
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height,
    uint32_t style,
    uint32_t ex_style,
    uint64_t parent
);

/* Non-blocking event pump: returns 1 if an event was written to out, 0 if queue is empty. */
uint8_t w32_poll_event(w32_event *out);

/* Blocking event pump: returns 1 for a normal event, 0 on WM_QUIT (out still carries WM_QUIT). */
uint8_t w32_wait_event(w32_event *out);

/* Flicker-free double-buffered paint session outside WndProc.
   w32_begin_paint returns a memory HDC (0 on failure) and writes client rect to out_rect. */
uint64_t w32_begin_paint(uint64_t hwnd, w32_rect *out_rect);
void w32_end_paint(uint64_t hwnd);

/* UTF-8 -> 2-byte-aligned UTF-16 wrappers for Win32 *W APIs (Goose data stacks
   are 1-byte packed, while Win32 kernel/user32 UNICODE_STRING APIs require
   2-byte aligned wchar_t buffers). */
uint8_t w32_set_window_text(uint64_t hwnd, w32_bytes text);
int32_t w32_message_box(uint64_t hwnd, w32_bytes text, w32_bytes caption, uint32_t flags);
int32_t w32_draw_text(uint64_t hdc, w32_bytes text, w32_rect *rect, uint32_t format);
uint8_t w32_text_out(uint64_t hdc, int32_t x, int32_t y, w32_bytes text);

/* UTF-8 builder helpers using Goose's u8[>..]& builder. */
uint8_t w32_get_window_text(uint64_t hwnd, gs_rref out);
uint8_t w32_format_error(uint32_t error_code, gs_rref out);

/* Menu helpers (aligned UTF-16 label & readback). */
uint8_t w32_append_menu(uint64_t hmenu, uint32_t flags, uint64_t id_or_submenu, w32_bytes text);
uint8_t w32_get_menu_string(uint64_t hmenu, uint32_t id, uint32_t flags, gs_rref out);

/* Common dialogs (src/win32_dialogs.c).
   `filter` uses pipe separators: "Text Files (*.txt)|*.txt|All Files (*.*)|*.*". */
uint8_t w32_open_file_dialog(
    uint64_t owner,
    w32_bytes title,
    w32_bytes filter,
    w32_bytes default_ext,
    gs_rref out_path
);
uint8_t w32_save_file_dialog(
    uint64_t owner,
    w32_bytes title,
    w32_bytes filter,
    w32_bytes default_ext,
    w32_bytes initial_name,
    gs_rref out_path
);
uint8_t w32_browse_folder_dialog(uint64_t owner, w32_bytes title, gs_rref out_path);
uint8_t w32_choose_color_dialog(uint64_t owner, uint32_t initial_color, uint32_t *out_color);
uint8_t w32_input_dialog(
    uint64_t owner,
    w32_bytes title,
    w32_bytes prompt,
    w32_bytes initial_text,
    gs_rref out_text
);

/* Clipboard helpers (src/win32_io.c). */
uint8_t w32_clipboard_set_text(uint64_t owner, w32_bytes text);
uint8_t w32_clipboard_get_text(uint64_t owner, gs_rref out);
uint8_t w32_clipboard_set_bytes(uint64_t owner, uint32_t format, w32_bytes data);
uint8_t w32_clipboard_get_bytes(uint64_t owner, uint32_t format, gs_rref out);
uint32_t w32_register_clipboard_format(w32_bytes name);

/* Win32 File I/O helpers (src/win32_io.c). */
uint64_t w32_create_file(
    w32_bytes path,
    uint32_t desired_access,
    uint32_t share_mode,
    uint32_t creation_disposition,
    uint32_t flags_and_attributes
);
uint8_t w32_get_file_info_by_handle(uint64_t hfile, w32_file_info *out);
uint32_t w32_get_file_attributes(w32_bytes path);
uint8_t w32_delete_file(w32_bytes path);
uint8_t w32_move_file(w32_bytes from, w32_bytes to, uint8_t replace_existing);
uint8_t w32_copy_file(w32_bytes from, w32_bytes to, uint8_t fail_if_exists);
uint8_t w32_create_directory(w32_bytes path);
uint8_t w32_remove_directory(w32_bytes path);
uint8_t w32_get_temp_path(gs_rref out);
uint8_t w32_get_current_directory(gs_rref out);
uint8_t w32_get_full_path_name(w32_bytes path, gs_rref out);
uint64_t w32_find_first_file(w32_bytes pattern, gs_rref out_name, w32_file_info *out_info);
uint8_t w32_find_next_file(uint64_t hfind, gs_rref out_name, w32_file_info *out_info);
uint8_t w32_read_file_all(w32_bytes path, gs_rref out);
uint8_t w32_write_file_all(w32_bytes path, w32_bytes data);

/* Win32 Synchronization objects (src/win32_sync.c). */
uint64_t w32_create_mutex(uint8_t initial_owner, w32_bytes name, uint8_t *out_already_exists);
uint64_t w32_open_mutex(uint32_t desired_access, uint8_t inherit_handle, w32_bytes name);
uint64_t w32_create_semaphore(int32_t initial_count, int32_t max_count, w32_bytes name, uint8_t *out_already_exists);
uint64_t w32_open_semaphore(uint32_t desired_access, uint8_t inherit_handle, w32_bytes name);
uint64_t w32_create_event(uint8_t manual_reset, uint8_t initial_state, w32_bytes name, uint8_t *out_already_exists);
uint64_t w32_open_event(uint32_t desired_access, uint8_t inherit_handle, w32_bytes name);
uint64_t w32_create_waitable_timer(uint8_t manual_reset, w32_bytes name, uint8_t *out_already_exists);
uint8_t w32_set_waitable_timer(uint64_t htimer, int64_t due_time_100ns, int32_t period_ms);
uint32_t w32_wait_multiple(w32_u64_slice handles, uint8_t wait_all, uint32_t timeout_ms);

/* DIB software pixel buffer blitting (src/win32_dib.c). */
uint8_t w32_blit_pixels(
    uint64_t hdc,
    w32_u32_slice pixels,
    int32_t src_w,
    int32_t src_h,
    int32_t dst_x,
    int32_t dst_y,
    int32_t dst_w,
    int32_t dst_h
);
uint8_t w32_present_pixels(uint64_t hwnd, w32_u32_slice pixels, int32_t src_w, int32_t src_h);

/* Direct3D 11 graphics shim (src/win32_d3d11.c). */
uint32_t w32_d3d11_create(uint64_t hwnd, uint8_t vsync);
void w32_d3d11_destroy(uint32_t dev_id);
uint8_t w32_d3d11_resize(uint32_t dev_id, int32_t width, int32_t height);
uint8_t w32_d3d11_adapter_name(uint32_t dev_id, gs_rref out);
void w32_d3d11_begin_frame(uint32_t dev_id, float r, float g, float b, float a);
uint8_t w32_d3d11_end_frame(uint32_t dev_id);
uint32_t w32_d3d11_create_pipeline(
    uint32_t dev_id,
    w32_bytes hlsl,
    w32_bytes vs_entry,
    w32_bytes ps_entry,
    uint8_t depth_enable,
    gs_rref out_error
);
void w32_d3d11_destroy_pipeline(uint32_t pipe_id);
uint32_t w32_d3d11_create_vertex_buffer(uint32_t dev_id, w32_d3d_vertex_slice verts, uint8_t dynamic);
uint32_t w32_d3d11_create_constant_buffer(uint32_t dev_id, uint32_t byte_size);
uint8_t w32_d3d11_update_buffer(uint32_t dev_id, uint32_t buf_id, w32_bytes data);
uint8_t w32_d3d11_update_vertex_buffer(uint32_t dev_id, uint32_t buf_id, w32_d3d_vertex_slice verts);
void w32_d3d11_destroy_buffer(uint32_t buf_id);
void w32_d3d11_bind_pipeline(uint32_t dev_id, uint32_t pipe_id);
void w32_d3d11_bind_vertex_buffer(uint32_t dev_id, uint32_t buf_id, uint32_t stride);
void w32_d3d11_bind_constant_buffer(uint32_t dev_id, uint32_t slot, uint32_t buf_id);
void w32_d3d11_draw(uint32_t dev_id, uint32_t vertex_count, uint32_t start_vertex);
uint8_t w32_d3d11_readback_pixel(uint32_t dev_id, int32_t x, int32_t y, uint32_t *out_rgba);

/* Direct2D + DirectWrite GPU 2D graphics shim (src/win32_d2d.c). */
uint32_t w32_d2d_create(uint64_t hwnd, uint8_t vsync);
void w32_d2d_destroy(uint32_t target_id);
uint8_t w32_d2d_resize(uint32_t target_id, int32_t width, int32_t height);
void w32_d2d_begin_draw(uint32_t target_id);
uint8_t w32_d2d_end_draw(uint32_t target_id);
void w32_d2d_clear(uint32_t target_id, w32_colorf color);
void w32_d2d_set_transform(uint32_t target_id, float m11, float m12, float m21, float m22, float dx, float dy);
void w32_d2d_push_clip(uint32_t target_id, float x, float y, float w, float h);
void w32_d2d_pop_clip(uint32_t target_id);
void w32_d2d_draw_line(uint32_t target_id, float x0, float y0, float x1, float y1, w32_colorf color, float stroke_width);
void w32_d2d_draw_rect(uint32_t target_id, float x, float y, float w, float h, w32_colorf color, float stroke_width);
void w32_d2d_fill_rect(uint32_t target_id, float x, float y, float w, float h, w32_colorf color);
void w32_d2d_draw_rounded_rect(uint32_t target_id, float x, float y, float w, float h, float rx, float ry, w32_colorf color, float stroke_width);
void w32_d2d_fill_rounded_rect(uint32_t target_id, float x, float y, float w, float h, float rx, float ry, w32_colorf color);
void w32_d2d_draw_ellipse(uint32_t target_id, float cx, float cy, float rx, float ry, w32_colorf color, float stroke_width);
void w32_d2d_fill_ellipse(uint32_t target_id, float cx, float cy, float rx, float ry, w32_colorf color);
uint32_t w32_d2d_create_font(uint32_t target_id, w32_bytes family, float size, int32_t weight, uint8_t italic);
void w32_d2d_set_font_align(uint32_t font_id, int32_t h_align, int32_t v_align);
void w32_d2d_set_font_word_wrap(uint32_t font_id, uint8_t wrap);
void w32_d2d_destroy_font(uint32_t font_id);
void w32_d2d_draw_text(uint32_t target_id, w32_bytes text, uint32_t font_id, float x, float y, float w, float h, w32_colorf color);
uint8_t w32_d2d_measure_text(uint32_t target_id, w32_bytes text, uint32_t font_id, float *out_w, float *out_h);

/* OpenGL 3.3 Core (WGL) graphics shim (src/win32_gl.c). */
uint32_t w32_gl_create(uint64_t hwnd, uint8_t vsync);
void w32_gl_destroy(uint32_t ctx_id);
uint8_t w32_gl_make_current(uint32_t ctx_id);
uint8_t w32_gl_swap_buffers(uint32_t ctx_id);
void w32_gl_set_vsync(uint32_t ctx_id, uint8_t vsync);
uint8_t w32_gl_renderer_name(uint32_t ctx_id, gs_rref out);
uint8_t w32_gl_version_string(uint32_t ctx_id, gs_rref out);
void w32_gl_begin_frame(uint32_t ctx_id, float r, float g, float b, float a);
uint32_t w32_gl_create_program(w32_bytes vs_src, w32_bytes fs_src, gs_rref out_error);
void w32_gl_destroy_program(uint32_t prog);
void w32_gl_use_program(uint32_t prog);
void w32_gl_uniform_1f(uint32_t prog, w32_bytes name, float v0);
void w32_gl_uniform_2f(uint32_t prog, w32_bytes name, float v0, float v1);
void w32_gl_uniform_4f(uint32_t prog, w32_bytes name, float v0, float v1, float v2, float v3);
uint32_t w32_gl_create_vertex_buffer(w32_d3d_vertex_slice verts, uint8_t dynamic);
void w32_gl_update_vertex_buffer(uint32_t mesh_id, w32_d3d_vertex_slice verts);
void w32_gl_destroy_vertex_buffer(uint32_t mesh_id);
void w32_gl_bind_vertex_buffer(uint32_t mesh_id);
void w32_gl_draw_arrays(uint32_t mode, int32_t first, int32_t count);
uint8_t w32_gl_readback_pixel(uint32_t ctx_id, int32_t x, int32_t y, uint32_t *out_rgba);

/* OpenGL 3.3 Core 2D SDF + GPU VRAM atlas renderer (src/win32_gl2d.c). */
uint32_t w32_gl2d_create(uint64_t hwnd, uint8_t vsync);
void w32_gl2d_destroy(uint32_t target_id);
uint8_t w32_gl2d_resize(uint32_t target_id, int32_t width, int32_t height);
void w32_gl2d_begin_draw(uint32_t target_id, w32_colorf clear_color);
uint8_t w32_gl2d_end_draw(uint32_t target_id);
void w32_gl2d_push_clip(uint32_t target_id, float x, float y, float w, float h);
void w32_gl2d_pop_clip(uint32_t target_id);
void w32_gl2d_clear_text_cache(uint32_t target_id);
void w32_gl2d_draw_line(uint32_t target_id, float x0, float y0, float x1, float y1, w32_colorf color, float stroke_width);
void w32_gl2d_draw_rect(uint32_t target_id, float x, float y, float w, float h, w32_colorf color, float stroke_width);
void w32_gl2d_fill_rect(uint32_t target_id, float x, float y, float w, float h, w32_colorf color);
void w32_gl2d_draw_rounded_rect(uint32_t target_id, float x, float y, float w, float h, float r, w32_colorf color, float stroke_width);
void w32_gl2d_fill_rounded_rect(uint32_t target_id, float x, float y, float w, float h, float r, w32_colorf color);
void w32_gl2d_draw_ellipse(uint32_t target_id, float cx, float cy, float rx, float ry, w32_colorf color, float stroke_width);
void w32_gl2d_fill_ellipse(uint32_t target_id, float cx, float cy, float rx, float ry, w32_colorf color);
uint32_t w32_gl2d_create_font(uint32_t target_id, w32_bytes family, float size, int32_t weight, uint8_t italic);
void w32_gl2d_destroy_font(uint32_t font_id);
void w32_gl2d_draw_text(
    uint32_t target_id,
    w32_bytes text,
    uint32_t font_id,
    float x, float y, float w, float h,
    int32_t h_align,
    int32_t v_align,
    w32_colorf color
);
uint8_t w32_gl2d_readback_pixel(uint32_t target_id, int32_t x, int32_t y, uint32_t *out_rgba);

/* Storage extensions: memory-mapped files, disk/volume info, dir watch (src/win32_io.c). */
uint32_t w32_file_map_open(uint64_t hfile, uint8_t writable, uint64_t max_size);
int64_t w32_file_map_size(uint32_t map_id);
int64_t w32_file_map_read(uint32_t map_id, int64_t offset, w32_mut_bytes out);
int64_t w32_file_map_write(uint32_t map_id, int64_t offset, w32_bytes data);
uint8_t w32_file_map_flush(uint32_t map_id, int64_t offset, int64_t len);
void w32_file_map_close(uint32_t map_id);
uint8_t w32_get_disk_free_space(w32_bytes path, w32_disk_space *out);
uint8_t w32_get_volume_info(w32_bytes root_path, gs_rref out_label, gs_rref out_fs, uint32_t *out_serial);
uint64_t w32_watch_dir_open(w32_bytes path, uint8_t watch_subtree, uint32_t filter);

/* System Services: system info, memory, time, env, process/pipes, registry, console (src/win32_system.c). */
void w32_get_system_info(w32_system_info *out);
uint8_t w32_get_memory_status(w32_memory_status *out);
void w32_get_local_time(w32_system_time *out);
void w32_get_system_time(w32_system_time *out);
uint8_t w32_get_computer_name(gs_rref out);
uint8_t w32_get_user_name(gs_rref out);
uint8_t w32_get_module_file_name(gs_rref out);
uint8_t w32_env_get(w32_bytes name, gs_rref out);
uint8_t w32_env_set(w32_bytes name, w32_bytes value);
uint8_t w32_env_delete(w32_bytes name);
uint8_t w32_run_process_capture(w32_bytes cmdline, gs_rref out_stdout, uint32_t *out_exit_code, uint32_t timeout_ms);
uint64_t w32_spawn_process(w32_bytes cmdline, w32_bytes working_dir, uint8_t hide_window);
uint8_t w32_get_exit_code_process(uint64_t hproc, uint32_t *out_code);
uint64_t w32_reg_create_key(uint64_t root, w32_bytes subkey, uint32_t access);
uint64_t w32_reg_open_key(uint64_t root, w32_bytes subkey, uint32_t access);
uint8_t w32_reg_delete_key(uint64_t root, w32_bytes subkey);
uint8_t w32_reg_close_key(uint64_t hkey);
uint8_t w32_reg_set_string(uint64_t hkey, w32_bytes name, w32_bytes value);
uint8_t w32_reg_get_string(uint64_t hkey, w32_bytes name, gs_rref out);
uint8_t w32_reg_set_u32(uint64_t hkey, w32_bytes name, uint32_t value);
uint8_t w32_reg_get_u32(uint64_t hkey, w32_bytes name, uint32_t *out);
uint8_t w32_reg_set_u64(uint64_t hkey, w32_bytes name, uint64_t value);
uint8_t w32_reg_get_u64(uint64_t hkey, w32_bytes name, uint64_t *out);
uint8_t w32_reg_set_bytes(uint64_t hkey, w32_bytes name, w32_bytes data);
uint8_t w32_reg_get_bytes(uint64_t hkey, w32_bytes name, gs_rref out);
uint8_t w32_reg_delete_value(uint64_t hkey, w32_bytes name);
uint8_t w32_enable_console_vt(void);
uint8_t w32_set_console_title(w32_bytes title);
uint8_t w32_get_console_size(int32_t *out_cols, int32_t *out_rows);

/* Networking & Internet: Winsock2 TCP/UDP/DNS + WinHTTP (src/win32_net.c). */
uint8_t w32_net_init(void);
void w32_net_cleanup(void);
uint64_t w32_tcp_listen(w32_bytes host, uint16_t port, int32_t backlog);
uint16_t w32_socket_local_port(uint64_t sock);
uint64_t w32_tcp_accept(uint64_t listen_sock);
uint64_t w32_tcp_connect(w32_bytes host, uint16_t port, uint32_t timeout_ms);
int64_t w32_socket_send(uint64_t sock, w32_bytes data);
int64_t w32_socket_recv(uint64_t sock, w32_mut_bytes buf);
uint8_t w32_socket_set_timeout(uint64_t sock, uint32_t recv_ms, uint32_t send_ms);
uint8_t w32_socket_set_nonblocking(uint64_t sock, uint8_t nb);
uint8_t w32_socket_shutdown(uint64_t sock);
uint8_t w32_socket_close(uint64_t sock);
uint64_t w32_udp_bind(w32_bytes host, uint16_t port);
int64_t w32_udp_send_to(uint64_t sock, w32_bytes host, uint16_t port, w32_bytes data);
int64_t w32_udp_recv_from(uint64_t sock, w32_mut_bytes buf, gs_rref out_host, uint16_t *out_port);
uint8_t w32_dns_resolve_ipv4(w32_bytes host, gs_rref out_ip);
uint8_t w32_http_request(
    w32_bytes method,
    w32_bytes url,
    w32_bytes body,
    w32_bytes content_type,
    gs_rref out_body,
    uint32_t *out_status,
    uint32_t timeout_ms
);

/* Display Devices (src/win32_display.c). */
int32_t w32_monitor_count(void);
uint8_t w32_get_monitor_info(int32_t index, w32_monitor_info *out, gs_rref out_device_name);
uint8_t w32_get_display_device(int32_t index, gs_rref out_name, gs_rref out_desc, uint8_t *out_active);
uint8_t w32_get_current_display_mode(w32_bytes device_name, w32_display_mode *out);
uint8_t w32_get_display_mode_at(w32_bytes device_name, uint32_t mode_index, w32_display_mode *out);

/* Desktop / Shell / DWM / System Tray (src/win32_desktop.c). */
uint8_t w32_dwm_set_dark_titlebar(uint64_t hwnd, uint8_t enabled);
uint8_t w32_dwm_set_corner_preference(uint64_t hwnd, uint32_t pref);
uint8_t w32_dwm_set_backdrop(uint64_t hwnd, uint32_t backdrop);
uint8_t w32_dwm_flush(void);
uint8_t w32_known_folder_path(int32_t kind, gs_rref out);
uint8_t w32_shell_open(w32_bytes target, w32_bytes params, w32_bytes verb, int32_t show_cmd);
uint8_t w32_tray_add(uint64_t hwnd, uint32_t id, w32_bytes tooltip);
uint8_t w32_tray_set_tooltip(uint64_t hwnd, uint32_t id, w32_bytes tooltip);
uint8_t w32_tray_balloon(uint64_t hwnd, uint32_t id, w32_bytes title, w32_bytes message, uint32_t info_flags);
uint8_t w32_tray_remove(uint64_t hwnd, uint32_t id);

/* Win32 Controls & Drag-and-Drop (src/win32_controls.c). */
uint8_t w32_init_common_controls(void);
uint64_t w32_create_control(
    uint64_t parent,
    uint32_t id,
    w32_bytes class_name,
    w32_bytes text,
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t height,
    uint32_t style,
    uint32_t ex_style
);
int32_t w32_listbox_add(uint64_t hwnd, w32_bytes text);
uint8_t w32_listbox_item_text(uint64_t hwnd, int32_t index, gs_rref out);
int32_t w32_combobox_add(uint64_t hwnd, w32_bytes text);
uint8_t w32_combobox_item_text(uint64_t hwnd, int32_t index, gs_rref out);
void w32_accept_drop_files(uint64_t hwnd, uint8_t enable);
int32_t w32_dropped_file_count(void);
uint8_t w32_dropped_file_path(int32_t index, gs_rref out);

/* Gaming: XInput (src/win32_game.c). */
uint8_t w32_xinput_get_state(uint32_t user_index, w32_gamepad_state *out);
uint8_t w32_xinput_set_vibration(uint32_t user_index, uint16_t left_motor, uint16_t right_motor);

/* Audio: WASAPI + winmm PlaySound (src/win32_audio.c). */
int32_t w32_audio_output_device_count(void);
uint8_t w32_audio_output_device_name(int32_t index, gs_rref out);
uint8_t w32_audio_default_output_name(gs_rref out);
uint32_t w32_wasapi_open_default(w32_audio_format *out_fmt);
uint32_t w32_wasapi_available_frames(uint32_t audio_id);
uint32_t w32_wasapi_write_f32(uint32_t audio_id, w32_f32_slice samples);
void w32_wasapi_start(uint32_t audio_id);
void w32_wasapi_stop(uint32_t audio_id);
void w32_wasapi_close(uint32_t audio_id);
uint8_t w32_play_sound_file(w32_bytes path, uint8_t async_play);
void w32_play_sound_stop(void);
uint8_t w32_play_wav_bytes(w32_bytes wav_bytes, uint8_t async_play);

/* Video: Media Foundation IMFSourceReader RGB32 decoder (src/win32_video.c). */
uint8_t w32_mf_startup(void);
void w32_mf_shutdown(void);
uint32_t w32_video_open(w32_bytes path, w32_video_info *out_info);
uint8_t w32_video_read_frame_rgb32(uint32_t vid_id, w32_mut_u32_slice out_pixels, int64_t *out_ts_100ns);
uint8_t w32_video_seek_100ns(uint32_t vid_id, int64_t pos_100ns);
void w32_video_close(uint32_t vid_id);

/* WIC Image decode/encode (src/win32_wic.c). */
uint8_t w32_image_load_bgra(w32_bytes path, gs_rref out_pixels_u32, int32_t *out_w, int32_t *out_h);
uint8_t w32_image_load_rgba(w32_bytes path, gs_rref out_pixels_u32, int32_t *out_w, int32_t *out_h);
uint8_t w32_image_save_png(w32_bytes path, w32_u32_slice pixels_bgra, int32_t w, int32_t h);
uint8_t w32_image_save_bmp(w32_bytes path, w32_u32_slice pixels_bgra, int32_t w, int32_t h);

/* Cursor, Capture, Borderless Fullscreen, Raw Input (src/win32_shim.c). */
uint8_t w32_set_system_cursor(uint32_t idc);
uint8_t w32_clip_cursor_window(uint64_t hwnd);
uint8_t w32_unclip_cursor(void);
uint8_t w32_set_borderless_fullscreen(uint64_t hwnd, uint8_t enable);
uint8_t w32_enable_raw_mouse(uint64_t hwnd, uint8_t enable);

/* D3D11 Textures & Index Buffers (src/win32_d3d11.c). */
uint32_t w32_d3d11_create_pipeline_textured(
    uint32_t dev_id, w32_bytes hlsl, w32_bytes vs_entry, w32_bytes ps_entry, uint8_t depth_enable, gs_rref out_error
);
uint32_t w32_d3d11_create_tex_vertex_buffer(uint32_t dev_id, w32_tex_vertex_slice verts, uint8_t dynamic);
uint32_t w32_d3d11_create_index_buffer(uint32_t dev_id, w32_u32_slice indices);
void w32_d3d11_bind_index_buffer(uint32_t dev_id, uint32_t buf_id);
void w32_d3d11_draw_indexed(uint32_t dev_id, uint32_t index_count, uint32_t start_index, int32_t base_vertex);
uint32_t w32_d3d11_create_texture_rgba(uint32_t dev_id, int32_t w, int32_t h, w32_u32_slice pixels_rgba);
uint8_t w32_d3d11_update_texture_rgba(uint32_t dev_id, uint32_t tex_id, w32_u32_slice pixels_rgba);
void w32_d3d11_bind_texture(uint32_t dev_id, uint32_t slot, uint32_t tex_id);
void w32_d3d11_destroy_texture(uint32_t tex_id);

/* OpenGL 3.3 Textures & Indexed Meshes (src/win32_gl.c). */
void w32_gl_uniform_1i(uint32_t prog, w32_bytes name, int32_t v0);
uint32_t w32_gl_create_indexed_mesh(w32_tex_vertex_slice verts, w32_u32_slice indices, uint8_t dynamic);
void w32_gl_draw_indexed(uint32_t mode, int32_t index_count);
uint32_t w32_gl_create_texture_rgba(int32_t w, int32_t h, w32_u32_slice pixels_rgba);
uint8_t w32_gl_update_texture_rgba(uint32_t tex_id, w32_u32_slice pixels_rgba);
void w32_gl_bind_texture(uint32_t slot, uint32_t tex_id);
void w32_gl_destroy_texture(uint32_t tex_id);

/* Networking extensions: WSAPoll + WinHTTP custom/response headers (src/win32_net.c). */
int32_t w32_socket_poll(uint64_t sock, uint8_t write_check, int32_t timeout_ms);
uint8_t w32_http_request_ex(
    w32_bytes method,
    w32_bytes url,
    w32_bytes headers_crlf,
    w32_bytes body,
    gs_rref out_body,
    uint32_t *out_status,
    gs_rref out_resp_headers,
    uint32_t timeout_ms
);

/* WASAPI Audio Capture / Loopback (src/win32_audio.c). */
int32_t w32_audio_input_device_count(void);
uint8_t w32_audio_input_device_name(int32_t index, gs_rref out);
uint8_t w32_audio_default_input_name(gs_rref out);
uint32_t w32_wasapi_capture_open(uint8_t loopback, w32_audio_format *out_fmt);
uint32_t w32_wasapi_capture_read_f32(uint32_t cap_id, gs_rref out_f32);
void w32_wasapi_capture_start(uint32_t cap_id);
void w32_wasapi_capture_stop(uint32_t cap_id);
void w32_wasapi_capture_close(uint32_t cap_id);

/* System Services extensions: processes, DLLs, virtual memory, power, named pipes (src/win32_system.c). */
uint64_t w32_process_snapshot_open(void);
uint8_t w32_process_snapshot_next(uint64_t hsnap, uint8_t first, w32_process_entry *out, gs_rref out_exe_name);
uint64_t w32_load_library(w32_bytes path);
uint64_t w32_get_proc_address(uint64_t hmod, w32_bytes proc_name);
uint64_t w32_virtual_alloc(uint64_t size, uint32_t alloc_type, uint32_t protect);
uint8_t w32_virtual_protect(uint64_t addr, uint64_t size, uint32_t new_protect, uint32_t *out_old_protect);
uint8_t w32_virtual_free(uint64_t addr);
uint8_t w32_get_power_status(w32_power_status *out);
uint32_t w32_set_thread_execution_state(uint32_t flags);
uint64_t w32_named_pipe_create(
    w32_bytes name,
    uint32_t max_instances,
    uint32_t out_buf_size,
    uint32_t in_buf_size,
    uint8_t nonblocking
);
uint64_t w32_named_pipe_open(w32_bytes name, uint32_t timeout_ms);

/* Additional UI Controls: Tab Control, Status Bar (src/win32_controls.c). */
int32_t w32_tab_add(uint64_t hwnd, int32_t index, w32_bytes text);
uint8_t w32_statusbar_set_text(uint64_t hwnd, int32_t part, w32_bytes text);

#ifdef __cplusplus
}
#endif

#endif /* WIN32_GOOSE_SHIM_H */
