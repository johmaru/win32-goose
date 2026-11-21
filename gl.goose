// OpenGL 3.3 Core (WGL) GPU graphics API + direct opengl32.dll externs.
namespace win32;

import types;
import d3d11;

// Common OpenGL primitive & flag constants.
let GL_POINTS: u32 = 0x0000;
let GL_LINES: u32 = 0x0001;
let GL_LINE_LOOP: u32 = 0x0002;
let GL_LINE_STRIP: u32 = 0x0003;
let GL_TRIANGLES: u32 = 0x0004;
let GL_TRIANGLE_STRIP: u32 = 0x0005;
let GL_TRIANGLE_FAN: u32 = 0x0006;

let GL_DEPTH_BUFFER_BIT: u32 = 0x00000100;
let GL_STENCIL_BUFFER_BIT: u32 = 0x00000400;
let GL_COLOR_BUFFER_BIT: u32 = 0x00004000;

let GL_DEPTH_TEST: u32 = 0x0B71;
let GL_BLEND: u32 = 0x0BE2;
let GL_CULL_FACE: u32 = 0x0B44;

// Direct opengl32.dll dynamic externs (OpenGL 1.1 core functions).
extern "opengl32" {
    fn glViewport(x: i32, y: i32, width: i32, height: i32);
    fn glClearColor(r: f32, g: f32, b: f32, a: f32);
    fn glClear(mask: u32);
    fn glEnable(cap: u32);
    fn glDisable(cap: u32);
    fn glDrawArrays(mode: u32, first: i32, count: i32);
    fn glGetError() -> u32;
    fn glFlush();
    fn glFinish();
}

struct GlContext { id: u32 = 0 }
struct GlProgram { id: u32 = 0 }
struct GlMesh { id: u32 = 0 }
struct GlTexture { id: u32 = 0 }

fn is_valid(c: GlContext) -> bool { c.id != 0 }
fn is_valid(p: GlProgram) -> bool { p.id != 0 }
fn is_valid(m: GlMesh) -> bool { m.id != 0 }
fn is_valid(t: GlTexture) -> bool { t.id != 0 }

extern fn shim_gl_create(hwnd: u64, vsync: bool) -> u32 = "w32_gl_create";
extern fn shim_gl_destroy(ctx_id: u32) = "w32_gl_destroy";
extern fn shim_gl_make_current(ctx_id: u32) -> bool = "w32_gl_make_current";
extern fn shim_gl_swap_buffers(ctx_id: u32) -> bool = "w32_gl_swap_buffers";
extern fn shim_gl_set_vsync(ctx_id: u32, vsync: bool) = "w32_gl_set_vsync";
extern fn shim_gl_renderer_name(ctx_id: u32, out: u8[>..]&) -> bool = "w32_gl_renderer_name";
extern fn shim_gl_version_string(ctx_id: u32, out: u8[>..]&) -> bool = "w32_gl_version_string";
extern fn shim_gl_begin_frame(ctx_id: u32, r: f32, g: f32, b: f32, a: f32) = "w32_gl_begin_frame";
extern fn shim_gl_create_program(vs_src: const u8[:], fs_src: const u8[:], out_error: u8[>..]&) -> u32 = "w32_gl_create_program";
extern fn shim_gl_destroy_program(prog: u32) = "w32_gl_destroy_program";
extern fn shim_gl_use_program(prog: u32) = "w32_gl_use_program";
extern fn shim_gl_uniform_1f(prog: u32, name: const u8[:], v0: f32) = "w32_gl_uniform_1f";
extern fn shim_gl_uniform_2f(prog: u32, name: const u8[:], v0: f32, v1: f32) = "w32_gl_uniform_2f";
extern fn shim_gl_uniform_4f(prog: u32, name: const u8[:], v0: f32, v1: f32, v2: f32, v3: f32) = "w32_gl_uniform_4f";
extern fn shim_gl_create_vertex_buffer(verts: const D3dVertex[:], dynamic: bool) -> u32 = "w32_gl_create_vertex_buffer";
extern fn shim_gl_update_vertex_buffer(mesh_id: u32, verts: const D3dVertex[:]) = "w32_gl_update_vertex_buffer";
extern fn shim_gl_destroy_vertex_buffer(mesh_id: u32) = "w32_gl_destroy_vertex_buffer";
extern fn shim_gl_bind_vertex_buffer(mesh_id: u32) = "w32_gl_bind_vertex_buffer";
extern fn shim_gl_draw_arrays(mode: u32, first: i32, count: i32) = "w32_gl_draw_arrays";
extern fn shim_gl_readback_pixel(ctx_id: u32, x: i32, y: i32, out_rgba: u32&) -> bool = "w32_gl_readback_pixel";
extern fn shim_gl_uniform_1i(prog: u32, name: const u8[:], v0: i32) = "w32_gl_uniform_1i";
extern fn shim_gl_create_indexed_mesh(verts: const D3dTexVertex[:], indices: const u32[:], dynamic: bool) -> u32 = "w32_gl_create_indexed_mesh";
extern fn shim_gl_draw_indexed(mode: u32, index_count: i32) = "w32_gl_draw_indexed";
extern fn shim_gl_create_texture_rgba(w: i32, h: i32, pixels_rgba: const u32[:]) -> u32 = "w32_gl_create_texture_rgba";
extern fn shim_gl_update_texture_rgba(tex_id: u32, pixels_rgba: const u32[:]) -> bool = "w32_gl_update_texture_rgba";
extern fn shim_gl_bind_texture(slot: u32, tex_id: u32) = "w32_gl_bind_texture";
extern fn shim_gl_destroy_texture(tex_id: u32) = "w32_gl_destroy_texture";

