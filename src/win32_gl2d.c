#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "win32_internal.h"
#include <GL/gl.h>

/* ---- OpenGL 3.3 Core & WGL constants (no static opengl32 link) ----------- */

#ifndef GL_R8
#define GL_R8                          0x8229
#endif
#ifndef GL_RED
#define GL_RED                         0x1903
#endif
#ifndef GL_TEXTURE0
#define GL_TEXTURE0                    0x84C0
#endif
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE               0x812F
#endif
#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER                0x8892
#endif
#ifndef GL_DYNAMIC_DRAW
#define GL_DYNAMIC_DRAW                0x88E8
#endif
#ifndef GL_FRAGMENT_SHADER
#define GL_FRAGMENT_SHADER             0x8B30
#endif
#ifndef GL_VERTEX_SHADER
#define GL_VERTEX_SHADER               0x8B31
#endif
#ifndef GL_COMPILE_STATUS
#define GL_COMPILE_STATUS              0x8B81
#endif
#ifndef GL_LINK_STATUS
#define GL_LINK_STATUS                 0x8B82
#endif

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

typedef ptrdiff_t GLsizeiptr;
typedef ptrdiff_t GLintptr;
typedef char GLchar;

/* ---- Lazy opengl32.dll + OpenGL 3.3 Core function pointers --------------- */

typedef PROC  (WINAPI *pfn_gl2d_wglGetProcAddress)(LPCSTR);
typedef HGLRC (WINAPI *pfn_gl2d_wglCreateContext)(HDC);
typedef BOOL  (WINAPI *pfn_gl2d_wglMakeCurrent)(HDC, HGLRC);
typedef BOOL  (WINAPI *pfn_gl2d_wglDeleteContext)(HGLRC);
typedef HGLRC (WINAPI *pfn_gl2d_wglGetCurrentContext)(void);

typedef void   (APIENTRY *pfn_gl2d_Enable)(GLenum);
typedef void   (APIENTRY *pfn_gl2d_Disable)(GLenum);
typedef void   (APIENTRY *pfn_gl2d_BlendFunc)(GLenum, GLenum);
typedef void   (APIENTRY *pfn_gl2d_Scissor)(GLint, GLint, GLsizei, GLsizei);
typedef void   (APIENTRY *pfn_gl2d_Viewport)(GLint, GLint, GLsizei, GLsizei);
typedef void   (APIENTRY *pfn_gl2d_ClearColor)(GLfloat, GLfloat, GLfloat, GLfloat);
typedef void   (APIENTRY *pfn_gl2d_Clear)(GLbitfield);
typedef void   (APIENTRY *pfn_gl2d_DrawArrays)(GLenum, GLint, GLsizei);
typedef void   (APIENTRY *pfn_gl2d_GenTextures)(GLsizei, GLuint *);
typedef void   (APIENTRY *pfn_gl2d_BindTexture)(GLenum, GLuint);
typedef void   (APIENTRY *pfn_gl2d_TexParameteri)(GLenum, GLenum, GLint);
typedef void   (APIENTRY *pfn_gl2d_PixelStorei)(GLenum, GLint);
typedef void   (APIENTRY *pfn_gl2d_TexImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *);
typedef void   (APIENTRY *pfn_gl2d_TexSubImage2D)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void *);
typedef void   (APIENTRY *pfn_gl2d_DeleteTextures)(GLsizei, const GLuint *);
typedef GLenum (APIENTRY *pfn_gl2d_GetError)(void);
typedef void   (APIENTRY *pfn_gl2d_Finish)(void);
typedef void   (APIENTRY *pfn_gl2d_ReadPixels)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void *);

typedef BOOL  (WINAPI *pfn_gl2d_wglChoosePixelFormatARB)(HDC, const int *, const FLOAT *, UINT, int *, UINT *);
typedef HGLRC (WINAPI *pfn_gl2d_wglCreateContextAttribsARB)(HDC, HGLRC, const int *);
typedef BOOL  (WINAPI *pfn_gl2d_wglSwapIntervalEXT)(int);

typedef GLuint (APIENTRY *pfn_gl2d_CreateShader)(GLenum);
typedef void   (APIENTRY *pfn_gl2d_ShaderSource)(GLuint, GLsizei, const GLchar *const *, const GLint *);
typedef void   (APIENTRY *pfn_gl2d_CompileShader)(GLuint);
typedef void   (APIENTRY *pfn_gl2d_GetShaderiv)(GLuint, GLenum, GLint *);
typedef void   (APIENTRY *pfn_gl2d_DeleteShader)(GLuint);
typedef GLuint (APIENTRY *pfn_gl2d_CreateProgram)(void);
typedef void   (APIENTRY *pfn_gl2d_AttachShader)(GLuint, GLuint);
typedef void   (APIENTRY *pfn_gl2d_LinkProgram)(GLuint);
typedef void   (APIENTRY *pfn_gl2d_GetProgramiv)(GLuint, GLenum, GLint *);
typedef void   (APIENTRY *pfn_gl2d_DeleteProgram)(GLuint);
typedef void   (APIENTRY *pfn_gl2d_UseProgram)(GLuint);
typedef GLint  (APIENTRY *pfn_gl2d_GetUniformLocation)(GLuint, const GLchar *);
typedef void   (APIENTRY *pfn_gl2d_Uniform1i)(GLint, GLint);
typedef void   (APIENTRY *pfn_gl2d_Uniform2f)(GLint, GLfloat, GLfloat);
typedef void   (APIENTRY *pfn_gl2d_ActiveTexture)(GLenum);
typedef void   (APIENTRY *pfn_gl2d_GenVertexArrays)(GLsizei, GLuint *);
typedef void   (APIENTRY *pfn_gl2d_BindVertexArray)(GLuint);
typedef void   (APIENTRY *pfn_gl2d_DeleteVertexArrays)(GLsizei, const GLuint *);
typedef void   (APIENTRY *pfn_gl2d_GenBuffers)(GLsizei, GLuint *);
typedef void   (APIENTRY *pfn_gl2d_BindBuffer)(GLenum, GLuint);
typedef void   (APIENTRY *pfn_gl2d_BufferData)(GLenum, GLsizeiptr, const void *, GLenum);
typedef void   (APIENTRY *pfn_gl2d_BufferSubData)(GLenum, GLintptr, GLsizeiptr, const void *);
typedef void   (APIENTRY *pfn_gl2d_DeleteBuffers)(GLsizei, const GLuint *);
typedef void   (APIENTRY *pfn_gl2d_EnableVertexAttribArray)(GLuint);
typedef void   (APIENTRY *pfn_gl2d_VertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void *);

static struct {
    int tried_ogl;
    int loaded_ogl;
    int wgl_bootstrapped;
    int loaded_core;
    HMODULE mod;

    pfn_gl2d_wglGetProcAddress    wglGetProcAddress;
    pfn_gl2d_wglCreateContext     wglCreateContext;
    pfn_gl2d_wglMakeCurrent       wglMakeCurrent;
    pfn_gl2d_wglDeleteContext     wglDeleteContext;
    pfn_gl2d_wglGetCurrentContext wglGetCurrentContext;

    pfn_gl2d_Enable               glEnable;
    pfn_gl2d_Disable              glDisable;
    pfn_gl2d_BlendFunc            glBlendFunc;
    pfn_gl2d_Scissor              glScissor;
    pfn_gl2d_Viewport             glViewport;
    pfn_gl2d_ClearColor           glClearColor;
    pfn_gl2d_Clear                glClear;
    pfn_gl2d_DrawArrays           glDrawArrays;
    pfn_gl2d_GenTextures          glGenTextures;
    pfn_gl2d_BindTexture          glBindTexture;
    pfn_gl2d_TexParameteri        glTexParameteri;
    pfn_gl2d_PixelStorei          glPixelStorei;
    pfn_gl2d_TexImage2D           glTexImage2D;
    pfn_gl2d_TexSubImage2D        glTexSubImage2D;
    pfn_gl2d_DeleteTextures       glDeleteTextures;
    pfn_gl2d_GetError             glGetError;
    pfn_gl2d_Finish               glFinish;
    pfn_gl2d_ReadPixels           glReadPixels;

