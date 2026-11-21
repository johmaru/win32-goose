// Interactive OpenGL 3.3 Core (WGL) + GLSL spinning triangle demo.
// Run with:   goose run examples/gl_triangle.goose
// Build with: goose build examples/gl_triangle.goose
import std;
import math;
import win32 from "..";

let VS_GLSL: const u8[:] = """
#version 330 core
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec4 a_col;
out vec4 v_col;
void main() {
    gl_Position = vec4(a_pos, 1.0);
    v_col = a_col;
}
""";

let FS_GLSL: const u8[:] = """
#version 330 core
in vec4 v_col;
uniform float u_time;
out vec4 frag_color;
void main() {
    float pulse = 0.85 + 0.15 * sin(u_time * 3.0);
    frag_color = vec4(v_col.rgb * pulse, v_col.a);
}
""";

fn main() {
    win32::enable_dpi_awareness();

    let win = win32::create_window("win32-goose — OpenGL 3.3 Core Triangle", 800, 560);
    guard win.is_valid() else { abort("create_window failed"); }

    let gl = win32::gl_create(win, true);
    guard gl.is_valid() else { abort("gl_create failed"); }
    print("OpenGL Renderer: ", gl.renderer_name());
    print("OpenGL Version:  ", gl.version());

    var err: u8[>..] = [];
    let prog = gl.create_program(VS_GLSL, FS_GLSL, err);
    guard prog.is_valid() else {
        abort(str("GLSL compile/link failed: ", err));
    }

    var verts: win32::D3dVertex[3] = [
        win32::D3dVertex { x:  0.0, y:  0.65, z: 0.0, r: 1.0, g: 0.30, b: 0.35, a: 1.0 },
        win32::D3dVertex { x:  0.6, y: -0.55, z: 0.0, r: 0.2, g: 0.90, b: 0.60, a: 1.0 },
        win32::D3dVertex { x: -0.6, y: -0.55, z: 0.0, r: 0.3, g: 0.55, b: 1.00, a: 1.0 },
    ];
    let mesh = gl.create_mesh(verts, true);
    assert(mesh.is_valid());

    win.show(win32::SW_SHOW);
    win.update();

    let t0 = win32::tick_count_ms();
    var running = true;
    var ev = win32::Event {};

    while running {
        while win32::poll_event(ev) {
            if ev.msg == win32::WM_QUIT {
                running = false;
            } else if ev.msg == win32::WM_CLOSE {
                win.destroy();
            } else if ev.msg == win32::WM_DESTROY {
                win32::post_quit_message(0);
            } else if ev.msg == win32::WM_KEYDOWN && ev.key == win32::VK_ESCAPE {
                win.close();
            }
        }
        guard running else { break; }

        let t = ((win32::tick_count_ms() - t0) as f64) * 0.0012;
        let rc = win.client_rect();
        let aspect = if rc.height() > 0 { (rc.width() as f64) / rc.height() } else { 1.0 };

        for i in 3 {
            let a = t + i * 2.0943951023931953;
            verts[i].x = (cos(a) * 0.65 / aspect) as f32;
            verts[i].y = (sin(a) * 0.65) as f32;
        }
        mesh.update(verts);

        gl.begin_frame(0.06, 0.08, 0.13, 1.0);
        gl.use_program(prog);
        prog.set_uniform("u_time", t as f32);
        gl.bind_mesh(mesh);
        gl.draw(3);
        gl.end_frame();
    }

    mesh.destroy();
    prog.destroy();
    gl.destroy();
}
