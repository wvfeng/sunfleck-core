#pragma once

#include "gpu_surface.h"
#include <memory>

#ifdef __OBJC__
@class CALayer;
std::unique_ptr<GpuSurface> open_metal_surface(CALayer* parent);
#endif

bool metal_device_available();
bool metal_readback(int width, int height, std::span<const FillRectCommand> commands,
                    std::vector<uint8_t>& pixels);
