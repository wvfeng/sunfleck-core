#pragma once
#include "drawing_commands.h"

class GpuSurface {
public:
    virtual bool records_commands() const { return false; }
    virtual bool present_frame(int width,int height,std::span<const FillRectCommand> reference,const drawing::CommandList&) {
        return present(width,height,reference);
    }
    virtual ~GpuSurface() = default;
    virtual bool present(int width, int height, std::span<const FillRectCommand> commands) = 0;
};