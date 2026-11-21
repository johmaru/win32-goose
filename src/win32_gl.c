#include <stdlib.h>
#include <string.h>

#include "win32_internal.h"
#include <GL/gl.h>

/* ---- WGL & OpenGL 3.3 Core constants & function pointers ----------------- */

#define WGL_DRAW_TO_WINDOW_ARB         0x2001
#define WGL_SUPPORT_OPENGL_ARB         0x2010
#define WGL_DOUBLE_BUFFER_ARB          0x2011
#define WGL_PIXEL_TYPE_ARB             0x2013
#define WGL_TYPE_RGBA_ARB              0x202B
#define WGL_COLOR_BITS_ARB             0x2014
#define WGL_DEPTH_BITS_ARB             0x2022
#define WGL_STENCIL_BITS_ARB           0x2023

#define WGL_CONTEXT_MAJOR_VERSION_ARB  0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB  0x2092
#define WGL_CONTEXT_PROFILE_MASK_ARB   0x9126
#define WGL_CONTEXT_CORE_PROFILE_BIT   0x00000001

#define GL_ARRAY_BUFFER                0x8892
#define GL_ELEMENT_ARRAY_BUFFER        0x8893
#define GL_STATIC_DRAW                 0x88E4
#define GL_DYNAMIC_DRAW                0x88E8
#define GL_TEXTURE0                    0x84C0
#define GL_CLAMP_TO_EDGE               0x812F
#define GL_FRAGMENT_SHADER             0x8B30
#define GL_VERTEX_SHADER               0x8B31
#define GL_COMPILE_STATUS              0x8B81
#define GL_LINK_STATUS                 0x8B82
#define GL_INFO_LOG_LENGTH             0x8B84

typedef ptrdiff_t GLsizeiptr;
typedef ptrdiff_t GLintptr;
typedef char GLchar;

typedef BOOL  (WINAPI *pfn_wglChoosePixelFormatARB)(HDC, const int *, const FLOAT *, UINT, int *, UINT *);
typedef HGLRC (WINAPI *pfn_wglCreateContextAttribsARB)(HDC, HGLRC, const int *);
typedef BOOL  (WINAPI *pfn_wglSwapIntervalEXT)(int);

typedef GLuint (APIENTRY *pfn_glCreateShader)(GLenum);
typedef void   (APIENTRY *pfn_glShaderSource)(GLuint, GLsizei, const GLchar *const *, const GLint *);
typedef void   (APIENTRY *pfn_glCompileShader)(GLuint);
typedef void   (APIENTRY *pfn_glGetShaderiv)(GLuint, GLenum, GLint *);
typedef void   (APIENTRY *pfn_glGetShaderInfoLog)(GLuint, GLsizei, GLsizei *, GLchar *);
typedef void   (APIENTRY *pfn_glDeleteShader)(GLuint);
typedef GLuint (APIENTRY *pfn_glCreateProgram)(void);
typedef void   (APIENTRY *pfn_glAttachShader)(GLuint, GLuint);
typedef void   (APIENTRY *pfn_glLinkProgram)(GLuint);
typedef void   (APIENTRY *pfn_glGetProgramiv)(GLuint, GLenum, GLint *);
typedef void   (APIENTRY *pfn_glGetProgramInfoLog)(GLuint, GLsizei, GLsizei *, GLchar *);
typedef void   (APIENTRY *pfn_glDeleteProgram)(GLuint);
typedef void   (APIENTRY *pfn_glUseProgram)(GLuint);
typedef GLint  (APIENTRY *pfn_glGetUniformLocation)(GLuint, const GLchar *);
typedef void   (APIENTRY *pfn_glUniform1i)(GLint, GLint);
typedef void   (APIENTRY *pfn_glUniform1f)(GLint, GLfloat);
typedef void   (APIENTRY *pfn_glUniform2f)(GLint, GLfloat, GLfloat);
typedef void   (APIENTRY *pfn_glUniform4f)(GLint, GLfloat, GLfloat, GLfloat, GLfloat);
typedef void   (APIENTRY *pfn_glActiveTexture)(GLenum);
typedef void   (APIENTRY *pfn_glGenVertexArrays)(GLsizei, GLuint *);
typedef void   (APIENTRY *pfn_glBindVertexArray)(GLuint);
typedef void   (APIENTRY *pfn_glDeleteVertexArrays)(GLsizei, const GLuint *);
typedef void   (APIENTRY *pfn_glGenBuffers)(GLsizei, GLuint *);
typedef void   (APIENTRY *pfn_glBindBuffer)(GLenum, GLuint);
typedef void   (APIENTRY *pfn_glBufferData)(GLenum, GLsizeiptr, const void *, GLenum);
typedef void   (APIENTRY *pfn_glBufferSubData)(GLenum, GLintptr, GLsizeiptr, const void *);
typedef void   (APIENTRY *pfn_glDeleteBuffers)(GLsizei, const GLuint *);
typedef void   (APIENTRY *pfn_glEnableVertexAttribArray)(GLuint);
typedef void   (APIENTRY *pfn_glVertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void *);