    pfn_gl2d_wglChoosePixelFormatARB    wglChoosePixelFormatARB;
    pfn_gl2d_wglCreateContextAttribsARB wglCreateContextAttribsARB;
    pfn_gl2d_wglSwapIntervalEXT         wglSwapIntervalEXT;

    pfn_gl2d_CreateShader             glCreateShader;
    pfn_gl2d_ShaderSource             glShaderSource;
    pfn_gl2d_CompileShader            glCompileShader;
    pfn_gl2d_GetShaderiv              glGetShaderiv;
    pfn_gl2d_DeleteShader             glDeleteShader;
    pfn_gl2d_CreateProgram            glCreateProgram;
    pfn_gl2d_AttachShader             glAttachShader;
    pfn_gl2d_LinkProgram              glLinkProgram;
    pfn_gl2d_GetProgramiv             glGetProgramiv;
    pfn_gl2d_DeleteProgram            glDeleteProgram;
    pfn_gl2d_UseProgram               glUseProgram;
    pfn_gl2d_GetUniformLocation       glGetUniformLocation;
    pfn_gl2d_Uniform1i                glUniform1i;
    pfn_gl2d_Uniform2f                glUniform2f;
    pfn_gl2d_ActiveTexture            glActiveTexture;
    pfn_gl2d_GenVertexArrays          glGenVertexArrays;
    pfn_gl2d_BindVertexArray          glBindVertexArray;
    pfn_gl2d_DeleteVertexArrays       glDeleteVertexArrays;
    pfn_gl2d_GenBuffers               glGenBuffers;
    pfn_gl2d_BindBuffer               glBindBuffer;
    pfn_gl2d_BufferData               glBufferData;
    pfn_gl2d_BufferSubData            glBufferSubData;
    pfn_gl2d_DeleteBuffers            glDeleteBuffers;
    pfn_gl2d_EnableVertexAttribArray  glEnableVertexAttribArray;
    pfn_gl2d_VertexAttribPointer      glVertexAttribPointer;
} g_gl2d;

static int ensure_gl2d_ogl(void) {
    if (g_gl2d.loaded_ogl) return 1;
    if (g_gl2d.tried_ogl) return 0;
    g_gl2d.tried_ogl = 1;
    g_gl2d.mod = LoadLibraryW(L"opengl32.dll");
    if (!g_gl2d.mod) return 0;

    g_gl2d.wglGetProcAddress    = (pfn_gl2d_wglGetProcAddress)(uintptr_t)GetProcAddress(g_gl2d.mod, "wglGetProcAddress");
    g_gl2d.wglCreateContext     = (pfn_gl2d_wglCreateContext)(uintptr_t)GetProcAddress(g_gl2d.mod, "wglCreateContext");
    g_gl2d.wglMakeCurrent       = (pfn_gl2d_wglMakeCurrent)(uintptr_t)GetProcAddress(g_gl2d.mod, "wglMakeCurrent");
    g_gl2d.wglDeleteContext     = (pfn_gl2d_wglDeleteContext)(uintptr_t)GetProcAddress(g_gl2d.mod, "wglDeleteContext");
    g_gl2d.wglGetCurrentContext = (pfn_gl2d_wglGetCurrentContext)(uintptr_t)GetProcAddress(g_gl2d.mod, "wglGetCurrentContext");

    g_gl2d.glEnable         = (pfn_gl2d_Enable)(uintptr_t)GetProcAddress(g_gl2d.mod, "glEnable");
    g_gl2d.glDisable        = (pfn_gl2d_Disable)(uintptr_t)GetProcAddress(g_gl2d.mod, "glDisable");
    g_gl2d.glBlendFunc      = (pfn_gl2d_BlendFunc)(uintptr_t)GetProcAddress(g_gl2d.mod, "glBlendFunc");
    g_gl2d.glScissor        = (pfn_gl2d_Scissor)(uintptr_t)GetProcAddress(g_gl2d.mod, "glScissor");
    g_gl2d.glViewport       = (pfn_gl2d_Viewport)(uintptr_t)GetProcAddress(g_gl2d.mod, "glViewport");
    g_gl2d.glClearColor     = (pfn_gl2d_ClearColor)(uintptr_t)GetProcAddress(g_gl2d.mod, "glClearColor");
    g_gl2d.glClear          = (pfn_gl2d_Clear)(uintptr_t)GetProcAddress(g_gl2d.mod, "glClear");
    g_gl2d.glDrawArrays     = (pfn_gl2d_DrawArrays)(uintptr_t)GetProcAddress(g_gl2d.mod, "glDrawArrays");
    g_gl2d.glGenTextures    = (pfn_gl2d_GenTextures)(uintptr_t)GetProcAddress(g_gl2d.mod, "glGenTextures");
    g_gl2d.glBindTexture    = (pfn_gl2d_BindTexture)(uintptr_t)GetProcAddress(g_gl2d.mod, "glBindTexture");
    g_gl2d.glTexParameteri  = (pfn_gl2d_TexParameteri)(uintptr_t)GetProcAddress(g_gl2d.mod, "glTexParameteri");
    g_gl2d.glPixelStorei    = (pfn_gl2d_PixelStorei)(uintptr_t)GetProcAddress(g_gl2d.mod, "glPixelStorei");
    g_gl2d.glTexImage2D     = (pfn_gl2d_TexImage2D)(uintptr_t)GetProcAddress(g_gl2d.mod, "glTexImage2D");
    g_gl2d.glTexSubImage2D  = (pfn_gl2d_TexSubImage2D)(uintptr_t)GetProcAddress(g_gl2d.mod, "glTexSubImage2D");
    g_gl2d.glDeleteTextures = (pfn_gl2d_DeleteTextures)(uintptr_t)GetProcAddress(g_gl2d.mod, "glDeleteTextures");
    g_gl2d.glGetError       = (pfn_gl2d_GetError)(uintptr_t)GetProcAddress(g_gl2d.mod, "glGetError");
    g_gl2d.glFinish         = (pfn_gl2d_Finish)(uintptr_t)GetProcAddress(g_gl2d.mod, "glFinish");
    g_gl2d.glReadPixels     = (pfn_gl2d_ReadPixels)(uintptr_t)GetProcAddress(g_gl2d.mod, "glReadPixels");

    if (g_gl2d.wglGetProcAddress && g_gl2d.wglCreateContext && g_gl2d.wglMakeCurrent &&
        g_gl2d.wglDeleteContext && g_gl2d.wglGetCurrentContext &&
        g_gl2d.glEnable && g_gl2d.glDisable && g_gl2d.glBlendFunc &&
        g_gl2d.glScissor && g_gl2d.glViewport && g_gl2d.glClearColor && g_gl2d.glClear &&
        g_gl2d.glDrawArrays && g_gl2d.glGenTextures && g_gl2d.glBindTexture &&
        g_gl2d.glTexParameteri && g_gl2d.glPixelStorei && g_gl2d.glTexImage2D &&
        g_gl2d.glTexSubImage2D && g_gl2d.glDeleteTextures &&
        g_gl2d.glGetError && g_gl2d.glFinish && g_gl2d.glReadPixels) {
        g_gl2d.loaded_ogl = 1;
        return 1;
    }
    return 0;
}

static void *gl2d_proc(const char *name) {
    if (!g_gl2d.wglGetProcAddress) return NULL;
    void *p = (void *)(uintptr_t)g_gl2d.wglGetProcAddress(name);
    if (!p || p == (void *)1 || p == (void *)2 || p == (void *)3 || p == (void *)(intptr_t)-1) {
        if (g_gl2d.mod) p = (void *)(uintptr_t)GetProcAddress(g_gl2d.mod, name);
    }
    return p;
}

