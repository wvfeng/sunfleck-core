#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <iostream>
#include "d3d12-adapter.h"
using Microsoft::WRL::ComPtr;
int main() {
    ComPtr<IDXGIFactory4> factory;
    HRESULT hr=CreateDXGIFactory2(0,IID_PPV_ARGS(&factory));
    if(FAILED(hr)){std::cout<<"CreateDXGIFactory2 HRESULT="<<std::hex<<hr<<'\n';return 1;}
    unsigned supported=0;
    for(UINT i=0;;++i) {
        ComPtr<IDXGIAdapter1> adapter;
        hr=factory->EnumAdapters1(i,&adapter);
        if(hr==DXGI_ERROR_NOT_FOUND)break;
        if(FAILED(hr))return 1;
        DXGI_ADAPTER_DESC1 desc{};
        if(FAILED(adapter->GetDesc1(&desc)))return 1;
        ComPtr<ID3D12Device> device;
        hr=D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device));
        printD3D12Adapter(adapter.Get(),desc);
        std::wcout<<L" D3D12 HRESULT=0x"<<std::hex<<static_cast<unsigned long>(hr)<<std::dec<<L'\n';
        if(SUCCEEDED(hr) && !(desc.Flags&DXGI_ADAPTER_FLAG_SOFTWARE))++supported;
    }
    ComPtr<IDXGIAdapter> warp;ComPtr<ID3D12Device> device;
    hr=factory->EnumWarpAdapter(IID_PPV_ARGS(&warp));
    if(SUCCEEDED(hr))hr=D3D12CreateDevice(warp.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device));
    std::cout<<"hardwareAdapters="<<std::dec<<supported<<" WARP D3D12 HRESULT=0x"<<std::hex<<static_cast<unsigned long>(hr)<<'\n';
    return FAILED(hr)?1:0;
}