static pfn_wglChoosePixelFormatARB    g_wglChoosePixelFormatARB;
static pfn_wglCreateContextAttribsARB g_wglCreateContextAttribsARB;
static pfn_wglSwapIntervalEXT         g_wglSwapIntervalEXT;

static pfn_glCreateShader             g_glCreateShader;
static pfn_glShaderSource             g_glShaderSource;
static pfn_glCompileShader            g_glCompileShader;
static pfn_glGetShaderiv              g_glGetShaderiv;
static pfn_glGetShaderInfoLog         g_glGetShaderInfoLog;
static pfn_glDeleteShader             g_glDeleteShader;
static pfn_glCreateProgram            g_glCreateProgram;
static pfn_glAttachShader             g_glAttachShader;
static pfn_glLinkProgram              g_glLinkProgram;
static pfn_glGetProgramiv             g_glGetProgramiv;
static pfn_glGetProgramInfoLog        g_glGetProgramInfoLog;
static pfn_glDeleteProgram            g_glDeleteProgram;
static pfn_glUseProgram               g_glUseProgram;
static pfn_glGetUniformLocation       g_glGetUniformLocation;
static pfn_glUniform1i                g_glUniform1i;
static pfn_glUniform1f                g_glUniform1f;
static pfn_glUniform2f                g_glUniform2f;
static pfn_glUniform4f                g_glUniform4f;
static pfn_glActiveTexture            g_glActiveTexture;
static pfn_glGenVertexArrays          g_glGenVertexArrays;
static pfn_glBindVertexArray          g_glBindVertexArray;
static pfn_glDeleteVertexArrays       g_glDeleteVertexArrays;
static pfn_glGenBuffers               g_glGenBuffers;
static pfn_glBindBuffer               g_glBindBuffer;
static pfn_glBufferData               g_glBufferData;
static pfn_glBufferSubData            g_glBufferSubData;
static pfn_glDeleteBuffers            g_glDeleteBuffers;
static pfn_glEnableVertexAttribArray  g_glEnableVertexAttribArray;
static pfn_glVertexAttribPointer      g_glVertexAttribPointer;

static int g_wgl_bootstrapped = 0;
static int g_gl_funcs_loaded = 0;

static void *load_gl_proc(const char *name) {
    void *p = (void *)(uintptr_t)wglGetProcAddress(name);
    if (!p || p == (void *)1 || p == (void *)2 || p == (void *)3 || p == (void *)(intptr_t)-1) {
        HMODULE ogl = GetModuleHandleW(L"opengl32.dll");
        if (ogl) p = (void *)(uintptr_t)GetProcAddress(ogl, name);
    }
    return p;
}

static void bootstrap_wgl_extensions(void) {
    if (g_wgl_bootstrapped) return;
    g_wgl_bootstrapped = 1;

    WNDCLASSW wc;
    memset(&wc, 0, sizeof(wc));
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = L"Win32GooseWGLDummy";
    RegisterClassW(&wc);

    HWND dummy = CreateWindowW(
        L"Win32GooseWGLDummy", L"", WS_POPUP,
        0, 0, 1, 1, NULL, NULL, wc.hInstance, NULL
    );
    if (!dummy) return;

    HDC dc = GetDC(dummy);
    if (dc) {
        PIXELFORMATDESCRIPTOR pfd;
        memset(&pfd, 0, sizeof(pfd));
        pfd.nSize = sizeof(pfd);
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = 32;
        pfd.cDepthBits = 24;
        pfd.cStencilBits = 8;

        int pf = ChoosePixelFormat(dc, &pfd);
        if (pf && SetPixelFormat(dc, pf, &pfd)) {
            HGLRC rc = wglCreateContext(dc);
            if (rc) {
                if (wglMakeCurrent(dc, rc)) {
                    g_wglChoosePixelFormatARB =
                        (pfn_wglChoosePixelFormatARB)load_gl_proc("wglChoosePixelFormatARB");
                    g_wglCreateContextAttribsARB =
                        (pfn_wglCreateContextAttribsARB)load_gl_proc("wglCreateContextAttribsARB");
                    g_wglSwapIntervalEXT =
                        (pfn_wglSwapIntervalEXT)load_gl_proc("wglSwapIntervalEXT");
                    wglMakeCurrent(NULL, NULL);
                }
                wglDeleteContext(rc);
            }
        }
        ReleaseDC(dummy, dc);
    }
    DestroyWindow(dummy);
}

