#include "gles_surface.h"

#include <GLES3/gl3.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {
// EGL initialization is display-wide, not context-wide. ANGLE windows commonly
// share one EGLDisplay, so closing a floating surface must keep its peers alive.
std::mutex egl_display_mutex;
std::map<EGLDisplay, unsigned> egl_display_users;

class GraphicsLibrary {
public:
    GraphicsLibrary() = default;
    GraphicsLibrary(const GraphicsLibrary&) = delete;
    GraphicsLibrary& operator=(const GraphicsLibrary&) = delete;

    ~GraphicsLibrary()
    {
        if (handle_) {
#ifdef _WIN32
            FreeLibrary(handle_);
#else
            dlclose(handle_);
#endif
        }
    }

    bool open(const char* name)
    {
#ifdef _WIN32
        handle_ = LoadLibraryExA(name, nullptr,
            LOAD_LIBRARY_SEARCH_APPLICATION_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
#else
        handle_ = dlopen(name, RTLD_NOW | RTLD_LOCAL);
#endif
        return handle_ != nullptr;
    }

    template<class Function>
    bool bind(Function& function, const char* name) const
    {
#ifdef _WIN32
        function = reinterpret_cast<Function>(GetProcAddress(handle_, name));
#else
        function = reinterpret_cast<Function>(dlsym(handle_, name));
#endif
        return function != nullptr;
    }

private:
#ifdef _WIN32
    HMODULE handle_ = nullptr;
#else
    void* handle_ = nullptr;
#endif
};
}

struct GlesSurface::Api {
    GraphicsLibrary gles;
    GraphicsLibrary egl;
    decltype(&::eglGetError) eglGetError = nullptr;
    decltype(&::eglGetCurrentContext) eglGetCurrentContext = nullptr;
    decltype(&::eglMakeCurrent) eglMakeCurrent = nullptr;
    decltype(&::eglDestroySurface) eglDestroySurface = nullptr;
    decltype(&::eglDestroyContext) eglDestroyContext = nullptr;
    decltype(&::eglTerminate) eglTerminate = nullptr;
    decltype(&::eglGetDisplay) eglGetDisplay = nullptr;
    decltype(&::eglInitialize) eglInitialize = nullptr;
    decltype(&::eglBindAPI) eglBindAPI = nullptr;
    decltype(&::eglChooseConfig) eglChooseConfig = nullptr;
    decltype(&::eglGetConfigAttrib) eglGetConfigAttrib = nullptr;
    decltype(&::eglCreateContext) eglCreateContext = nullptr;
    decltype(&::eglCreateWindowSurface) eglCreateWindowSurface = nullptr;
    decltype(&::eglSwapBuffers) eglSwapBuffers = nullptr;
    decltype(&::glDisable) glDisable = nullptr;
    decltype(&::glEnable) glEnable = nullptr;
    decltype(&::glViewport) glViewport = nullptr;
    decltype(&::glScissor) glScissor = nullptr;
    decltype(&::glClearColor) glClearColor = nullptr;
    decltype(&::glClear) glClear = nullptr;

    decltype(&::glCreateShader) glCreateShader = nullptr;
    decltype(&::glShaderSource) glShaderSource = nullptr;
    decltype(&::glCompileShader) glCompileShader = nullptr;
    decltype(&::glGetShaderiv) glGetShaderiv = nullptr;
    decltype(&::glDeleteShader) glDeleteShader = nullptr;
    decltype(&::glCreateProgram) glCreateProgram = nullptr;
    decltype(&::glAttachShader) glAttachShader = nullptr;
    decltype(&::glLinkProgram) glLinkProgram = nullptr;
    decltype(&::glGetProgramiv) glGetProgramiv = nullptr;
    decltype(&::glDeleteProgram) glDeleteProgram = nullptr;
    decltype(&::glGetUniformLocation) glGetUniformLocation = nullptr;
    decltype(&::glUseProgram) glUseProgram = nullptr;
    decltype(&::glUniform4f) glUniform4f = nullptr;
    decltype(&::glBlendFuncSeparate) glBlendFuncSeparate = nullptr;
    decltype(&::glDrawArrays) glDrawArrays = nullptr;