static void bootstrap_gl2d_wgl(void) {
    if (g_gl2d.wgl_bootstrapped) return;
    g_gl2d.wgl_bootstrapped = 1;

    WNDCLASSW wc;
    memset(&wc, 0, sizeof(wc));
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = L"Win32GooseGL2DDummy";
    RegisterClassW(&wc);

    HWND dummy = CreateWindowW(
        L"Win32GooseGL2DDummy", L"", WS_POPUP,
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
            HGLRC rc = g_gl2d.wglCreateContext(dc);
            if (rc) {
                if (g_gl2d.wglMakeCurrent(dc, rc)) {
                    g_gl2d.wglChoosePixelFormatARB =
                        (pfn_gl2d_wglChoosePixelFormatARB)(uintptr_t)gl2d_proc("wglChoosePixelFormatARB");
                    g_gl2d.wglCreateContextAttribsARB =
                        (pfn_gl2d_wglCreateContextAttribsARB)(uintptr_t)gl2d_proc("wglCreateContextAttribsARB");
                    g_gl2d.wglSwapIntervalEXT =
                        (pfn_gl2d_wglSwapIntervalEXT)(uintptr_t)gl2d_proc("wglSwapIntervalEXT");
                    g_gl2d.wglMakeCurrent(NULL, NULL);
                }
                g_gl2d.wglDeleteContext(rc);
            }
        }
        ReleaseDC(dummy, dc);
    }
    DestroyWindow(dummy);
}

static int ensure_gl2d_core(void) {
    if (g_gl2d.loaded_core) return 1;
    g_gl2d.glCreateShader            = (pfn_gl2d_CreateShader)(uintptr_t)gl2d_proc("glCreateShader");
    g_gl2d.glShaderSource            = (pfn_gl2d_ShaderSource)(uintptr_t)gl2d_proc("glShaderSource");
    g_gl2d.glCompileShader           = (pfn_gl2d_CompileShader)(uintptr_t)gl2d_proc("glCompileShader");
    g_gl2d.glGetShaderiv             = (pfn_gl2d_GetShaderiv)(uintptr_t)gl2d_proc("glGetShaderiv");
    g_gl2d.glDeleteShader            = (pfn_gl2d_DeleteShader)(uintptr_t)gl2d_proc("glDeleteShader");
    g_gl2d.glCreateProgram           = (pfn_gl2d_CreateProgram)(uintptr_t)gl2d_proc("glCreateProgram");
    g_gl2d.glAttachShader            = (pfn_gl2d_AttachShader)(uintptr_t)gl2d_proc("glAttachShader");
    g_gl2d.glLinkProgram             = (pfn_gl2d_LinkProgram)(uintptr_t)gl2d_proc("glLinkProgram");
    g_gl2d.glGetProgramiv            = (pfn_gl2d_GetProgramiv)(uintptr_t)gl2d_proc("glGetProgramiv");
    g_gl2d.glDeleteProgram           = (pfn_gl2d_DeleteProgram)(uintptr_t)gl2d_proc("glDeleteProgram");
    g_gl2d.glUseProgram              = (pfn_gl2d_UseProgram)(uintptr_t)gl2d_proc("glUseProgram");
    g_gl2d.glGetUniformLocation      = (pfn_gl2d_GetUniformLocation)(uintptr_t)gl2d_proc("glGetUniformLocation");
    g_gl2d.glUniform1i               = (pfn_gl2d_Uniform1i)(uintptr_t)gl2d_proc("glUniform1i");
    g_gl2d.glUniform2f               = (pfn_gl2d_Uniform2f)(uintptr_t)gl2d_proc("glUniform2f");
    g_gl2d.glActiveTexture           = (pfn_gl2d_ActiveTexture)(uintptr_t)gl2d_proc("glActiveTexture");
    g_gl2d.glGenVertexArrays         = (pfn_gl2d_GenVertexArrays)(uintptr_t)gl2d_proc("glGenVertexArrays");
    g_gl2d.glBindVertexArray         = (pfn_gl2d_BindVertexArray)(uintptr_t)gl2d_proc("glBindVertexArray");
    g_gl2d.glDeleteVertexArrays      = (pfn_gl2d_DeleteVertexArrays)(uintptr_t)gl2d_proc("glDeleteVertexArrays");
    g_gl2d.glGenBuffers              = (pfn_gl2d_GenBuffers)(uintptr_t)gl2d_proc("glGenBuffers");
    g_gl2d.glBindBuffer              = (pfn_gl2d_BindBuffer)(uintptr_t)gl2d_proc("glBindBuffer");
    g_gl2d.glBufferData              = (pfn_gl2d_BufferData)(uintptr_t)gl2d_proc("glBufferData");
    g_gl2d.glBufferSubData           = (pfn_gl2d_BufferSubData)(uintptr_t)gl2d_proc("glBufferSubData");
    g_gl2d.glDeleteBuffers           = (pfn_gl2d_DeleteBuffers)(uintptr_t)gl2d_proc("glDeleteBuffers");
    g_gl2d.glEnableVertexAttribArray = (pfn_gl2d_EnableVertexAttribArray)(uintptr_t)gl2d_proc("glEnableVertexAttribArray");
    g_gl2d.glVertexAttribPointer     = (pfn_gl2d_VertexAttribPointer)(uintptr_t)gl2d_proc("glVertexAttribPointer");

    if (g_gl2d.glCreateShader && g_gl2d.glShaderSource && g_gl2d.glCompileShader &&
        g_gl2d.glGetShaderiv && g_gl2d.glDeleteShader && g_gl2d.glCreateProgram &&
        g_gl2d.glAttachShader && g_gl2d.glLinkProgram && g_gl2d.glGetProgramiv &&
        g_gl2d.glDeleteProgram && g_gl2d.glUseProgram && g_gl2d.glGetUniformLocation &&
        g_gl2d.glUniform1i && g_gl2d.glUniform2f && g_gl2d.glActiveTexture &&
        g_gl2d.glGenVertexArrays && g_gl2d.glBindVertexArray && g_gl2d.glDeleteVertexArrays &&
        g_gl2d.glGenBuffers && g_gl2d.glBindBuffer && g_gl2d.glBufferData &&
        g_gl2d.glBufferSubData && g_gl2d.glDeleteBuffers &&
        g_gl2d.glEnableVertexAttribArray && g_gl2d.glVertexAttribPointer) {
        g_gl2d.loaded_core = 1;
        return 1;
    }
    return 0;
}

/* ---- Shader & Vertex Batch Types ----------------------------------------- */

#define GL2D_MAX_TARGETS     8
#define GL2D_MAX_FONTS       64
#define GL2D_MAX_QUADS       512
#define GL2D_MAX_VERTS       (GL2D_MAX_QUADS * 6)
#define GL2D_ATLAS_W         1024
#define GL2D_ATLAS_H         1024
#define GL2D_SCRATCH_W       1024
#define GL2D_SCRATCH_H       128
#define GL2D_TEXT_CACHE_CAP  512
#define GL2D_CLIP_STACK_CAP  16

typedef struct {
    float px, py;     /* 0: screen pixels (top-left origin) */
    float lx, ly;     /* 1: local coords or atlas UV */
    float hx, hy;     /* 2: half extents */
    float radius;     /* 3: corner radius or half line width */
    float stroke;     /* 4: <= 0 fill, > 0 stroke width */
    float mode;       /* 5: 0 rect/rrect, 1 ellipse, 2 line, 3 text */
    float r, g, b, a; /* 6: RGBA */
} gl2d_vert;

typedef struct {
    int occupied;
    uint64_t h1;
    uint64_t h2;
    uint32_t font_id;
    int32_t text_len;
    int32_t tw;
    int32_t th;
    float u0, v0, u1, v1;
} gl2d_text_entry;