static int load_gl33_funcs(void) {
    if (g_gl_funcs_loaded) return 1;
    g_glCreateShader            = (pfn_glCreateShader)load_gl_proc("glCreateShader");
    g_glShaderSource            = (pfn_glShaderSource)load_gl_proc("glShaderSource");
    g_glCompileShader           = (pfn_glCompileShader)load_gl_proc("glCompileShader");
    g_glGetShaderiv             = (pfn_glGetShaderiv)load_gl_proc("glGetShaderiv");
    g_glGetShaderInfoLog        = (pfn_glGetShaderInfoLog)load_gl_proc("glGetShaderInfoLog");
    g_glDeleteShader            = (pfn_glDeleteShader)load_gl_proc("glDeleteShader");
    g_glCreateProgram           = (pfn_glCreateProgram)load_gl_proc("glCreateProgram");
    g_glAttachShader            = (pfn_glAttachShader)load_gl_proc("glAttachShader");
    g_glLinkProgram             = (pfn_glLinkProgram)load_gl_proc("glLinkProgram");
    g_glGetProgramiv            = (pfn_glGetProgramiv)load_gl_proc("glGetProgramiv");
    g_glGetProgramInfoLog       = (pfn_glGetProgramInfoLog)load_gl_proc("glGetProgramInfoLog");
    g_glDeleteProgram           = (pfn_glDeleteProgram)load_gl_proc("glDeleteProgram");
    g_glUseProgram              = (pfn_glUseProgram)load_gl_proc("glUseProgram");
    g_glGetUniformLocation      = (pfn_glGetUniformLocation)load_gl_proc("glGetUniformLocation");
    g_glUniform1i               = (pfn_glUniform1i)load_gl_proc("glUniform1i");
    g_glUniform1f               = (pfn_glUniform1f)load_gl_proc("glUniform1f");
    g_glUniform2f               = (pfn_glUniform2f)load_gl_proc("glUniform2f");
    g_glUniform4f               = (pfn_glUniform4f)load_gl_proc("glUniform4f");
    g_glActiveTexture           = (pfn_glActiveTexture)load_gl_proc("glActiveTexture");
    g_glGenVertexArrays         = (pfn_glGenVertexArrays)load_gl_proc("glGenVertexArrays");
    g_glBindVertexArray         = (pfn_glBindVertexArray)load_gl_proc("glBindVertexArray");
    g_glDeleteVertexArrays      = (pfn_glDeleteVertexArrays)load_gl_proc("glDeleteVertexArrays");
    g_glGenBuffers              = (pfn_glGenBuffers)load_gl_proc("glGenBuffers");
    g_glBindBuffer              = (pfn_glBindBuffer)load_gl_proc("glBindBuffer");
    g_glBufferData              = (pfn_glBufferData)load_gl_proc("glBufferData");
    g_glBufferSubData           = (pfn_glBufferSubData)load_gl_proc("glBufferSubData");
    g_glDeleteBuffers           = (pfn_glDeleteBuffers)load_gl_proc("glDeleteBuffers");
    g_glEnableVertexAttribArray = (pfn_glEnableVertexAttribArray)load_gl_proc("glEnableVertexAttribArray");
    g_glVertexAttribPointer     = (pfn_glVertexAttribPointer)load_gl_proc("glVertexAttribPointer");

    if (g_glCreateShader && g_glShaderSource && g_glCompileShader &&
        g_glGetShaderiv && g_glGetShaderInfoLog && g_glDeleteShader &&
        g_glCreateProgram && g_glAttachShader && g_glLinkProgram &&
        g_glGetProgramiv && g_glGetProgramInfoLog && g_glDeleteProgram &&
        g_glUseProgram && g_glGetUniformLocation &&
        g_glUniform1i && g_glUniform1f && g_glUniform2f && g_glUniform4f &&
        g_glActiveTexture &&
        g_glGenVertexArrays && g_glBindVertexArray && g_glDeleteVertexArrays &&
        g_glGenBuffers && g_glBindBuffer && g_glBufferData &&
        g_glBufferSubData && g_glDeleteBuffers &&
        g_glEnableVertexAttribArray && g_glVertexAttribPointer) {
        g_gl_funcs_loaded = 1;
        return 1;
    }
    return 0;
}