    bool load()
    {
#ifdef _WIN32
        if (!gles.open("libGLESv2.dll") || !egl.open("libEGL.dll")) {
#else
        if (!egl.open("libEGL.so.1") || !gles.open("libGLESv2.so.2")) {
#endif
            return false;
        }
        return egl.bind(eglGetError, "eglGetError") &&
               egl.bind(eglGetCurrentContext, "eglGetCurrentContext") &&
               egl.bind(eglMakeCurrent, "eglMakeCurrent") &&
               egl.bind(eglDestroySurface, "eglDestroySurface") &&
               egl.bind(eglDestroyContext, "eglDestroyContext") &&
               egl.bind(eglTerminate, "eglTerminate") &&
               egl.bind(eglGetDisplay, "eglGetDisplay") &&
               egl.bind(eglInitialize, "eglInitialize") &&
               egl.bind(eglBindAPI, "eglBindAPI") &&
               egl.bind(eglChooseConfig, "eglChooseConfig") &&
               egl.bind(eglGetConfigAttrib, "eglGetConfigAttrib") &&
               egl.bind(eglCreateContext, "eglCreateContext") &&
               egl.bind(eglCreateWindowSurface, "eglCreateWindowSurface") &&
               egl.bind(eglSwapBuffers, "eglSwapBuffers") &&
               gles.bind(glDisable, "glDisable") &&
               gles.bind(glEnable, "glEnable") &&
               gles.bind(glViewport, "glViewport") &&
               gles.bind(glScissor, "glScissor") &&
               gles.bind(glClearColor, "glClearColor") &&
               gles.bind(glClear, "glClear") &&
               gles.bind(glCreateShader, "glCreateShader") &&
               gles.bind(glShaderSource, "glShaderSource") &&
               gles.bind(glCompileShader, "glCompileShader") &&
               gles.bind(glGetShaderiv, "glGetShaderiv") &&
               gles.bind(glDeleteShader, "glDeleteShader") &&
               gles.bind(glCreateProgram, "glCreateProgram") &&
               gles.bind(glAttachShader, "glAttachShader") &&
               gles.bind(glLinkProgram, "glLinkProgram") &&
               gles.bind(glGetProgramiv, "glGetProgramiv") &&
               gles.bind(glDeleteProgram, "glDeleteProgram") &&
               gles.bind(glGetUniformLocation, "glGetUniformLocation") &&
               gles.bind(glUseProgram, "glUseProgram") &&
               gles.bind(glUniform4f, "glUniform4f") &&
               gles.bind(glBlendFuncSeparate, "glBlendFuncSeparate") &&
               gles.bind(glDrawArrays, "glDrawArrays");
    }
};

GlesSurface::GlesSurface() = default;

bool GlesSurface::failed(const char* stage) const
{
    std::fprintf(stderr, "Sunfleck: EGL %s failed (0x%04x)\n", stage, api_->eglGetError());
    return false;
}

namespace {

bool inject(const char* stage)
{
#ifdef SUNFLECK_TEST_EGL
    const auto* value = std::getenv("SUNFLECK_TEST_EGL_FAILURE");
    return value && std::strcmp(value, stage) == 0;
#else
    (void)stage;
    return false;
#endif
}
}

GlesSurface::~GlesSurface()
{
    if (!initialized_) {
        return;
    }
    if (context_ != EGL_NO_CONTEXT && api_->eglGetCurrentContext() == context_) {
        api_->eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }
    if (surface_ != EGL_NO_SURFACE) {
        api_->eglDestroySurface(display_, surface_);
    }
    if (context_ != EGL_NO_CONTEXT) {
        api_->eglDestroyContext(display_, context_);
    }
    std::lock_guard lock(egl_display_mutex);
    const auto users = egl_display_users.find(display_);
    if (users != egl_display_users.end() && --users->second == 0) {
        api_->eglTerminate(display_); egl_display_users.erase(users);
    }
}

bool GlesSurface::open(EGLNativeDisplayType native_display, EGLNativeWindowType window, int visual)
{
    api_ = std::make_unique<Api>();
    if (inject("load") || !api_->load()) {
        std::fprintf(stderr, "Sunfleck: EGL/GLES libraries or required symbols unavailable\n");
        return false;
    }
    display_ = api_->eglGetDisplay(native_display);
    if (display_ == EGL_NO_DISPLAY || inject("initialize")) {
        return failed("initialize");
    }
    {
        std::lock_guard lock(egl_display_mutex);
        auto [users, inserted] = egl_display_users.try_emplace(display_, 0);
        (void)inserted;
        if (!users->second && !api_->eglInitialize(display_, nullptr, nullptr)) {
            egl_display_users.erase(users); return failed("initialize");
        }
        ++users->second; initialized_ = true;
    }
    if (!api_->eglBindAPI(EGL_OPENGL_ES_API)) {
        return failed("bind API");
    }

    constexpr EGLint attributes[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RENDERABLE_TYPE, 0x0040, // ES 3
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_NONE
    };
    EGLint count = 0;
    if (!api_->eglChooseConfig(display_, attributes, nullptr, 0, &count) || count == 0) {
        return failed("choose config");
    }
    std::vector<EGLConfig> configs(count);
    if (!api_->eglChooseConfig(display_, attributes, configs.data(), count, &count)) {
        return failed("choose config");
    }
    EGLConfig config = nullptr;
    for (EGLint i = 0; i < count; ++i) {
        EGLint candidate = 0;
        if (visual == 0 || (api_->eglGetConfigAttrib(display_, configs[i], EGL_NATIVE_VISUAL_ID, &candidate) &&
                            candidate == visual)) {
            config = configs[i];
            break;
        }
    }
    if (!config) {
        return failed("match native visual");
    }

    constexpr EGLint version[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    if (!inject("context")) {
        context_ = api_->eglCreateContext(display_, config, EGL_NO_CONTEXT, version);
    }
    if (context_ == EGL_NO_CONTEXT) {
        return failed("create context");
    }
    if (!inject("surface")) {
        surface_ = api_->eglCreateWindowSurface(display_, config, window, nullptr);
    }
    if (surface_ == EGL_NO_SURFACE) {
        return failed("create surface");
    }
    if (!api_->eglMakeCurrent(display_, surface_, surface_, context_)) {
        return failed("make current");
    }
#ifdef _WIN32
    config_ = config;
    window_ = window;
#endif
    return true;
}

bool GlesSurface::create_blend_program()
{
    constexpr const char* vertex = R"(#version 300 es
void main() {
    vec2 corner = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    gl_Position = vec4(corner * 2.0 - 1.0, 0.0, 1.0);
})";
    constexpr const char* fragment = R"(#version 300 es
precision highp float;
uniform vec4 tint;
out vec4 color;
void main() { color = tint; }
)";
    const GLuint program = api_->glCreateProgram();
    if (!program) {
        return false;
    }
    const auto attach = [&](GLenum type, const char* source) {
        const GLuint shader = api_->glCreateShader(type);
        if (!shader) {
            return false;
        }
        api_->glShaderSource(shader, 1, &source, nullptr);
        api_->glCompileShader(shader);
        GLint compiled = GL_FALSE;
        api_->glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (compiled) {
            api_->glAttachShader(program, shader);
        }
        api_->glDeleteShader(shader);
        return compiled == GL_TRUE;
    };
    GLint linked = GL_FALSE;
    if (attach(GL_VERTEX_SHADER, vertex) && attach(GL_FRAGMENT_SHADER, fragment)) {
        api_->glLinkProgram(program);
        api_->glGetProgramiv(program, GL_LINK_STATUS, &linked);
    }
    if (!linked) {
        api_->glDeleteProgram(program);
        return false;
    }
    blend_color_ = api_->glGetUniformLocation(program, "tint");
    blend_program_ = program;
    return true;
}

