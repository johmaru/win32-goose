// Direct3D 11 GPU graphics API (device, swapchain, HLSL compilation, buffers, draw).
namespace win32;

import types;

// Matches w32_d3d_vertex in include/win32_shim.h (28 bytes, naturally 4-byte aligned).
struct D3dVertex {
    x: f32 = 0.0,
    y: f32 = 0.0,
    z: f32 = 0.0,
    r: f32 = 1.0,
    g: f32 = 1.0,
    b: f32 = 1.0,
    a: f32 = 1.0,
}

// Matches w32_tex_vertex in include/win32_shim.h (36 bytes: pos + uv + color).
struct D3dTexVertex {
    x: f32 = 0.0,
    y: f32 = 0.0,
    z: f32 = 0.0,
    u: f32 = 0.0,
    v: f32 = 0.0,
    r: f32 = 1.0,
    g: f32 = 1.0,
    b: f32 = 1.0,
    a: f32 = 1.0,
}

struct D3dDevice { id: u32 = 0 }
struct D3dPipeline { id: u32 = 0 }
struct D3dBuffer { id: u32 = 0 }
struct D3dTexture { id: u32 = 0 }

fn is_valid(d: D3dDevice) -> bool { d.id != 0 }
fn is_valid(p: D3dPipeline) -> bool { p.id != 0 }
fn is_valid(b: D3dBuffer) -> bool { b.id != 0 }
fn is_valid(t: D3dTexture) -> bool { t.id != 0 }

extern fn shim_d3d11_create(hwnd: u64, vsync: bool) -> u32 = "w32_d3d11_create";
extern fn shim_d3d11_destroy(dev_id: u32) = "w32_d3d11_destroy";
extern fn shim_d3d11_resize(dev_id: u32, width: i32, height: i32) -> bool = "w32_d3d11_resize";
extern fn shim_d3d11_adapter_name(dev_id: u32, out: u8[>..]&) -> bool = "w32_d3d11_adapter_name";
extern fn shim_d3d11_begin_frame(dev_id: u32, r: f32, g: f32, b: f32, a: f32) = "w32_d3d11_begin_frame";
extern fn shim_d3d11_end_frame(dev_id: u32) -> bool = "w32_d3d11_end_frame";
extern fn shim_d3d11_create_pipeline(
    dev_id: u32,
    hlsl: const u8[:],
    vs_entry: const u8[:],
    ps_entry: const u8[:],
    depth_enable: bool,
    out_error: u8[>..]&
) -> u32 = "w32_d3d11_create_pipeline";
extern fn shim_d3d11_destroy_pipeline(pipe_id: u32) = "w32_d3d11_destroy_pipeline";
extern fn shim_d3d11_create_vertex_buffer(dev_id: u32, verts: const D3dVertex[:], dynamic: bool) -> u32 = "w32_d3d11_create_vertex_buffer";
extern fn shim_d3d11_create_constant_buffer(dev_id: u32, byte_size: u32) -> u32 = "w32_d3d11_create_constant_buffer";
extern fn shim_d3d11_update_buffer(dev_id: u32, buf_id: u32, data: const u8[:]) -> bool = "w32_d3d11_update_buffer";
extern fn shim_d3d11_update_vertex_buffer(dev_id: u32, buf_id: u32, verts: const D3dVertex[:]) -> bool = "w32_d3d11_update_vertex_buffer";
extern fn shim_d3d11_destroy_buffer(buf_id: u32) = "w32_d3d11_destroy_buffer";
extern fn shim_d3d11_bind_pipeline(dev_id: u32, pipe_id: u32) = "w32_d3d11_bind_pipeline";
extern fn shim_d3d11_bind_vertex_buffer(dev_id: u32, buf_id: u32, stride: u32) = "w32_d3d11_bind_vertex_buffer";
extern fn shim_d3d11_bind_constant_buffer(dev_id: u32, slot: u32, buf_id: u32) = "w32_d3d11_bind_constant_buffer";
extern fn shim_d3d11_draw(dev_id: u32, vertex_count: u32, start_vertex: u32) = "w32_d3d11_draw";
extern fn shim_d3d11_readback_pixel(dev_id: u32, x: i32, y: i32, out_rgba: u32&) -> bool = "w32_d3d11_readback_pixel";
extern fn shim_d3d11_create_pipeline_textured(
    dev_id: u32,
    hlsl: const u8[:],
    vs_entry: const u8[:],
    ps_entry: const u8[:],
    depth_enable: bool,
    out_error: u8[>..]&
) -> u32 = "w32_d3d11_create_pipeline_textured";
extern fn shim_d3d11_create_tex_vertex_buffer(dev_id: u32, verts: const D3dTexVertex[:], dynamic: bool) -> u32 = "w32_d3d11_create_tex_vertex_buffer";
extern fn shim_d3d11_create_index_buffer(dev_id: u32, indices: const u32[:]) -> u32 = "w32_d3d11_create_index_buffer";
extern fn shim_d3d11_bind_index_buffer(dev_id: u32, buf_id: u32) = "w32_d3d11_bind_index_buffer";
extern fn shim_d3d11_draw_indexed(dev_id: u32, index_count: u32, start_index: u32, base_vertex: i32) = "w32_d3d11_draw_indexed";
extern fn shim_d3d11_create_texture_rgba(dev_id: u32, w: i32, h: i32, pixels_rgba: const u32[:]) -> u32 = "w32_d3d11_create_texture_rgba";
extern fn shim_d3d11_update_texture_rgba(dev_id: u32, tex_id: u32, pixels_rgba: const u32[:]) -> bool = "w32_d3d11_update_texture_rgba";
extern fn shim_d3d11_bind_texture(dev_id: u32, slot: u32, tex_id: u32) = "w32_d3d11_bind_texture";
extern fn shim_d3d11_destroy_texture(tex_id: u32) = "w32_d3d11_destroy_texture";