typedef struct {
    int used;
    HWND hwnd;
    HDC hdc;
    HGLRC hglrc;
    int32_t fb_w;
    int32_t fb_h;

    GLuint vao;
    GLuint vbo;
    GLuint prog;
    GLuint atlas_tex;
    GLint u_viewport;
    GLint u_atlas;

    gl2d_vert *verts;
    int vert_count;

    RECT clip_stack[GL2D_CLIP_STACK_CAP];
    int clip_depth;

    /* Single reusable scratch GDI DIB + staging bytes for text glyph rasterization */
    HDC scratch_dc;
    HBITMAP scratch_bmp;
    HGDIOBJ scratch_old_bmp;
    uint32_t *scratch_bits;
    uint8_t *atlas_stage;

    /* GPU VRAM GL_R8 texture atlas shelf packer & hash cache */
    int32_t shelf_x;
    int32_t shelf_y;
    int32_t shelf_h;
    gl2d_text_entry text_cache[GL2D_TEXT_CACHE_CAP];
} w32_gl2d_target;

typedef struct {
    int used;
    uint32_t target_id;
    HFONT hfont;
} w32_gl2d_font;

static w32_gl2d_target g_targets[GL2D_MAX_TARGETS];
static w32_gl2d_font   g_fonts[GL2D_MAX_FONTS];

static w32_gl2d_target *get_target(uint32_t id) {
    if (id == 0 || id > GL2D_MAX_TARGETS) return NULL;
    return g_targets[id - 1].used ? &g_targets[id - 1] : NULL;
}

static w32_gl2d_font *get_font(uint32_t id) {
    if (id == 0 || id > GL2D_MAX_FONTS) return NULL;
    return g_fonts[id - 1].used ? &g_fonts[id - 1] : NULL;
}

static const char *const k_gl2d_vs =
    "#version 330 core\n"
    "layout(location=0) in vec2 aPos;\n"
    "layout(location=1) in vec2 aLocal;\n"
    "layout(location=2) in vec2 aHalf;\n"
    "layout(location=3) in float aRadius;\n"
    "layout(location=4) in float aStroke;\n"
    "layout(location=5) in float aMode;\n"
    "layout(location=6) in vec4 aColor;\n"
    "uniform vec2 uViewport;\n"
    "out vec2 vLocal;\n"
    "out vec2 vHalf;\n"
    "out float vRadius;\n"
    "out float vStroke;\n"
    "out float vMode;\n"
    "out vec4 vColor;\n"
    "void main() {\n"
    "    vec2 ndc = (aPos / max(uViewport, vec2(1.0))) * 2.0 - 1.0;\n"
    "    gl_Position = vec4(ndc.x, -ndc.y, 0.0, 1.0);\n"
    "    vLocal = aLocal;\n"
    "    vHalf = aHalf;\n"
    "    vRadius = aRadius;\n"
    "    vStroke = aStroke;\n"
    "    vMode = aMode;\n"
    "    vColor = aColor;\n"
    "}\n";

static const char *const k_gl2d_fs =
    "#version 330 core\n"
    "uniform sampler2D uAtlas;\n"
    "in vec2 vLocal;\n"
    "in vec2 vHalf;\n"
    "in float vRadius;\n"
    "in float vStroke;\n"
    "in float vMode;\n"
    "in vec4 vColor;\n"
    "out vec4 FragColor;\n"
    "void main() {\n"
    "    float alpha = 1.0;\n"
    "    if (vMode < 0.5) {\n"
    "        if (vRadius <= 0.001 && vStroke <= 0.001) {\n"
    "            alpha = 1.0;\n"
    "        } else {\n"
    "            float rad = min(vRadius, min(vHalf.x, vHalf.y));\n"
    "            vec2 q = abs(vLocal) - (vHalf - vec2(rad));\n"
    "            float d = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - rad;\n"
    "            if (vStroke > 0.0) {\n"
    "                alpha = clamp(0.5 - (abs(d) - vStroke * 0.5), 0.0, 1.0);\n"
    "            } else {\n"
    "                alpha = clamp(0.5 - d, 0.0, 1.0);\n"
    "            }\n"
    "        }\n"
    "    } else if (vMode < 1.5) {\n"
    "        vec2 h = max(vHalf, vec2(0.001));\n"
    "        vec2 p = vLocal / h;\n"
    "        float k0 = length(p);\n"
    "        float k1 = length(p / h);\n"
    "        float d = (k0 > 0.0001) ? (k0 * (k0 - 1.0) / max(k1, 0.0001)) : -min(h.x, h.y);\n"
    "        if (vStroke > 0.0) {\n"
    "            alpha = clamp(0.5 - (abs(d) - vStroke * 0.5), 0.0, 1.0);\n"
    "        } else {\n"
    "            alpha = clamp(0.5 - d, 0.0, 1.0);\n"
    "        }\n"
    "    } else if (vMode < 2.5) {\n"
    "        float dx = max(abs(vLocal.x) - vHalf.x, 0.0);\n"
    "        float d = length(vec2(dx, vLocal.y)) - vRadius;\n"
    "        alpha = clamp(0.5 - d, 0.0, 1.0);\n"
    "    } else {\n"
    "        alpha = texture(uAtlas, vLocal).r;\n"
    "    }\n"
    "    if (alpha <= 0.001) discard;\n"
    "    FragColor = vec4(vColor.rgb, vColor.a * alpha);\n"
    "}\n";

static GLuint compile_shader_str(GLenum kind, const char *src) {
    GLuint sh = g_gl2d.glCreateShader(kind);
    if (!sh) return 0;
    GLint len = (GLint)strlen(src);
    g_gl2d.glShaderSource(sh, 1, &src, &len);
    g_gl2d.glCompileShader(sh);
    GLint ok = 0;
    g_gl2d.glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        g_gl2d.glDeleteShader(sh);
        return 0;
    }
    return sh;
}

static void gl2d_make_current(w32_gl2d_target *t) {
    if (g_gl2d.loaded_ogl && g_gl2d.wglGetCurrentContext() != t->hglrc) {
        g_gl2d.wglMakeCurrent(t->hdc, t->hglrc);
    }
}

static void gl2d_flush(w32_gl2d_target *t) {
    if (!t || t->vert_count <= 0) return;
    gl2d_make_current(t);

    g_gl2d.glUseProgram(t->prog);
    g_gl2d.glUniform2f(t->u_viewport, (float)t->fb_w, (float)t->fb_h);
    g_gl2d.glActiveTexture(GL_TEXTURE0);
    g_gl2d.glBindTexture(GL_TEXTURE_2D, t->atlas_tex);
    g_gl2d.glUniform1i(t->u_atlas, 0);

    g_gl2d.glBindVertexArray(t->vao);
    g_gl2d.glBindBuffer(GL_ARRAY_BUFFER, t->vbo);
    g_gl2d.glBufferSubData(
        GL_ARRAY_BUFFER, 0, (GLsizeiptr)((size_t)t->vert_count * sizeof(gl2d_vert)), t->verts
    );
    g_gl2d.glDrawArrays(GL_TRIANGLES, 0, (GLsizei)t->vert_count);
    t->vert_count = 0;
}

static void gl2d_push_quad(w32_gl2d_target *t, const gl2d_vert q[4]) {
    if (t->vert_count + 6 > GL2D_MAX_VERTS) {
        gl2d_flush(t);
    }
    gl2d_vert *dst = t->verts + t->vert_count;
    dst[0] = q[0];
    dst[1] = q[1];
    dst[2] = q[2];
    dst[3] = q[0];
    dst[4] = q[2];
    dst[5] = q[3];
    t->vert_count += 6;
}