/* ---- Context & Mesh slot tables ------------------------------------------ */

#define W32_MAX_GL_CTXS   8
#define W32_MAX_GL_MESHES 128

typedef struct {
    int used;
    HWND hwnd;
    HDC hdc;
    HGLRC hglrc;
} w32_gl_ctx;

typedef struct {
    int used;
    GLuint vao;
    GLuint vbo;
    GLuint ebo;
    size_t byte_size;
    uint8_t dynamic;
    uint8_t is_textured;
} w32_gl_mesh;

static w32_gl_ctx g_gl_ctxs[W32_MAX_GL_CTXS];
static w32_gl_mesh g_gl_meshes[W32_MAX_GL_MESHES];

static w32_gl_ctx *get_ctx(uint32_t id) {
    if (id == 0 || id > W32_MAX_GL_CTXS) return NULL;
    return g_gl_ctxs[id - 1].used ? &g_gl_ctxs[id - 1] : NULL;
}

static w32_gl_mesh *get_mesh(uint32_t id) {
    if (id == 0 || id > W32_MAX_GL_MESHES) return NULL;
    return g_gl_meshes[id - 1].used ? &g_gl_meshes[id - 1] : NULL;
}

/* ---- Context lifecycle --------------------------------------------------- */

uint32_t w32_gl_create(uint64_t hwnd_u64, uint8_t vsync) {
    HWND hwnd = (HWND)(uintptr_t)hwnd_u64;
    if (!hwnd) return 0;

    int slot = -1;
    for (int i = 0; i < W32_MAX_GL_CTXS; i++) {
        if (!g_gl_ctxs[i].used) { slot = i; break; }
    }
    if (slot < 0) return 0;

    bootstrap_wgl_extensions();

    HDC hdc = GetDC(hwnd);
    if (!hdc) return 0;

    PIXELFORMATDESCRIPTOR pfd;
    memset(&pfd, 0, sizeof(pfd));
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;

    int pf = 0;
    if (g_wglChoosePixelFormatARB) {
        const int attribs[] = {
            WGL_DRAW_TO_WINDOW_ARB, 1,
            WGL_SUPPORT_OPENGL_ARB, 1,
            WGL_DOUBLE_BUFFER_ARB,  1,
            WGL_PIXEL_TYPE_ARB,     WGL_TYPE_RGBA_ARB,
            WGL_COLOR_BITS_ARB,     32,
            WGL_DEPTH_BITS_ARB,     24,
            WGL_STENCIL_BITS_ARB,   8,
            0
        };
        UINT num_formats = 0;
        if (!g_wglChoosePixelFormatARB(hdc, attribs, NULL, 1, &pf, &num_formats) || num_formats == 0) {
            pf = 0;
        }
    }
    if (!pf) {
        pf = ChoosePixelFormat(hdc, &pfd);
    }
    if (!pf) {
        ReleaseDC(hwnd, hdc);
        return 0;
    }
    DescribePixelFormat(hdc, pf, sizeof(pfd), &pfd);
    if (!SetPixelFormat(hdc, pf, &pfd)) {
        ReleaseDC(hwnd, hdc);
        return 0;
    }

    HGLRC hglrc = NULL;
    if (g_wglCreateContextAttribsARB) {
        const int ctx_attribs[] = {
            WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
            WGL_CONTEXT_MINOR_VERSION_ARB, 3,
            WGL_CONTEXT_PROFILE_MASK_ARB,  WGL_CONTEXT_CORE_PROFILE_BIT,
            0
        };
        hglrc = g_wglCreateContextAttribsARB(hdc, NULL, ctx_attribs);
    }
    if (!hglrc) {
        hglrc = wglCreateContext(hdc);
    }
    if (!hglrc) {
        ReleaseDC(hwnd, hdc);
        return 0;
    }

    if (!wglMakeCurrent(hdc, hglrc)) {
        wglDeleteContext(hglrc);
        ReleaseDC(hwnd, hdc);
        return 0;
    }

    if (!load_gl33_funcs()) {
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(hglrc);
        ReleaseDC(hwnd, hdc);
        return 0;
    }
    if (g_wglSwapIntervalEXT) {
        g_wglSwapIntervalEXT(vsync ? 1 : 0);
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    w32_gl_ctx *c = &g_gl_ctxs[slot];
    c->used = 1;
    c->hwnd = hwnd;
    c->hdc = hdc;
    c->hglrc = hglrc;
    return (uint32_t)(slot + 1);
}

void w32_gl_destroy(uint32_t ctx_id) {
    w32_gl_ctx *c = get_ctx(ctx_id);
    if (!c) return;
    if (wglGetCurrentContext() == c->hglrc) {
        wglMakeCurrent(NULL, NULL);
    }
    if (c->hglrc) wglDeleteContext(c->hglrc);
    if (c->hdc && c->hwnd) ReleaseDC(c->hwnd, c->hdc);
    memset(c, 0, sizeof(*c));
}

uint8_t w32_gl_make_current(uint32_t ctx_id) {
    w32_gl_ctx *c = get_ctx(ctx_id);
    if (!c) return 0;
    return wglMakeCurrent(c->hdc, c->hglrc) ? 1 : 0;
}

uint8_t w32_gl_swap_buffers(uint32_t ctx_id) {
    w32_gl_ctx *c = get_ctx(ctx_id);
    if (!c) return 0;
    return SwapBuffers(c->hdc) ? 1 : 0;
}

void w32_gl_set_vsync(uint32_t ctx_id, uint8_t vsync) {
    w32_gl_ctx *c = get_ctx(ctx_id);
    if (!c || !g_wglSwapIntervalEXT) return;
    wglMakeCurrent(c->hdc, c->hglrc);
    g_wglSwapIntervalEXT(vsync ? 1 : 0);
}

uint8_t w32_gl_renderer_name(uint32_t ctx_id, gs_rref out) {
    w32_gl_ctx *c = get_ctx(ctx_id);
    if (!c) return 0;
    wglMakeCurrent(c->hdc, c->hglrc);
    const char *s = (const char *)glGetString(GL_RENDERER);
    if (!s) return 0;
    gs_bld_append(out, s, (int64_t)strlen(s));
    return 1;
}

uint8_t w32_gl_version_string(uint32_t ctx_id, gs_rref out) {
    w32_gl_ctx *c = get_ctx(ctx_id);
    if (!c) return 0;
    wglMakeCurrent(c->hdc, c->hglrc);
    const char *s = (const char *)glGetString(GL_VERSION);
    if (!s) return 0;
    gs_bld_append(out, s, (int64_t)strlen(s));
    return 1;
}

void w32_gl_begin_frame(uint32_t ctx_id, float r, float g, float b, float a) {
    w32_gl_ctx *c = get_ctx(ctx_id);
    if (!c) return;
    wglMakeCurrent(c->hdc, c->hglrc);
    RECT rc;
    if (GetClientRect(c->hwnd, &rc)) {
        int w = (int)(rc.right - rc.left);
        int h = (int)(rc.bottom - rc.top);
        if (w < 1) w = 1;
        if (h < 1) h = 1;
        glViewport(0, 0, w, h);
    }
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

/* ---- GLSL Shaders & Uniforms --------------------------------------------- */

static GLuint compile_shader(GLenum kind, w32_bytes src, gs_rref out_error) {
    if (!g_glCreateShader || !src.data || src.len <= 0) return 0;
    GLuint sh = g_glCreateShader(kind);
    if (!sh) return 0;

    const GLchar *ptr = (const GLchar *)src.data;
    GLint len = (GLint)src.len;
    g_glShaderSource(sh, 1, &ptr, &len);
    g_glCompileShader(sh);

    GLint ok = 0;
    g_glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint log_len = 0;
        g_glGetShaderiv(sh, GL_INFO_LOG_LENGTH, &log_len);
        if (log_len > 1) {
            char *log = (char *)malloc((size_t)log_len);
            if (log) {
                GLsizei written = 0;
                g_glGetShaderInfoLog(sh, log_len, &written, log);
                if (written > 0) gs_bld_append(out_error, log, (int64_t)written);
                free(log);
            }
        }
        g_glDeleteShader(sh);
        return 0;
    }
    return sh;
}

uint32_t w32_gl_create_program(w32_bytes vs_src, w32_bytes fs_src, gs_rref out_error) {
    if (!g_gl_funcs_loaded) return 0;
    GLuint vs = compile_shader(GL_VERTEX_SHADER, vs_src, out_error);
    if (!vs) return 0;
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fs_src, out_error);
    if (!fs) {
        g_glDeleteShader(vs);
        return 0;
    }

    GLuint prog = g_glCreateProgram();
    if (!prog) {
        g_glDeleteShader(vs);
        g_glDeleteShader(fs);
        return 0;
    }
    g_glAttachShader(prog, vs);
    g_glAttachShader(prog, fs);
    g_glLinkProgram(prog);
    g_glDeleteShader(vs);
    g_glDeleteShader(fs);

    GLint ok = 0;
    g_glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint log_len = 0;
        g_glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &log_len);
        if (log_len > 1) {
            char *log = (char *)malloc((size_t)log_len);
            if (log) {
                GLsizei written = 0;
                g_glGetProgramInfoLog(prog, log_len, &written, log);
                if (written > 0) gs_bld_append(out_error, log, (int64_t)written);
                free(log);
            }
        }
        g_glDeleteProgram(prog);
        return 0;
    }
    return (uint32_t)prog;
}