fn d3d11_create(win: Hwnd, vsync: bool = true) -> D3dDevice {
    return D3dDevice { id: shim_d3d11_create(win.raw, vsync) };
}

fn destroy(d: D3dDevice) {
    shim_d3d11_destroy(d.id);
}

fn resize(d: D3dDevice, width: i32, height: i32) -> bool {
    return shim_d3d11_resize(d.id, width, height);
}

fn adapter_name(d: D3dDevice) -> u8[>..] {
    var out: u8[>..] = [];
    shim_d3d11_adapter_name(d.id, out);
    return out;
}

fn begin_frame(d: D3dDevice, r: f32, g: f32, b: f32, a: f32 = 1.0) {
    shim_d3d11_begin_frame(d.id, r, g, b, a);
}

fn end_frame(d: D3dDevice) -> bool {
    return shim_d3d11_end_frame(d.id);
}

fn create_pipeline(
    d: D3dDevice,
    hlsl: const u8[:],
    out_error: u8[>..]&,
    vs_entry: const u8[:] = "vs_main",
    ps_entry: const u8[:] = "ps_main",
    depth_enable: bool = false
) -> D3dPipeline {
    return D3dPipeline {
        id: shim_d3d11_create_pipeline(d.id, hlsl, vs_entry, ps_entry, depth_enable, out_error)
    };
}

fn destroy(p: D3dPipeline) {
    shim_d3d11_destroy_pipeline(p.id);
}

fn create_vertex_buffer(d: D3dDevice, verts: const D3dVertex[:], dynamic: bool = false) -> D3dBuffer {
    return D3dBuffer { id: shim_d3d11_create_vertex_buffer(d.id, verts, dynamic) };
}

fn create_constant_buffer(d: D3dDevice, byte_size: u32) -> D3dBuffer {
    return D3dBuffer { id: shim_d3d11_create_constant_buffer(d.id, byte_size) };
}