static void gl2d_apply_top_scissor(w32_gl2d_target *t) {
    if (t->clip_depth <= 0) {
        g_gl2d.glDisable(GL_SCISSOR_TEST);
        return;
    }
    RECT rc = t->clip_stack[t->clip_depth - 1];
    int32_t w = (int32_t)(rc.right - rc.left);
    int32_t h = (int32_t)(rc.bottom - rc.top);
    if (w < 0) w = 0;
    if (h < 0) h = 0;
    int32_t gl_y = t->fb_h - (int32_t)rc.bottom;
    g_gl2d.glEnable(GL_SCISSOR_TEST);
    g_gl2d.glScissor((GLint)rc.left, (GLint)gl_y, (GLsizei)w, (GLsizei)h);
}

static void gl2d_emit_box_primitive(
    w32_gl2d_target *t,
    float x, float y, float w, float h,
    float radius, float stroke, float mode, w32_colorf c
) {
    if (!t || w <= 0.0f || h <= 0.0f || c.a <= 0.0f) return;
    float hx = w * 0.5f;
    float hy = h * 0.5f;
    float pad = 0.0f;
    if (stroke > 0.0f) {
        pad = ceilf(stroke * 0.5f + 1.5f);
    } else if (radius > 0.001f || mode > 0.5f) {
        pad = 1.5f;
    }

    float px0 = x - pad;
    float py0 = y - pad;
    float px1 = x + w + pad;
    float py1 = y + h + pad;
    float lx = hx + pad;
    float ly = hy + pad;

    gl2d_vert q[4];
    q[0] = (gl2d_vert){ px0, py0, -lx, -ly, hx, hy, radius, stroke, mode, c.r, c.g, c.b, c.a };
    q[1] = (gl2d_vert){ px1, py0,  lx, -ly, hx, hy, radius, stroke, mode, c.r, c.g, c.b, c.a };
    q[2] = (gl2d_vert){ px1, py1,  lx,  ly, hx, hy, radius, stroke, mode, c.r, c.g, c.b, c.a };
    q[3] = (gl2d_vert){ px0, py1, -lx,  ly, hx, hy, radius, stroke, mode, c.r, c.g, c.b, c.a };
    gl2d_push_quad(t, q);
}

static int gl2d_create_scratch_dib(w32_gl2d_target *t) {
    HDC screen = GetDC(NULL);
    t->scratch_dc = CreateCompatibleDC(screen);
    if (screen) ReleaseDC(NULL, screen);
    if (!t->scratch_dc) return 0;

    BITMAPINFO bmi;
    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = GL2D_SCRATCH_W;
    bmi.bmiHeader.biHeight = -GL2D_SCRATCH_H; /* top-down */
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void *bits = NULL;
    t->scratch_bmp = CreateDIBSection(t->scratch_dc, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!t->scratch_bmp || !bits) {
        DeleteDC(t->scratch_dc);
        t->scratch_dc = NULL;
        return 0;
    }
    t->scratch_bits = (uint32_t *)bits;
    t->scratch_old_bmp = SelectObject(t->scratch_dc, t->scratch_bmp);
    t->atlas_stage = (uint8_t *)malloc((size_t)GL2D_SCRATCH_W * (size_t)GL2D_SCRATCH_H);
    if (!t->atlas_stage) return 0;
    return 1;
}

/* ---- Public Lifecycle & Rendering API ------------------------------------ */

uint32_t w32_gl2d_create(uint64_t hwnd_u64, uint8_t vsync) {
    HWND hwnd = (HWND)(uintptr_t)hwnd_u64;
    if (!hwnd || !ensure_gl2d_ogl()) return 0;

    int slot = -1;
    for (int i = 0; i < GL2D_MAX_TARGETS; i++) {
        if (!g_targets[i].used) { slot = i; break; }
    }
    if (slot < 0) return 0;

    bootstrap_gl2d_wgl();

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
    if (g_gl2d.wglChoosePixelFormatARB) {
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
        if (!g_gl2d.wglChoosePixelFormatARB(hdc, attribs, NULL, 1, &pf, &num_formats) || num_formats == 0) {
            pf = 0;
        }
    }
    if (!pf) pf = ChoosePixelFormat(hdc, &pfd);
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
    if (g_gl2d.wglCreateContextAttribsARB) {
        const int ctx_attribs[] = {
            WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
            WGL_CONTEXT_MINOR_VERSION_ARB, 3,
            WGL_CONTEXT_PROFILE_MASK_ARB,  WGL_CONTEXT_CORE_PROFILE_BIT,
            0
        };
        hglrc = g_gl2d.wglCreateContextAttribsARB(hdc, NULL, ctx_attribs);
    }
    if (!hglrc) hglrc = g_gl2d.wglCreateContext(hdc);
    if (!hglrc) {
        ReleaseDC(hwnd, hdc);
        return 0;
    }

    if (!g_gl2d.wglMakeCurrent(hdc, hglrc)) {
        g_gl2d.wglDeleteContext(hglrc);
        ReleaseDC(hwnd, hdc);
        return 0;
    }

    if (!ensure_gl2d_core()) {
        g_gl2d.wglMakeCurrent(NULL, NULL);
        g_gl2d.wglDeleteContext(hglrc);
        ReleaseDC(hwnd, hdc);
        return 0;
    }
    if (g_gl2d.wglSwapIntervalEXT) {
        g_gl2d.wglSwapIntervalEXT(vsync ? 1 : 0);
    }

    GLuint vs = compile_shader_str(GL_VERTEX_SHADER, k_gl2d_vs);
    GLuint fs = vs ? compile_shader_str(GL_FRAGMENT_SHADER, k_gl2d_fs) : 0;
    GLuint prog = 0;
    if (vs && fs) {
        prog = g_gl2d.glCreateProgram();
        if (prog) {
            g_gl2d.glAttachShader(prog, vs);
            g_gl2d.glAttachShader(prog, fs);
            g_gl2d.glLinkProgram(prog);
            GLint ok = 0;
            g_gl2d.glGetProgramiv(prog, GL_LINK_STATUS, &ok);
            if (!ok) {
                g_gl2d.glDeleteProgram(prog);
                prog = 0;
            }
        }
    }
    if (vs) g_gl2d.glDeleteShader(vs);
    if (fs) g_gl2d.glDeleteShader(fs);
    if (!prog) {
        g_gl2d.wglMakeCurrent(NULL, NULL);
        g_gl2d.wglDeleteContext(hglrc);
        ReleaseDC(hwnd, hdc);
        return 0;
    }

    w32_gl2d_target *t = &g_targets[slot];
    memset(t, 0, sizeof(*t));
    t->hwnd = hwnd;
    t->hdc = hdc;
    t->hglrc = hglrc;
    t->prog = prog;
    t->u_viewport = g_gl2d.glGetUniformLocation(prog, "uViewport");
    t->u_atlas = g_gl2d.glGetUniformLocation(prog, "uAtlas");

    t->verts = (gl2d_vert *)malloc((size_t)GL2D_MAX_VERTS * sizeof(gl2d_vert));
    if (!t->verts || !gl2d_create_scratch_dib(t)) {
        free(t->verts);
        free(t->atlas_stage);
        if (t->scratch_dc) {
            if (t->scratch_old_bmp) SelectObject(t->scratch_dc, t->scratch_old_bmp);
            if (t->scratch_bmp) DeleteObject(t->scratch_bmp);
            DeleteDC(t->scratch_dc);
        }
        g_gl2d.glDeleteProgram(prog);
        g_gl2d.wglMakeCurrent(NULL, NULL);
        g_gl2d.wglDeleteContext(hglrc);
        ReleaseDC(hwnd, hdc);
        memset(t, 0, sizeof(*t));
        return 0;
    }

    g_gl2d.glGenVertexArrays(1, &t->vao);
    g_gl2d.glGenBuffers(1, &t->vbo);
    g_gl2d.glBindVertexArray(t->vao);
    g_gl2d.glBindBuffer(GL_ARRAY_BUFFER, t->vbo);
    g_gl2d.glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)((size_t)GL2D_MAX_VERTS * sizeof(gl2d_vert)), NULL, GL_DYNAMIC_DRAW);

    GLsizei stride = (GLsizei)sizeof(gl2d_vert);
    g_gl2d.glEnableVertexAttribArray(0);
    g_gl2d.glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (const void *)0);
    g_gl2d.glEnableVertexAttribArray(1);
    g_gl2d.glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (const void *)8);
    g_gl2d.glEnableVertexAttribArray(2);
    g_gl2d.glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (const void *)16);
    g_gl2d.glEnableVertexAttribArray(3);
    g_gl2d.glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride, (const void *)24);
    g_gl2d.glEnableVertexAttribArray(4);
    g_gl2d.glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, stride, (const void *)28);
    g_gl2d.glEnableVertexAttribArray(5);
    g_gl2d.glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, stride, (const void *)32);
    g_gl2d.glEnableVertexAttribArray(6);
    g_gl2d.glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, stride, (const void *)36);
    g_gl2d.glBindVertexArray(0);

    /* Allocate 1024x1024 GL_R8 glyph atlas cleanly in GPU VRAM (0 retained CPU copy). */
    g_gl2d.glGenTextures(1, &t->atlas_tex);
    g_gl2d.glBindTexture(GL_TEXTURE_2D, t->atlas_tex);
    g_gl2d.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    g_gl2d.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    g_gl2d.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    g_gl2d.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    g_gl2d.glTexImage2D(
        GL_TEXTURE_2D, 0, GL_R8, GL2D_ATLAS_W, GL2D_ATLAS_H, 0, GL_RED, GL_UNSIGNED_BYTE, NULL
    );
    g_gl2d.glBindTexture(GL_TEXTURE_2D, 0);

    g_gl2d.glEnable(GL_BLEND);
    g_gl2d.glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    RECT rc;
    GetClientRect(hwnd, &rc);
    t->fb_w = (int32_t)(rc.right > rc.left ? rc.right - rc.left : 1);
    t->fb_h = (int32_t)(rc.bottom > rc.top ? rc.bottom - rc.top : 1);
    t->used = 1;
    return (uint32_t)(slot + 1);
}