void w32_gl_destroy_program(uint32_t prog) {
    if (prog && g_glDeleteProgram) g_glDeleteProgram((GLuint)prog);
}

void w32_gl_use_program(uint32_t prog) {
    if (g_glUseProgram) g_glUseProgram((GLuint)prog);
}

static GLint get_uniform_loc(uint32_t prog, w32_bytes name) {
    if (!prog || !g_glGetUniformLocation || !name.data || name.len <= 0) return -1;
    char stack[128];
    char *buf = stack;
    if (name.len + 1 > (int64_t)sizeof(stack)) {
        buf = (char *)malloc((size_t)name.len + 1);
        if (!buf) return -1;
    }
    memcpy(buf, name.data, (size_t)name.len);
    buf[name.len] = '\0';
    GLint loc = g_glGetUniformLocation((GLuint)prog, buf);
    if (buf != stack) free(buf);
    return loc;
}

void w32_gl_uniform_1f(uint32_t prog, w32_bytes name, float v0) {
    GLint loc = get_uniform_loc(prog, name);
    if (loc >= 0 && g_glUniform1f) g_glUniform1f(loc, v0);
}

void w32_gl_uniform_2f(uint32_t prog, w32_bytes name, float v0, float v1) {
    GLint loc = get_uniform_loc(prog, name);
    if (loc >= 0 && g_glUniform2f) g_glUniform2f(loc, v0, v1);
}