fn update_buffer(d: D3dDevice, buf: D3dBuffer, data: const u8[:]) -> bool {
    return shim_d3d11_update_buffer(d.id, buf.id, data);
}

fn update_vertex_buffer(d: D3dDevice, buf: D3dBuffer, verts: const D3dVertex[:]) -> bool {
    return shim_d3d11_update_vertex_buffer(d.id, buf.id, verts);
}

fn destroy(b: D3dBuffer) {
    shim_d3d11_destroy_buffer(b.id);
}

fn bind_pipeline(d: D3dDevice, pipe: D3dPipeline) {
    shim_d3d11_bind_pipeline(d.id, pipe.id);
}

fn bind_vertex_buffer(d: D3dDevice, buf: D3dBuffer, stride: u32 = 28) {
    shim_d3d11_bind_vertex_buffer(d.id, buf.id, stride);
}

fn bind_constant_buffer(d: D3dDevice, slot: u32, buf: D3dBuffer) {
    shim_d3d11_bind_constant_buffer(d.id, slot, buf.id);
}

fn draw(d: D3dDevice, vertex_count: u32, start_vertex: u32 = 0) {
    shim_d3d11_draw(d.id, vertex_count, start_vertex);
}

// Reads back one RGBA8 pixel (packed 0xAABBGGRR: low byte = R, then G, B, A).
fn readback_pixel(d: D3dDevice, x: i32, y: i32, out_rgba: u32&) -> bool {
    return shim_d3d11_readback_pixel(d.id, x, y, out_rgba);
}

// --- Textured pipelines, index buffers, and Texture2D -----------------------

fn create_pipeline_textured(
    d: D3dDevice,
    hlsl: const u8[:],
    out_error: u8[>..]&,
    vs_entry: const u8[:] = "vs_main",
    ps_entry: const u8[:] = "ps_main",
    depth_enable: bool = false
) -> D3dPipeline {
    return D3dPipeline {
        id: shim_d3d11_create_pipeline_textured(d.id, hlsl, vs_entry, ps_entry, depth_enable, out_error)
    };
}

fn create_tex_vertex_buffer(d: D3dDevice, verts: const D3dTexVertex[:], dynamic: bool = false) -> D3dBuffer {
    return D3dBuffer { id: shim_d3d11_create_tex_vertex_buffer(d.id, verts, dynamic) };
}

fn bind_tex_vertex_buffer(d: D3dDevice, buf: D3dBuffer) {
    shim_d3d11_bind_vertex_buffer(d.id, buf.id, 36);
}

fn create_index_buffer(d: D3dDevice, indices: const u32[:]) -> D3dBuffer {
    return D3dBuffer { id: shim_d3d11_create_index_buffer(d.id, indices) };
}

fn bind_index_buffer(d: D3dDevice, buf: D3dBuffer) {
    shim_d3d11_bind_index_buffer(d.id, buf.id);
}

fn draw_indexed(d: D3dDevice, index_count: u32, start_index: u32 = 0, base_vertex: i32 = 0) {
    shim_d3d11_draw_indexed(d.id, index_count, start_index, base_vertex);
}

fn create_texture_rgba(d: D3dDevice, width: i32, height: i32, pixels_rgba: const u32[:]) -> D3dTexture {
    return D3dTexture { id: shim_d3d11_create_texture_rgba(d.id, width, height, pixels_rgba) };
}

fn update_texture_rgba(d: D3dDevice, tex: D3dTexture, pixels_rgba: const u32[:]) -> bool {
    return shim_d3d11_update_texture_rgba(d.id, tex.id, pixels_rgba);
}

fn bind_texture(d: D3dDevice, slot: u32, tex: D3dTexture) {
    shim_d3d11_bind_texture(d.id, slot, tex.id);
}

fn destroy(t: D3dTexture) {
    shim_d3d11_destroy_texture(t.id);
}
