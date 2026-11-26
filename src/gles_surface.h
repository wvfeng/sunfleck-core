#pragma once

#include "gpu_surface.h"

#include <EGL/egl.h>
#include <memory>

class GlesSurface final : public GpuSurface {
public:
    GlesSurface();
    ~GlesSurface() override;
    GlesSurface(const GlesSurface&) = delete;
    GlesSurface& operator=(const GlesSurface&) = delete;

    bool open(EGLNativeDisplayType display, EGLNativeWindowType window, int visual);
    bool present(int width, int height, std::span<const FillRectCommand> commands) override;

private:
    struct Api;
    std::unique_ptr<Api> api_;
    bool create_blend_program();
    unsigned blend_program_ = 0;
    int blend_color_ = -1;
    bool failed(const char* stage) const;
    EGLDisplay display_ = EGL_NO_DISPLAY;
    EGLContext context_ = EGL_NO_CONTEXT;
    EGLSurface surface_ = EGL_NO_SURFACE;
    bool initialized_ = false;
    bool presented_ = false;
#ifdef _WIN32
    EGLConfig config_ = nullptr;
    EGLNativeWindowType window_ = nullptr;
    int width_ = 0;
    int height_ = 0;
#endif
};