void w32_gl2d_destroy(uint32_t target_id) {
    w32_gl2d_target *t = get_target(target_id);
    if (!t) return;

    for (int i = 0; i < GL2D_MAX_FONTS; i++) {
        if (g_fonts[i].used && g_fonts[i].target_id == target_id) {
            if (g_fonts[i].hfont) DeleteObject(g_fonts[i].hfont);
            memset(&g_fonts[i], 0, sizeof(g_fonts[i]));
        }
    }

    if (g_gl2d.loaded_ogl && g_gl2d.loaded_core) {
        gl2d_make_current(t);
        if (t->atlas_tex) g_gl2d.glDeleteTextures(1, &t->atlas_tex);
        if (t->vbo) g_gl2d.glDeleteBuffers(1, &t->vbo);
        if (t->vao) g_gl2d.glDeleteVertexArrays(1, &t->vao);
        if (t->prog) g_gl2d.glDeleteProgram(t->prog);
        if (g_gl2d.wglGetCurrentContext() == t->hglrc) {
            g_gl2d.wglMakeCurrent(NULL, NULL);
        }
        if (t->hglrc) g_gl2d.wglDeleteContext(t->hglrc);
    }

    if (t->hdc && t->hwnd) ReleaseDC(t->hwnd, t->hdc);
    if (t->scratch_dc) {
        if (t->scratch_old_bmp) SelectObject(t->scratch_dc, t->scratch_old_bmp);
        if (t->scratch_bmp) DeleteObject(t->scratch_bmp);
        DeleteDC(t->scratch_dc);
    }
    free(t->atlas_stage);
    free(t->verts);
    memset(t, 0, sizeof(*t));
}

uint8_t w32_gl2d_resize(uint32_t target_id, int32_t width, int32_t height) {
    w32_gl2d_target *t = get_target(target_id);
    if (!t || width <= 0 || height <= 0) return 0;
    gl2d_make_current(t);
    t->fb_w = width;
    t->fb_h = height;
    return 1;
}

void w32_gl2d_begin_draw(uint32_t target_id, w32_colorf clear_color) {
    w32_gl2d_target *t = get_target(target_id);
    if (!t || !g_gl2d.loaded_ogl || !g_gl2d.loaded_core) return;
    gl2d_make_current(t);

    RECT rc;
    if (GetClientRect(t->hwnd, &rc)) {
        int32_t w = (int32_t)(rc.right - rc.left);
        int32_t h = (int32_t)(rc.bottom - rc.top);
        t->fb_w = w > 0 ? w : 1;
        t->fb_h = h > 0 ? h : 1;
    }

    t->vert_count = 0;
    t->clip_depth = 0;
    g_gl2d.glDisable(GL_SCISSOR_TEST);
    g_gl2d.glViewport(0, 0, (GLsizei)t->fb_w, (GLsizei)t->fb_h);
    g_gl2d.glClearColor(clear_color.r, clear_color.g, clear_color.b, clear_color.a);
    g_gl2d.glClear(GL_COLOR_BUFFER_BIT);
}

uint8_t w32_gl2d_end_draw(uint32_t target_id) {
    w32_gl2d_target *t = get_target(target_id);
    if (!t || !g_gl2d.loaded_ogl || !g_gl2d.loaded_core) return 0;
    gl2d_make_current(t);
    gl2d_flush(t);
    t->clip_depth = 0;
    g_gl2d.glDisable(GL_SCISSOR_TEST);
    return SwapBuffers(t->hdc) ? 1 : 0;
}

void w32_gl2d_push_clip(uint32_t target_id, float x, float y, float w, float h) {
    w32_gl2d_target *t = get_target(target_id);
    if (!t || !g_gl2d.loaded_ogl) return;
    gl2d_flush(t);

    RECT next;
    next.left = (LONG)floorf(x + 0.5f);
    next.top = (LONG)floorf(y + 0.5f);
    next.right = (LONG)floorf(x + (w > 0.0f ? w : 0.0f) + 0.5f);
    next.bottom = (LONG)floorf(y + (h > 0.0f ? h : 0.0f) + 0.5f);

    if (next.left < 0) next.left = 0;
    if (next.top < 0) next.top = 0;
    if (next.right > t->fb_w) next.right = t->fb_w;
    if (next.bottom > t->fb_h) next.bottom = t->fb_h;

    if (t->clip_depth > 0) {
        RECT prev = t->clip_stack[t->clip_depth - 1];
        if (next.left < prev.left) next.left = prev.left;
        if (next.top < prev.top) next.top = prev.top;
        if (next.right > prev.right) next.right = prev.right;
        if (next.bottom > prev.bottom) next.bottom = prev.bottom;
    }
    if (next.right < next.left) next.right = next.left;
    if (next.bottom < next.top) next.bottom = next.top;

    if (t->clip_depth < GL2D_CLIP_STACK_CAP) {
        t->clip_stack[t->clip_depth++] = next;
    }
    gl2d_apply_top_scissor(t);
}

void w32_gl2d_pop_clip(uint32_t target_id) {
    w32_gl2d_target *t = get_target(target_id);
    if (!t || !g_gl2d.loaded_ogl) return;
    gl2d_flush(t);
    if (t->clip_depth > 0) {
        t->clip_depth--;
    }
    gl2d_apply_top_scissor(t);
}

void w32_gl2d_clear_text_cache(uint32_t target_id) {
    w32_gl2d_target *t = get_target(target_id);
    if (!t) return;
    t->shelf_x = 0;
    t->shelf_y = 0;
    t->shelf_h = 0;
    memset(t->text_cache, 0, sizeof(t->text_cache));
}

