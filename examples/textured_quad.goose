// Direct3D 11 textured indexed quad + WIC PNG generation/reload + borderless
// fullscreen (F11) + Raw Input relative mouse demo.
// Run: goose run examples/textured_quad.goose
import std;
import math;
import win32 from "..";

let HLSL: const u8[:] = "Texture2D tex0 : register(t0);\nSamplerState samp0 : register(s0);\nstruct VSIn { float3 pos : POSITION; float2 uv : TEXCOORD; float4 col : COLOR; };\nstruct PSIn { float4 pos : SV_POSITION; float2 uv : TEXCOORD; float4 col : COLOR; };\nPSIn vs_main(VSIn i) {\n    PSIn o;\n    o.pos = float4(i.pos, 1.0);\n    o.uv = i.uv;\n    o.col = i.col;\n    return o;\n}\nfloat4 ps_main(PSIn i) : SV_TARGET {\n    return tex0.Sample(samp0, i.uv) * i.col;\n}\n";

fn main() {
    win32::enable_dpi_awareness();

    let win = win32::create_window("win32-goose — Textured Quad (F11: Fullscreen, ESC: Quit)", 900, 620);
    guard win.is_valid() else {
        print("Failed to create window");
        return;
    }
    defer win.destroy();
    win.set_dark_titlebar(true);
    win.enable_raw_mouse(true);
    win.show(win32::SW_SHOWNORMAL);

    let dev = win32::d3d11_create(win, true);
    guard dev.is_valid() else {
        print("Failed to create D3D11 device");
        return;
    }
    defer dev.destroy();

    var err: u8[>..] = [];
    let pipe = dev.create_pipeline_textured(HLSL, err);
    guard pipe.is_valid() else {
        print("Shader compile failed: ", err);
        return;
    }
    defer pipe.destroy();

    // Generate a 128x128 checkerboard texture, round-trip through WIC PNG!
    let tw: i32 = 128;
    let th: i32 = 128;
    var bgra: u32[>..] = [];
    for y in th {
        for x in tw {
            let checker = ((x >> 4) ^ (y >> 4)) & 1;
            let r = (x * 2) as! u8;
            let g = (y * 2) as! u8;
            let b: u8 = if checker == 0 { 220 } else { 70 };
            bgra.push(win32::argb(r, g, b, 255));
        }
    }

    var png_path: u8[>..] = [];
    format(png_path, win32::temp_dir(), "win32_goose_demo_tex.png");
    win32::image_save_png(png_path, bgra, tw, th);
    defer win32::delete_file(png_path);

    var rgba: u32[>..] = [];
    var lw: i32 = 0;
    var lh: i32 = 0;
    guard win32::image_load_rgba(png_path, rgba, lw, lh) else {
        print("Failed to reload WIC PNG");
        return;
    }

    let tex = dev.create_texture_rgba(lw, lh, rgba);
    guard tex.is_valid() else { return; }
    defer tex.destroy();

    let quad: win32::D3dTexVertex[4] = [
        win32::D3dTexVertex { x: -0.65, y:  0.65, z: 0.0, u: 0.0, v: 0.0 },
        win32::D3dTexVertex { x:  0.65, y:  0.65, z: 0.0, u: 1.0, v: 0.0 },
        win32::D3dTexVertex { x:  0.65, y: -0.65, z: 0.0, u: 1.0, v: 1.0 },
        win32::D3dTexVertex { x: -0.65, y: -0.65, z: 0.0, u: 0.0, v: 1.0 },
    ];
    let indices: u32[6] = [0, 1, 2, 0, 2, 3];

    let vb = dev.create_tex_vertex_buffer(quad, true);
    let ib = dev.create_index_buffer(indices);
    guard vb.is_valid() && ib.is_valid() else { return; }
    defer vb.destroy();
    defer ib.destroy();

    let freq = win32::perf_frequency() as f64;
    let start = win32::perf_counter();
    var fullscreen = false;
    var raw_dx: i32 = 0;
    var raw_dy: i32 = 0;
    var ev = win32::Event {};
    var running = true;

    while running {
        while win32::poll_event(ev) {
            if ev.msg == win32::WM_QUIT {
                running = false;
            } else if ev.msg == win32::WM_CLOSE {
                running = false;
            } else if ev.msg == win32::WM_DESTROY {
                win32::post_quit_message(0);
            } else if ev.msg == win32::WM_SIZE && ev.width > 0 && ev.height > 0 {
                dev.resize(ev.width, ev.height);
            } else if ev.msg == win32::WM_INPUT {
                raw_dx += ev.x;
                raw_dy += ev.y;
            } else if ev.msg == win32::WM_KEYDOWN {
                if ev.key == win32::VK_ESCAPE {
                    win.close();
                } else if ev.key == win32::VK_F11 {
                    fullscreen = !fullscreen;
                    win.set_borderless_fullscreen(fullscreen);
                }
            }
        }
        guard running else { break; }

        let t = ((win32::perf_counter() - start) / freq) as! f32;
        let bg = 0.08 + 0.04 * sin(t * 2.0);
        dev.begin_frame(bg, 0.08, 0.14, 1.0);
        dev.bind_pipeline(pipe);
        dev.bind_tex_vertex_buffer(vb);
        dev.bind_index_buffer(ib);
        dev.bind_texture(0, tex);
        dev.draw_indexed(6);
        dev.end_frame();
    }
    print("Exiting (total raw mouse delta: ", raw_dx, ", ", raw_dy, ")");
}
