#include "d3d11_surface.h"
#include "d3d11_rect_vs.h"
#include "d3d11_rect_ps.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using Microsoft::WRL::ComPtr;

namespace {
bool succeeded(HRESULT result, const char* operation)
{
    if (FAILED(result)) {
        std::fprintf(stderr, "Sunfleck: D3D11 %s failed (0x%08lx)\n",
                     operation, static_cast<unsigned long>(result));
        return false;
    }
    return true;
}

bool inject(const char* stage)
{
#ifdef SUNFLECK_TEST_D3D11
    const auto* value = std::getenv("SUNFLECK_TEST_D3D11_FAILURE");
    return value && std::strcmp(value, stage) == 0;
#else
    (void)stage;
    return false;
#endif
}
}

bool D3d11Surface::open(HWND window)
{
    if (inject("initialize")) {
        return succeeded(DXGI_ERROR_DEVICE_REMOVED, "initialize (injected)");
    }
    DXGI_SWAP_CHAIN_DESC swap{};
    swap.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swap.SampleDesc.Count = 1;
    swap.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap.BufferCount = 1;
    swap.OutputWindow = window;
    swap.Windowed = TRUE;
    // Keep the Win32 client surface readable via GDI for window captures.
    swap.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    constexpr D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0};
    if (!succeeded(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
            0, levels, 1, D3D11_SDK_VERSION, &swap, swap_chain_.GetAddressOf(),
            device_.GetAddressOf(), nullptr, context_.GetAddressOf()), "create device and swap chain")) {
        return false;
    }

    D3D11_RASTERIZER_DESC rasterizer{};
    rasterizer.FillMode = D3D11_FILL_SOLID;
    rasterizer.CullMode = D3D11_CULL_NONE;
    rasterizer.DepthClipEnable = TRUE;
    rasterizer.ScissorEnable = TRUE;

    D3D11_BLEND_DESC blend{};
    auto& target = blend.RenderTarget[0];
    target.BlendEnable = TRUE;
    target.SrcBlend = D3D11_BLEND_SRC_ALPHA;
    target.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    target.BlendOp = D3D11_BLEND_OP_ADD;
    target.SrcBlendAlpha = D3D11_BLEND_ONE;
    target.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    target.BlendOpAlpha = D3D11_BLEND_OP_ADD;
    target.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    D3D11_BUFFER_DESC color{};
    color.ByteWidth = 4 * sizeof(float);
    color.Usage = D3D11_USAGE_DEFAULT;
    color.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    return succeeded(device_->CreateVertexShader(d3d11_rect_vs, sizeof(d3d11_rect_vs),
                         nullptr, vertex_shader_.GetAddressOf()), "create vertex shader") &&
           succeeded(device_->CreatePixelShader(d3d11_rect_ps, sizeof(d3d11_rect_ps),
                         nullptr, pixel_shader_.GetAddressOf()), "create pixel shader") &&
           succeeded(device_->CreateRasterizerState(&rasterizer, rasterizer_.GetAddressOf()),
                         "create rasterizer state") &&
           succeeded(device_->CreateBlendState(&blend, blend_.GetAddressOf()), "create blend state") &&
           succeeded(device_->CreateBuffer(&color, nullptr, color_.GetAddressOf()),
                         "create constant buffer");
}

bool D3d11Surface::resize(int width, int height)
{
    context_->OMSetRenderTargets(0, nullptr, nullptr);
    target_.Reset();
    if (!succeeded(swap_chain_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0),
                   "resize swap chain")) {
        return false;
    }
    ComPtr<ID3D11Texture2D> back_buffer;
    if (!succeeded(swap_chain_->GetBuffer(0, IID_PPV_ARGS(back_buffer.GetAddressOf())),
                   "get back buffer") ||
        !succeeded(device_->CreateRenderTargetView(back_buffer.Get(), nullptr,
                   target_.GetAddressOf()), "create render target")) {
        return false;
    }
    width_ = width;
    height_ = height;
    return true;
}

