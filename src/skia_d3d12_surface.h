#pragma once
#include "gpu_surface.h"
#include <memory>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

// Skia stays private to the native implementation; no public SDK types depend on it.
class SkiaD3d12Surface final : public GpuSurface {
public:
    SkiaD3d12Surface();
    ~SkiaD3d12Surface() override;
    bool open(HWND window);
    bool records_commands() const override { return true; }
    bool present_frame(int width,int height,std::span<const FillRectCommand> reference,const drawing::CommandList& list) override;
    bool present(int width, int height, std::span<const FillRectCommand> commands) override;
#ifdef SUNFLECK_TEST_SKIA_D3D12
    const std::vector<std::uint8_t>& captured_pixels() const;
    bool debug_clean();
#endif
private:
    bool draw_frame(int width,int height,std::span<const FillRectCommand> commands,const drawing::CommandList* list);
    struct State;
    std::unique_ptr<State> state_;
};