void w32_gl_uniform_4f(uint32_t prog, w32_bytes name, float v0, float v1, float v2, float v3) {
    GLint loc = get_uniform_loc(prog, name);
    if (loc >= 0 && g_glUniform4f) g_glUniform4f(loc, v0, v1, v2, v3);
}

/* ---- VAO + VBO Vertex Buffers -------------------------------------------- */

uint32_t w32_gl_create_vertex_buffer(w32_d3d_vertex_slice verts, uint8_t dynamic) {
    if (!g_gl_funcs_loaded || !verts.data || verts.len <= 0) return 0;

    int slot = -1;
    for (int i = 0; i < W32_MAX_GL_MESHES; i++) {
        if (!g_gl_meshes[i].used) { slot = i; break; }
    }
    if (slot < 0) return 0;

    GLuint vao = 0, vbo = 0;
    g_glGenVertexArrays(1, &vao);
    g_glGenBuffers(1, &vbo);
    if (!vao || !vbo) {
        if (vbo) g_glDeleteBuffers(1, &vbo);
        if (vao) g_glDeleteVertexArrays(1, &vao);
        return 0;
    }

    size_t bytes = (size_t)verts.len * sizeof(w32_d3d_vertex);
    g_glBindVertexArray(vao);
    g_glBindBuffer(GL_ARRAY_BUFFER, vbo);
    g_glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)bytes, verts.data, dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);

    /* layout(location = 0) in vec3 pos; layout(location = 1) in vec4 col; */
    g_glEnableVertexAttribArray(0);
    g_glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, (GLsizei)sizeof(w32_d3d_vertex), (const void *)0);
    g_glEnableVertexAttribArray(1);
    g_glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, (GLsizei)sizeof(w32_d3d_vertex), (const void *)12);
    g_glBindVertexArray(0);

    w32_gl_mesh *m = &g_gl_meshes[slot];
    memset(m, 0, sizeof(*m));
    m->used = 1;
    m->vao = vao;
    m->vbo = vbo;
    m->ebo = 0;
    m->byte_size = bytes;
    m->dynamic = dynamic ? 1 : 0;
    m->is_textured = 0;
    return (uint32_t)(slot + 1);
}

