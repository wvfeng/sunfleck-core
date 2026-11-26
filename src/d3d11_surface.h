#pragma once
#include "gpu_surface.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <d3d11.h>
#include <wrl/client.h>

class D3d11Surface final : public GpuSurface {
public:
    bool open(HWND window);
#ifdef SUNFLECK_TEST_D3D11
    std::vector<uint8_t> captured;
#endif
    bool present(int width, int height, std::span<const FillRectCommand> commands) override;

private:
    bool resize(int width, int height);

    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<IDXGISwapChain> swap_chain_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> target_;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertex_shader_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixel_shader_;
    Microsoft::WRL::ComPtr<ID3D11BlendState> blend_;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizer_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> color_;
    int width_ = 0;
    int height_ = 0;
    bool presented_ = false;
};