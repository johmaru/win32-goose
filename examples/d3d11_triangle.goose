// Interactive Direct3D 11 + HLSL spinning RGB triangle demo.
// Run with:   goose run examples/d3d11_triangle.goose
// Build with: goose build examples/d3d11_triangle.goose
import std;
import math;
import win32 from "..";

let HLSL_SRC: const u8[:] = """
struct VSIn {
    float3 pos : POSITION;
    float4 col : COLOR;
};

struct PSIn {
    float4 pos : SV_POSITION;
    float4 col : COLOR;
};

PSIn vs_main(VSIn v) {
    PSIn o;
    o.pos = float4(v.pos, 1.0);
    o.col = v.col;
    return o;
}

float4 ps_main(PSIn p) : SV_TARGET {
    return p.col;
}
""";

fn main() {
    win32::enable_dpi_awareness();

    let win = win32::create_window("win32-goose — Direct3D 11 Triangle", 800, 560);
    guard win.is_valid() else { abort("create_window failed"); }

    let dev = win32::d3d11_create(win, true);
    guard dev.is_valid() else { abort("d3d11_create failed"); }
    print("D3D11 Adapter: ", dev.adapter_name());

    var shader_err: u8[>..] = [];
    let pipe = dev.create_pipeline(HLSL_SRC, shader_err);
    guard pipe.is_valid() else {
        abort(str("HLSL compile failed: ", shader_err));
    }

    var verts: win32::D3dVertex[3] = [
        win32::D3dVertex { x:  0.0, y:  0.65, z: 0.0, r: 1.0, g: 0.25, b: 0.35, a: 1.0 },
        win32::D3dVertex { x:  0.6, y: -0.55, z: 0.0, r: 0.2, g: 0.90, b: 0.55, a: 1.0 },
        win32::D3dVertex { x: -0.6, y: -0.55, z: 0.0, r: 0.25, g: 0.55, b: 1.0, a: 1.0 },
    ];
    let vbuf = dev.create_vertex_buffer(verts, true);
    assert(vbuf.is_valid());

    win.show(win32::SW_SHOW);
    win.update();

    let t0 = win32::tick_count_ms();
    var running = true;
    var ev = win32::Event {};

    while running {
        while win32::poll_event(ev) {
            if ev.msg == win32::WM_QUIT || ev.msg == win32::WM_CLOSE {
                running = false;
            } else if ev.msg == win32::WM_DESTROY {
                win32::post_quit_message(0);
            } else if ev.msg == win32::WM_SIZE && ev.width > 0 && ev.height > 0 {
                dev.resize(ev.width, ev.height);
            } else if ev.msg == win32::WM_KEYDOWN && ev.key == win32::VK_ESCAPE {
                win.close();
            }
        }
        guard running else { break; }

        let angle = ((win32::tick_count_ms() - t0) as f64) * 0.0012;
        let rc = win.client_rect();
        let aspect = if rc.height() > 0 { (rc.width() as f64) / rc.height() } else { 1.0 };

        for i in 3 {
            let a = angle + i * 2.0943951023931953; // 120 deg
            verts[i].x = (cos(a) * 0.65 / aspect) as f32;
            verts[i].y = (sin(a) * 0.65) as f32;
        }
        dev.update_vertex_buffer(vbuf, verts);

        dev.begin_frame(0.08, 0.10, 0.14, 1.0);
        dev.bind_pipeline(pipe);
        dev.bind_vertex_buffer(vbuf);
        dev.draw(3);
        dev.end_frame();
    }

    vbuf.destroy();
    pipe.destroy();
    dev.destroy();
    win.destroy();
}