void w32_gl2d_draw_line(
    uint32_t target_id,
    float x0, float y0, float x1, float y1,
    w32_colorf color, float stroke_width
) {
    w32_gl2d_target *t = get_target(target_id);
    if (!t || color.a <= 0.0f) return;

    float sw = stroke_width > 0.0f ? stroke_width : 1.0f;
    float r = sw * 0.5f;
    float pad = ceilf(r + 1.5f);

    float dx = x1 - x0;
    float dy = y1 - y0;
    float len = sqrtf(dx * dx + dy * dy);
    float ux = 1.0f, uy = 0.0f;
    if (len >= 0.001f) {
        ux = dx / len;
        uy = dy / len;
    } else {
        len = 0.0f;
    }
    float nx = -uy;
    float ny = ux;
    float half_len = len * 0.5f;
    float ext = half_len + pad;
    float cx = (x0 + x1) * 0.5f;
    float cy = (y0 + y1) * 0.5f;

    gl2d_vert q[4];
    q[0] = (gl2d_vert){
        cx - ux * ext - nx * pad, cy - uy * ext - ny * pad,
        -ext, -pad, half_len, 0.0f, r, 0.0f, 2.0f, color.r, color.g, color.b, color.a
    };
    q[1] = (gl2d_vert){
        cx + ux * ext - nx * pad, cy + uy * ext - ny * pad,
        ext, -pad, half_len, 0.0f, r, 0.0f, 2.0f, color.r, color.g, color.b, color.a
    };
    q[2] = (gl2d_vert){
        cx + ux * ext + nx * pad, cy + uy * ext + ny * pad,
        ext, pad, half_len, 0.0f, r, 0.0f, 2.0f, color.r, color.g, color.b, color.a
    };
    q[3] = (gl2d_vert){
        cx - ux * ext + nx * pad, cy - uy * ext + ny * pad,
        -ext, pad, half_len, 0.0f, r, 0.0f, 2.0f, color.r, color.g, color.b, color.a
    };
    gl2d_push_quad(t, q);
}

void w32_gl2d_draw_rect(
    uint32_t target_id,
    float x, float y, float w, float h,
    w32_colorf color, float stroke_width
) {
    gl2d_emit_box_primitive(get_target(target_id), x, y, w, h, 0.0f, stroke_width > 0.0f ? stroke_width : 1.0f, 0.0f, color);
}

void w32_gl2d_fill_rect(
    uint32_t target_id,
    float x, float y, float w, float h,
    w32_colorf color
) {
    gl2d_emit_box_primitive(get_target(target_id), x, y, w, h, 0.0f, 0.0f, 0.0f, color);
}

void w32_gl2d_draw_rounded_rect(
    uint32_t target_id,
    float x, float y, float w, float h, float r,
    w32_colorf color, float stroke_width
) {
    gl2d_emit_box_primitive(get_target(target_id), x, y, w, h, r > 0.0f ? r : 0.0f, stroke_width > 0.0f ? stroke_width : 1.0f, 0.0f, color);
}

void w32_gl2d_fill_rounded_rect(
    uint32_t target_id,
    float x, float y, float w, float h, float r,
    w32_colorf color
) {
    gl2d_emit_box_primitive(get_target(target_id), x, y, w, h, r > 0.0f ? r : 0.0f, 0.0f, 0.0f, color);
}

void w32_gl2d_draw_ellipse(
    uint32_t target_id,
    float cx, float cy, float rx, float ry,
    w32_colorf color, float stroke_width
) {
    gl2d_emit_box_primitive(
        get_target(target_id), cx - rx, cy - ry, rx * 2.0f, ry * 2.0f,
        0.0f, stroke_width > 0.0f ? stroke_width : 1.0f, 1.0f, color
    );
}

void w32_gl2d_fill_ellipse(
    uint32_t target_id,
    float cx, float cy, float rx, float ry,
    w32_colorf color
) {
    gl2d_emit_box_primitive(
        get_target(target_id), cx - rx, cy - ry, rx * 2.0f, ry * 2.0f,
        0.0f, 0.0f, 1.0f, color
    );
}

/* ---- Fonts & GPU VRAM GL_R8 Glyph Atlas ---------------------------------- */

uint32_t w32_gl2d_create_font(
    uint32_t target_id,
    w32_bytes family,
    float size,
    int32_t weight,
    uint8_t italic
) {
    if (!get_target(target_id)) return 0;

    int slot = -1;
    for (int i = 0; i < GL2D_MAX_FONTS; i++) {
        if (!g_fonts[i].used) { slot = i; break; }
    }
    if (slot < 0) return 0;

    w32_wide_buf wfam;
    if (!w32_wide_init(&wfam, family)) return 0;
    const wchar_t *fname = wfam.len > 0 ? wfam.ptr : L"Segoe UI";

    int px_h = size > 1.0f ? (int)(size + 0.5f) : 14;
    HFONT hf = CreateFontW(
        -px_h, 0, 0, 0,
        weight > 0 ? (int)weight : 400,
        italic ? TRUE : FALSE,
        FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, fname
    );
    w32_wide_free(&wfam);
    if (!hf) return 0;

    g_fonts[slot].used = 1;
    g_fonts[slot].target_id = target_id;
    g_fonts[slot].hfont = hf;
    return (uint32_t)(slot + 1);
}

void w32_gl2d_destroy_font(uint32_t font_id) {
    w32_gl2d_font *f = get_font(font_id);
    if (!f) return;
    if (f->hfont) DeleteObject(f->hfont);
    memset(f, 0, sizeof(*f));
}

static void fnv1a_dual(const uint8_t *p, int64_t len, uint32_t font_id, uint64_t *h1_out, uint64_t *h2_out) {
    uint64_t h1 = 14695981039346656037ull ^ ((uint64_t)font_id * 0x9E3779B97F4A7C15ull);
    uint64_t h2 = 0xcbf29ce484222325ull ^ ((uint64_t)font_id * 0xBF58476D1CE4E5B9ull);
    for (int64_t i = 0; i < len; i++) {
        uint8_t b = p[i];
        h1 = (h1 ^ b) * 1099511628211ull;
        h2 = ((h2 << 5) | (h2 >> 59)) ^ b;
        h2 *= 0x9E3779B185EBCA87ull;
    }
    *h1_out = h1;
    *h2_out = h2;
}

static gl2d_text_entry *gl2d_cache_lookup(
    w32_gl2d_target *t, uint64_t h1, uint64_t h2, uint32_t font_id, int32_t len, uint32_t *out_idx
) {
    uint32_t mask = GL2D_TEXT_CACHE_CAP - 1;
    uint32_t idx = (uint32_t)(h1 ^ (h1 >> 32)) & mask;
    uint32_t first_free = mask + 1u;
    for (uint32_t step = 0; step < GL2D_TEXT_CACHE_CAP; step++) {
        gl2d_text_entry *e = &t->text_cache[idx];
        if (!e->occupied) {
            if (out_idx) *out_idx = (first_free <= mask) ? first_free : idx;
            return NULL;
        }
        if (e->h1 == h1 && e->h2 == h2 && e->font_id == font_id && e->text_len == len) {
            return e;
        }
        idx = (idx + 1u) & mask;
    }
    if (out_idx) *out_idx = idx;
    return NULL;
}