void w32_gl_update_vertex_buffer(uint32_t mesh_id, w32_d3d_vertex_slice verts) {
    w32_gl_mesh *m = get_mesh(mesh_id);
    if (!m || m->is_textured || !verts.data || verts.len <= 0) return;
    size_t bytes = (size_t)verts.len * sizeof(w32_d3d_vertex);
    g_glBindBuffer(GL_ARRAY_BUFFER, m->vbo);
    if (bytes <= m->byte_size) {
        g_glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)bytes, verts.data);
    } else {
        g_glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)bytes, verts.data, m->dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
        m->byte_size = bytes;
    }
}

void w32_gl_destroy_vertex_buffer(uint32_t mesh_id) {
    w32_gl_mesh *m = get_mesh(mesh_id);
    if (!m) return;
    if (m->ebo && g_glDeleteBuffers) g_glDeleteBuffers(1, &m->ebo);
    if (m->vbo && g_glDeleteBuffers) g_glDeleteBuffers(1, &m->vbo);
    if (m->vao && g_glDeleteVertexArrays) g_glDeleteVertexArrays(1, &m->vao);
    memset(m, 0, sizeof(*m));
}

void w32_gl_bind_vertex_buffer(uint32_t mesh_id) {
    w32_gl_mesh *m = get_mesh(mesh_id);
    if (!m || !g_glBindVertexArray) return;
    g_glBindVertexArray(m->vao);
}

void w32_gl_draw_arrays(uint32_t mode, int32_t first, int32_t count) {
    glDrawArrays((GLenum)mode, (GLint)first, (GLsizei)count);
}

/* Reads back one RGBA8 pixel (0xAABBGGRR) using top-left coordinates (matching D3D11). */
uint8_t w32_gl_readback_pixel(uint32_t ctx_id, int32_t x, int32_t y, uint32_t *out_rgba) {
    w32_gl_ctx *c = get_ctx(ctx_id);
    if (!c || !out_rgba) return 0;
    wglMakeCurrent(c->hdc, c->hglrc);
    RECT rc;
    if (!GetClientRect(c->hwnd, &rc)) return 0;
    int32_t h = (int32_t)(rc.bottom - rc.top);
    int32_t gl_y = h - 1 - y;
    if (x < 0 || gl_y < 0 || x >= (rc.right - rc.left) || gl_y >= h) return 0;

    glFinish();
    /* Drain any pre-existing GL errors so the check below reflects glReadPixels. */
    while (glGetError() != GL_NO_ERROR) {}
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    uint32_t px = 0;
    glReadPixels((GLint)x, (GLint)gl_y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &px);
    *out_rgba = px;
    return glGetError() == GL_NO_ERROR ? 1 : 0;
}

/* ---- OpenGL 3.3 Textures & Indexed Meshes -------------------------------- */

void w32_gl_uniform_1i(uint32_t prog, w32_bytes name, int32_t v0) {
    GLint loc = get_uniform_loc(prog, name);
    if (loc >= 0 && g_glUniform1i) g_glUniform1i(loc, (GLint)v0);
}