bool GlesSurface::present(int width, int height, std::span<const FillRectCommand> commands)
{
#ifdef _WIN32
    if (presented_ && (width != width_ || height != height_)) {
        if (!api_->eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT) ||
            !api_->eglDestroySurface(display_, surface_)) {
            return failed("release resized surface");
        }
        surface_ = api_->eglCreateWindowSurface(display_, config_, window_, nullptr);
        if (surface_ == EGL_NO_SURFACE) {
            return failed("recreate resized surface");
        }
    }
    width_ = width;
    height_ = height;
#endif
    if (!api_->eglMakeCurrent(display_, surface_, surface_, context_)) {
        return failed("make current");
    }
    api_->glDisable(GL_DITHER);
    api_->glEnable(GL_SCISSOR_TEST);
    api_->glViewport(0, 0, width, height);
    api_->glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    for (const auto& rect : commands) {
        const int left = std::clamp(rect.left, 0, width);
        const int right = std::clamp(rect.right, left, width);
        const int top = std::clamp(rect.top, 0, height);
        const int bottom = std::clamp(rect.bottom, top, height);
        api_->glScissor(left, height - bottom, right - left, bottom - top);
        const float red = rect.color[0] / 255.0f;
        const float green = rect.color[1] / 255.0f;
        const float blue = rect.color[2] / 255.0f;
        if (rect.color[3] == 255) {
            api_->glClearColor(red, green, blue, 1.0f);
            api_->glClear(GL_COLOR_BUFFER_BIT);
        } else {
            if (!blend_program_ && !create_blend_program()) {
                return failed("create alpha blend shader");
            }
            api_->glUseProgram(blend_program_);
            api_->glEnable(GL_BLEND);
            api_->glUniform4f(blend_color_, red, green, blue, rect.color[3] / 255.0f);
            api_->glDrawArrays(GL_TRIANGLES, 0, 3);
            api_->glDisable(GL_BLEND);
        }
    }
    if (presented_ && inject("swap")) {
        std::fprintf(stderr, "Sunfleck: injected EGL swap failure after first frame\n");
        return false;
    }
    if (!api_->eglSwapBuffers(display_, surface_)) {
        return failed("swap buffers");
    }
    presented_ = true;
    return true;
}