fn gl_create(win: Hwnd, vsync: bool = true) -> GlContext {
    return GlContext { id: shim_gl_create(win.raw, vsync) };
}

fn destroy(c: GlContext) {
    shim_gl_destroy(c.id);
}

fn make_current(c: GlContext) -> bool {
    return shim_gl_make_current(c.id);
}

fn swap_buffers(c: GlContext) -> bool {
    return shim_gl_swap_buffers(c.id);
}

fn set_vsync(c: GlContext, vsync: bool) {
    shim_gl_set_vsync(c.id, vsync);
}

fn renderer_name(c: GlContext) -> u8[>..] {
    var out: u8[>..] = [];
    shim_gl_renderer_name(c.id, out);
    return out;
}

fn version(c: GlContext) -> u8[>..] {
    var out: u8[>..] = [];
    shim_gl_version_string(c.id, out);
    return out;
}

fn begin_frame(c: GlContext, r: f32, g: f32, b: f32, a: f32 = 1.0) {
    shim_gl_begin_frame(c.id, r, g, b, a);
}

fn end_frame(c: GlContext) -> bool {
    return shim_gl_swap_buffers(c.id);
}

fn create_program(c: GlContext, vs_src: const u8[:], fs_src: const u8[:], out_error: u8[>..]&) -> GlProgram {
    c.make_current();
    return GlProgram { id: shim_gl_create_program(vs_src, fs_src, out_error) };
}

fn destroy(p: GlProgram) {
    shim_gl_destroy_program(p.id);
}

fn use_program(c: GlContext, p: GlProgram) {
    c.make_current();
    shim_gl_use_program(p.id);
}

fn set_uniform(p: GlProgram, name: const u8[:], v0: f32) {
    shim_gl_uniform_1f(p.id, name, v0);
}

fn set_uniform(p: GlProgram, name: const u8[:], v0: f32, v1: f32) {
    shim_gl_uniform_2f(p.id, name, v0, v1);
}

fn set_uniform(p: GlProgram, name: const u8[:], v0: f32, v1: f32, v2: f32, v3: f32) {
    shim_gl_uniform_4f(p.id, name, v0, v1, v2, v3);
}

fn create_mesh(c: GlContext, verts: const D3dVertex[:], dynamic: bool = false) -> GlMesh {
    c.make_current();
    return GlMesh { id: shim_gl_create_vertex_buffer(verts, dynamic) };
}

fn update(m: GlMesh, verts: const D3dVertex[:]) {
    shim_gl_update_vertex_buffer(m.id, verts);
}

fn update_mesh(c: GlContext, m: GlMesh, verts: const D3dVertex[:]) {
    c.make_current();
    shim_gl_update_vertex_buffer(m.id, verts);
}

fn destroy(m: GlMesh) {
    shim_gl_destroy_vertex_buffer(m.id);
}

fn bind_mesh(c: GlContext, m: GlMesh) {
    c.make_current();
    shim_gl_bind_vertex_buffer(m.id);
}

fn draw(c: GlContext, count: i32, first: i32 = 0, mode: u32 = GL_TRIANGLES) {
    c.make_current();
    shim_gl_draw_arrays(mode, first, count);
}

fn readback_pixel(c: GlContext, x: i32, y: i32, out_rgba: u32&) -> bool {
    return shim_gl_readback_pixel(c.id, x, y, out_rgba);
}

// --- Textures & Indexed Meshes (EBO) ----------------------------------------

fn set_uniform_i32(p: GlProgram, name: const u8[:], v0: i32) {
    shim_gl_uniform_1i(p.id, name, v0);
}

fn create_indexed_mesh(c: GlContext, verts: const D3dTexVertex[:], indices: const u32[:], dynamic: bool = false) -> GlMesh {
    c.make_current();
    return GlMesh { id: shim_gl_create_indexed_mesh(verts, indices, dynamic) };
}

fn draw_indexed(c: GlContext, index_count: i32, mode: u32 = GL_TRIANGLES) {
    c.make_current();
    shim_gl_draw_indexed(mode, index_count);
}

fn create_texture_rgba(c: GlContext, width: i32, height: i32, pixels_rgba: const u32[:]) -> GlTexture {
    c.make_current();
    return GlTexture { id: shim_gl_create_texture_rgba(width, height, pixels_rgba) };
}

fn update_texture_rgba(c: GlContext, tex: GlTexture, pixels_rgba: const u32[:]) -> bool {
    c.make_current();
    return shim_gl_update_texture_rgba(tex.id, pixels_rgba);
}

fn bind_texture(c: GlContext, slot: u32, tex: GlTexture) {
    c.make_current();
    shim_gl_bind_texture(slot, tex.id);
}

fn destroy(t: GlTexture) {
    shim_gl_destroy_texture(t.id);
}
