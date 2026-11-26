#include "metal_surface.h"

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <QuartzCore/CATransaction.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <simd/simd.h>

namespace {
constexpr auto format = MTLPixelFormatBGRA8Unorm;

struct Rectangle {
    simd_float4 bounds;
    simd_float4 color;
};

const char* shader = R"metal(
#include <metal_stdlib>
using namespace metal;

struct Rectangle { float4 bounds; float4 color; };
struct Vertex { float4 position [[position]]; float4 color; };

vertex Vertex rect_vertex(uint vertex_id [[vertex_id]], uint instance [[instance_id]],
                          const device Rectangle* rects [[buffer(0)]],
                          constant float2& viewport [[buffer(1)]])
{
    const float2 corners[] = {{0,0}, {1,0}, {0,1}, {1,1}};
    Rectangle rect = rects[instance];
    float2 pixel = mix(rect.bounds.xy, rect.bounds.zw, corners[vertex_id]);
    return {float4(pixel / viewport * float2(2,-2) + float2(-1,1), 0, 1), rect.color};
}

fragment float4 rect_fragment(Vertex input [[stage_in]]) { return input.color; }
)metal";

class MetalRenderer {
public:
    id<MTLDevice> device;

    bool open()
    {
        device = MTLCreateSystemDefaultDevice();
        queue_ = [device newCommandQueue];
        if (!queue_) {
            return false;
        }
        NSError* error = nil;
        id<MTLLibrary> library = [device newLibraryWithSource:@(shader) options:nil error:&error];
        if (library) {
            auto* descriptor = [MTLRenderPipelineDescriptor new];
            descriptor.vertexFunction = [library newFunctionWithName:@"rect_vertex"];
            descriptor.fragmentFunction = [library newFunctionWithName:@"rect_fragment"];
            auto* color = descriptor.colorAttachments[0];
            color.pixelFormat = format;
            color.blendingEnabled = YES;
            color.sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
            color.destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
            color.sourceAlphaBlendFactor = MTLBlendFactorOne;
            color.destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
            pipeline_ = [device newRenderPipelineStateWithDescriptor:descriptor error:&error];
        }
        if (!pipeline_) {
            std::fprintf(stderr, "Sunfleck: Metal pipeline initialization failed: %s\n",
                         error.localizedDescription.UTF8String ?: "unknown error");
        }
        return pipeline_ != nil;
    }

    id<MTLCommandBuffer> encode(id<MTLTexture> surface, std::span<const FillRectCommand> commands)
    {
        const auto width = static_cast<int>(surface.width);
        const auto height = static_cast<int>(surface.height);
        rectangles_.clear();
        rectangles_.reserve(commands.size());
        for (const auto& rect : commands) {
            const auto left = std::max(0, rect.left);
            const auto top = std::max(0, rect.top);
            const auto right = std::min(width, rect.right);
            const auto bottom = std::min(height, rect.bottom);
            if (left < right && top < bottom && rect.color[3]) {
                rectangles_.push_back({
                    {float(left), float(top), float(right), float(bottom)},
                    {rect.color[0] / 255.f, rect.color[1] / 255.f, rect.color[2] / 255.f, rect.color[3] / 255.f}
                });
            }
        }
        id<MTLBuffer> vertices = nil;
        if (!rectangles_.empty()) {
            vertices = [device newBufferWithBytes:rectangles_.data()
                                          length:rectangles_.size() * sizeof(Rectangle)
                                         options:MTLResourceStorageModeShared];
            if (!vertices) {
                return nil;
            }
        }
        auto* pass = [MTLRenderPassDescriptor renderPassDescriptor];
        auto* color = pass.colorAttachments[0];
        color.texture = surface;
        color.loadAction = MTLLoadActionClear;
        color.storeAction = MTLStoreActionStore;
        color.clearColor = MTLClearColorMake(0, 0, 0, 0);
        id<MTLCommandBuffer> command = [queue_ commandBuffer];
        id<MTLRenderCommandEncoder> encoder = [command renderCommandEncoderWithDescriptor:pass];
        if (!encoder) {
            return nil;
        }
        if (vertices) {
            const simd_float2 viewport{float(width), float(height)};
            [encoder setRenderPipelineState:pipeline_];
            [encoder setVertexBuffer:vertices offset:0 atIndex:0];
            [encoder setVertexBytes:&viewport length:sizeof(viewport) atIndex:1];
            [encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4
                      instanceCount:rectangles_.size()];
        }
        [encoder endEncoding];
        return command;
    }

private:
    id<MTLCommandQueue> queue_;
    id<MTLRenderPipelineState> pipeline_;
    std::vector<Rectangle> rectangles_;
};