bool D3d11Surface::present(int width, int height, std::span<const FillRectCommand> commands)
{
    if (width <= 0 || height <= 0) {
        return true;
    }
    if ((width != width_ || height != height_) && !resize(width, height)) {
        return false;
    }

    if (std::any_of(commands.begin(),commands.end(),[](const auto& c){ return c.software_layer; })) {
        // A reference-composited frame has premultiplied channels. Replace the
        // back buffer directly so translucent pixels are not blended twice.
        std::vector<uint8_t> pixels;
        rasterize(pixels,width,height,commands,PixelOrder::Bgra);
        ComPtr<ID3D11Resource> buffer;
        target_->GetResource(buffer.GetAddressOf());
        context_->OMSetRenderTargets(0,nullptr,nullptr);
        context_->UpdateSubresource(buffer.Get(),0,nullptr,pixels.data(),UINT(width)*4,0);
    } else {
        const D3D11_VIEWPORT viewport{0, 0, static_cast<float>(width), static_cast<float>(height), 0, 1};
        context_->OMSetRenderTargets(1, target_.GetAddressOf(), nullptr);
        context_->RSSetViewports(1, &viewport);
        context_->RSSetState(rasterizer_.Get());
        context_->OMSetBlendState(blend_.Get(), nullptr, 0xffffffffu);
        context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context_->VSSetShader(vertex_shader_.Get(), nullptr, 0);
        context_->PSSetShader(pixel_shader_.Get(), nullptr, 0);
        context_->PSSetConstantBuffers(0, 1, color_.GetAddressOf());
        for (const auto& rect : commands) {
            const int left = std::clamp(rect.left, 0, width);
            const int right = std::clamp(rect.right, left, width);
            const int top = std::clamp(rect.top, 0, height);
            const int bottom = std::clamp(rect.bottom, top, height);
            if (left == right || top == bottom) {
                continue;
            }
            const D3D11_RECT clip{left, top, right, bottom};
            const float color[]{rect.color[0] / 255.0f, rect.color[1] / 255.0f,
                                rect.color[2] / 255.0f, rect.color[3] / 255.0f};
            context_->RSSetScissorRects(1, &clip);
            context_->UpdateSubresource(color_.Get(), 0, nullptr, color, 0, 0);
            context_->Draw(3, 0);
        }
    }

#ifdef SUNFLECK_TEST_D3D11
    if (std::getenv("SUNFLECK_TEST_D3D11_CAPTURE")) {
        ComPtr<ID3D11Texture2D> buffer, staging;
        if (!succeeded(swap_chain_->GetBuffer(0,IID_PPV_ARGS(buffer.GetAddressOf())),"capture buffer")) return false;
        D3D11_TEXTURE2D_DESC desc{};
        buffer->GetDesc(&desc);
        desc.Usage=D3D11_USAGE_STAGING; desc.BindFlags=0; desc.MiscFlags=0; desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        if (!succeeded(device_->CreateTexture2D(&desc,nullptr,&staging),"create capture texture")) return false;
        context_->CopyResource(staging.Get(),buffer.Get());
        captured.resize(size_t(width)*height*4);
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (!succeeded(context_->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped),"map capture")) return false;
        for(int y=0;y<height;++y) {
            const auto* row=static_cast<const uint8_t*>(mapped.pData)+size_t(y)*mapped.RowPitch;
            for(int x=0;x<width;++x) {
                const auto i=(size_t(y)*width+x)*4;
                captured[i]=row[4*x+2]; captured[i+1]=row[4*x+1];
                captured[i+2]=row[4*x]; captured[i+3]=row[4*x+3];
            }
        }
        context_->Unmap(staging.Get(),0);
    }
#endif
    const auto result = presented_ && inject("present")
        ? DXGI_ERROR_DEVICE_REMOVED : swap_chain_->Present(1, 0);
    if (!succeeded(result, "present")) {
        return false;
    }
    presented_ = result != DXGI_STATUS_OCCLUDED;
    return true;
}