uint32_t w32_gl_create_indexed_mesh(w32_tex_vertex_slice verts, w32_u32_slice indices, uint8_t dynamic) {
    if (!g_gl_funcs_loaded || !verts.data || verts.len <= 0 || !indices.data || indices.len <= 0) return 0;

    int slot = -1;
    for (int i = 0; i < W32_MAX_GL_MESHES; i++) {
        if (!g_gl_meshes[i].used) { slot = i; break; }
    }
    if (slot < 0) return 0;

    GLuint vao = 0, vbo = 0, ebo = 0;
    g_glGenVertexArrays(1, &vao);
    g_glGenBuffers(1, &vbo);
    g_glGenBuffers(1, &ebo);
    if (!vao || !vbo || !ebo) {
        if (ebo) g_glDeleteBuffers(1, &ebo);
        if (vbo) g_glDeleteBuffers(1, &vbo);
        if (vao) g_glDeleteVertexArrays(1, &vao);
        return 0;
    }

    size_t vbytes = (size_t)verts.len * sizeof(w32_tex_vertex);
    size_t ibytes = (size_t)indices.len * sizeof(uint32_t);
    g_glBindVertexArray(vao);
    g_glBindBuffer(GL_ARRAY_BUFFER, vbo);
    g_glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)vbytes, verts.data, dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
    g_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    g_glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)ibytes, indices.data, GL_STATIC_DRAW);

    /* layout(0) vec3 pos; layout(1) vec2 uv; layout(2) vec4 col; */
    g_glEnableVertexAttribArray(0);
    g_glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, (GLsizei)sizeof(w32_tex_vertex), (const void *)0);
    g_glEnableVertexAttribArray(1);
    g_glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, (GLsizei)sizeof(w32_tex_vertex), (const void *)12);
    g_glEnableVertexAttribArray(2);
    g_glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, (GLsizei)sizeof(w32_tex_vertex), (const void *)20);
    g_glBindVertexArray(0);

    w32_gl_mesh *m = &g_gl_meshes[slot];
    memset(m, 0, sizeof(*m));
    m->used = 1;
    m->vao = vao;
    m->vbo = vbo;
    m->ebo = ebo;
    m->byte_size = vbytes;
    m->dynamic = dynamic ? 1 : 0;
    m->is_textured = 1;
    return (uint32_t)(slot + 1);
}

void w32_gl_draw_indexed(uint32_t mode, int32_t index_count) {
    glDrawElements((GLenum)mode, (GLsizei)index_count, GL_UNSIGNED_INT, (const void *)0);
}

#define W32_MAX_GL_TEXTURES 64

typedef struct {
    int used;
    GLuint tex;
    int32_t width;
    int32_t height;
} w32_gl_tex;

static w32_gl_tex g_gl_texs[W32_MAX_GL_TEXTURES];

static w32_gl_tex *get_gl_tex(uint32_t id) {
    if (id == 0 || id > W32_MAX_GL_TEXTURES) return NULL;
    return g_gl_texs[id - 1].used ? &g_gl_texs[id - 1] : NULL;
}

uint32_t w32_gl_create_texture_rgba(int32_t w, int32_t h, w32_u32_slice pixels_rgba) {
    if (w <= 0 || h <= 0 || !pixels_rgba.data || pixels_rgba.len < (int64_t)w * (int64_t)h) return 0;
    int slot = -1;
    for (int i = 0; i < W32_MAX_GL_TEXTURES; i++) {
        if (!g_gl_texs[i].used) { slot = i; break; }
    }
    if (slot < 0) return 0;

    GLuint tex = 0;
    glGenTextures(1, &tex);
    if (!tex) return 0;
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, (GLsizei)w, (GLsizei)h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels_rgba.data);
    glBindTexture(GL_TEXTURE_2D, 0);

    w32_gl_tex *t = &g_gl_texs[slot];
    t->used = 1;
    t->tex = tex;
    t->width = w;
    t->height = h;
    return (uint32_t)(slot + 1);
}

uint8_t w32_gl_update_texture_rgba(uint32_t tex_id, w32_u32_slice pixels_rgba) {
    w32_gl_tex *t = get_gl_tex(tex_id);
    if (!t || !pixels_rgba.data || pixels_rgba.len < (int64_t)t->width * (int64_t)t->height) return 0;
    glBindTexture(GL_TEXTURE_2D, t->tex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, (GLsizei)t->width, (GLsizei)t->height, GL_RGBA, GL_UNSIGNED_BYTE, pixels_rgba.data);
    glBindTexture(GL_TEXTURE_2D, 0);
    return 1;
}

void w32_gl_bind_texture(uint32_t slot_idx, uint32_t tex_id) {
    w32_gl_tex *t = get_gl_tex(tex_id);
    if (!t) return;
    if (g_glActiveTexture) g_glActiveTexture((GLenum)(GL_TEXTURE0 + slot_idx));
    glBindTexture(GL_TEXTURE_2D, t->tex);
}

void w32_gl_destroy_texture(uint32_t tex_id) {
    w32_gl_tex *t = get_gl_tex(tex_id);
    if (!t) return;
    if (t->tex) glDeleteTextures(1, &t->tex);
    memset(t, 0, sizeof(*t));
}