class MetalSurface final : public GpuSurface {
public:
    bool open(CALayer* parent)
    {
        if (!renderer_.open()) {
            return false;
        }
        layer_ = [CAMetalLayer layer];
        layer_.device = renderer_.device;
        layer_.pixelFormat = format;
        auto space = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
        if (space) {
            layer_.colorspace = space;
            CGColorSpaceRelease(space);
        }
        [parent addSublayer:layer_];
        return true;
    }

    ~MetalSurface() override
    {
        [layer_ removeFromSuperlayer];
    }

    bool present(int width, int height, std::span<const FillRectCommand> commands) override
    {
        @autoreleasepool {
            if (failed_->load()) {
                return false;
            }
            [CATransaction begin];
            [CATransaction setDisableActions:YES];
            layer_.frame = layer_.superlayer.bounds;
            layer_.drawableSize = CGSizeMake(width, height);
            [CATransaction commit];
            id<CAMetalDrawable> drawable = [layer_ nextDrawable];
            if (!drawable) {
                return false;
            }
            auto command = renderer_.encode(drawable.texture, commands);
            if (!command) {
                return false;
            }
            const auto failed = failed_;
            [command addCompletedHandler:^(id<MTLCommandBuffer> finished) {
                if (finished.status == MTLCommandBufferStatusError) {
                    failed->store(true);
                    std::fprintf(stderr, "Sunfleck: Metal command failed: %s\n",
                                 finished.error.localizedDescription.UTF8String ?: "unknown error");
                }
            }];
            [command presentDrawable:drawable];
            [command commit];
            if (!submitted_) {
                // Check first-frame failures even for windows that only paint once.
                [command waitUntilCompleted];
                if (command.status == MTLCommandBufferStatusError) {
                    return false;
                }
                submitted_ = true;
            }
            return true;
        }
    }

private:
    MetalRenderer renderer_;
    CAMetalLayer* layer_;
    std::shared_ptr<std::atomic_bool> failed_ = std::make_shared<std::atomic_bool>(false);
    bool submitted_ = false;
};
}

std::unique_ptr<GpuSurface> open_metal_surface(CALayer* parent)
{
    @autoreleasepool {
        auto surface = std::make_unique<MetalSurface>();
        if (!surface->open(parent)) {
            return nullptr;
        }
        return surface;
    }
}

bool metal_device_available()
{
    @autoreleasepool {
        return MTLCreateSystemDefaultDevice() != nil;
    }
}

bool metal_readback(int width, int height, std::span<const FillRectCommand> commands,
                    std::vector<uint8_t>& pixels)
{
    @autoreleasepool {
        MetalRenderer renderer;
        if (width <= 0 || height <= 0 || !renderer.open()) {
            return false;
        }
        auto* descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format
                                      width:width height:height mipmapped:NO];
        descriptor.usage = MTLTextureUsageRenderTarget;
        descriptor.storageMode = MTLStorageModeShared;
        auto texture = [renderer.device newTextureWithDescriptor:descriptor];
        if (!texture) {
            return false;
        }
        auto command = renderer.encode(texture, commands);
        [command commit];
        [command waitUntilCompleted];
        if (command.status != MTLCommandBufferStatusCompleted) {
            return false;
        }
        const auto stride = size_t(width) * 4;
        pixels.resize(stride * height);
        [texture getBytes:pixels.data() bytesPerRow:stride
                fromRegion:MTLRegionMake2D(0, 0, width, height) mipmapLevel:0];
        return true;
    }
}