static gl2d_text_entry *gl2d_rasterize_and_upload(
    w32_gl2d_target *t,
    w32_bytes text,
    uint32_t font_id,
    HFONT hfont,
    uint64_t h1,
    uint64_t h2,
    uint32_t slot_hint
) {
    w32_wide_buf wb;
    if (!w32_wide_init(&wb, text) || wb.len <= 0) {
        w32_wide_free(&wb);
        return NULL;
    }

    HGDIOBJ prev_f = SelectObject(t->scratch_dc, hfont);
    RECT calc = { 0, 0, GL2D_SCRATCH_W - 4, GL2D_SCRATCH_H - 4 };
    DrawTextW(t->scratch_dc, wb.ptr, wb.len, &calc, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX | DT_CALCRECT);

    int32_t tw = (int32_t)calc.right;
    int32_t th = (int32_t)calc.bottom;
    if (tw < 1) tw = 1;
    if (th < 1) th = 1;
    if (tw > GL2D_SCRATCH_W - 4) tw = GL2D_SCRATCH_W - 4;
    if (th > GL2D_SCRATCH_H - 4) th = GL2D_SCRATCH_H - 4;

    for (int32_t r = 0; r < th; r++) {
        memset(t->scratch_bits + (size_t)r * GL2D_SCRATCH_W, 0, (size_t)tw * sizeof(uint32_t));
    }

    SetBkMode(t->scratch_dc, TRANSPARENT);
    SetTextColor(t->scratch_dc, RGB(255, 255, 255));
    RECT draw_rc = { 0, 0, tw, th };
    DrawTextW(t->scratch_dc, wb.ptr, wb.len, &draw_rc, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
    GdiFlush();
    SelectObject(t->scratch_dc, prev_f);
    w32_wide_free(&wb);

    /* Extract grayscale coverage into tight staging buffer. */
    for (int32_t r = 0; r < th; r++) {
        const uint32_t *src_row = t->scratch_bits + (size_t)r * GL2D_SCRATCH_W;
        uint8_t *dst_row = t->atlas_stage + (size_t)r * (size_t)tw;
        for (int32_t c = 0; c < tw; c++) {
            uint32_t px = src_row[c];
            dst_row[c] = (uint8_t)((px >> 8) & 0xFFu);
        }
    }

    int32_t cell_w = tw + 1;
    int32_t cell_h = th + 1;
    if (t->shelf_x + cell_w > GL2D_ATLAS_W) {
        t->shelf_x = 0;
        t->shelf_y += t->shelf_h;
        t->shelf_h = 0;
    }
    if (t->shelf_y + cell_h > GL2D_ATLAS_H) {
        /* Flush queued vertices before overwriting the shared atlas! */
        gl2d_flush(t);
        t->shelf_x = 0;
        t->shelf_y = 0;
        t->shelf_h = 0;
        memset(t->text_cache, 0, sizeof(t->text_cache));
        slot_hint = (uint32_t)(h1 ^ (h1 >> 32)) & (GL2D_TEXT_CACHE_CAP - 1u);
    }

    int32_t ax = t->shelf_x;
    int32_t ay = t->shelf_y;
    t->shelf_x += cell_w;
    if (cell_h > t->shelf_h) t->shelf_h = cell_h;

    gl2d_make_current(t);
    g_gl2d.glBindTexture(GL_TEXTURE_2D, t->atlas_tex);
    g_gl2d.glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    g_gl2d.glTexSubImage2D(
        GL_TEXTURE_2D, 0, (GLint)ax, (GLint)ay, (GLsizei)tw, (GLsizei)th,
        GL_RED, GL_UNSIGNED_BYTE, t->atlas_stage
    );
    g_gl2d.glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    /* Ensure slot_hint points to an unoccupied slot; replace slot_hint if table saturated. */
    gl2d_text_entry *dst = &t->text_cache[slot_hint % GL2D_TEXT_CACHE_CAP];
    dst->occupied = 1;
    dst->h1 = h1;
    dst->h2 = h2;
    dst->font_id = font_id;
    dst->text_len = (int32_t)text.len;
    dst->tw = tw;
    dst->th = th;
    dst->u0 = (float)ax / (float)GL2D_ATLAS_W;
    dst->v0 = (float)ay / (float)GL2D_ATLAS_H;
    dst->u1 = (float)(ax + tw) / (float)GL2D_ATLAS_W;
    dst->v1 = (float)(ay + th) / (float)GL2D_ATLAS_H;
    return dst;
}

void w32_gl2d_draw_text(
    uint32_t target_id,
    w32_bytes text,
    uint32_t font_id,
    float x, float y, float w, float h,
    int32_t h_align,
    int32_t v_align,
    w32_colorf color
) {
    w32_gl2d_target *t = get_target(target_id);
    w32_gl2d_font *f = get_font(font_id);
    if (!t || !f || !f->hfont || !text.data || text.len <= 0 || w <= 0.0f || h <= 0.0f || color.a <= 0.0f) {
        return;
    }

    uint64_t h1 = 0, h2 = 0;
    fnv1a_dual(text.data, text.len, font_id, &h1, &h2);
    uint32_t slot_hint = 0;
    gl2d_text_entry *e = gl2d_cache_lookup(t, h1, h2, font_id, (int32_t)text.len, &slot_hint);
    if (!e) {
        e = gl2d_rasterize_and_upload(t, text, font_id, f->hfont, h1, h2, slot_hint);
        if (!e) return;
    }

    float ftw = (float)e->tw;
    float fth = (float)e->th;

    float dx = x;
    if (h_align == 1) {
        dx = x + (w - ftw) * 0.5f;
    } else if (h_align == 2) {
        dx = x + w - ftw;
    }

    float dy = y;
    if (v_align == 1) {
        dy = y + (h - fth) * 0.5f;
    } else if (v_align == 2) {
        dy = y + h - fth;
    }

    dx = floorf(dx + 0.5f);
    dy = floorf(dy + 0.5f);

    /* Box-crop overflowing text to [x, x+w] x [y, y+h] with exact UV clamping. */
    float qx0 = dx > x ? dx : x;
    float qy0 = dy > y ? dy : y;
    float qx1 = (dx + ftw) < (x + w) ? (dx + ftw) : (x + w);
    float qy1 = (dy + fth) < (y + h) ? (dy + fth) : (y + h);
    if (qx1 <= qx0 || qy1 <= qy0) return;

    float u_span = e->u1 - e->u0;
    float v_span = e->v1 - e->v0;
    float cu0 = e->u0 + ((qx0 - dx) / ftw) * u_span;
    float cv0 = e->v0 + ((qy0 - dy) / fth) * v_span;
    float cu1 = e->u0 + ((qx1 - dx) / ftw) * u_span;
    float cv1 = e->v0 + ((qy1 - dy) / fth) * v_span;

    gl2d_vert q[4];
    q[0] = (gl2d_vert){ qx0, qy0, cu0, cv0, 0.0f, 0.0f, 0.0f, 0.0f, 3.0f, color.r, color.g, color.b, color.a };
    q[1] = (gl2d_vert){ qx1, qy0, cu1, cv0, 0.0f, 0.0f, 0.0f, 0.0f, 3.0f, color.r, color.g, color.b, color.a };
    q[2] = (gl2d_vert){ qx1, qy1, cu1, cv1, 0.0f, 0.0f, 0.0f, 0.0f, 3.0f, color.r, color.g, color.b, color.a };
    q[3] = (gl2d_vert){ qx0, qy1, cu0, cv1, 0.0f, 0.0f, 0.0f, 0.0f, 3.0f, color.r, color.g, color.b, color.a };
    gl2d_push_quad(t, q);
}

uint8_t w32_gl2d_readback_pixel(uint32_t target_id, int32_t x, int32_t y, uint32_t *out_rgba) {
    w32_gl2d_target *t = get_target(target_id);
    if (!t || !out_rgba || !g_gl2d.loaded_ogl || !g_gl2d.loaded_core) return 0;
    gl2d_flush(t);

    int32_t gl_y = t->fb_h - 1 - y;
    if (x < 0 || gl_y < 0 || x >= t->fb_w || gl_y >= t->fb_h) return 0;

    g_gl2d.glFinish();
    while (g_gl2d.glGetError() != GL_NO_ERROR) {}
    g_gl2d.glPixelStorei(GL_PACK_ALIGNMENT, 4);
    uint32_t px = 0;
    g_gl2d.glReadPixels((GLint)x, (GLint)gl_y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &px);
    *out_rgba = px;
    return g_gl2d.glGetError() == GL_NO_ERROR ? 1 : 0;
}
