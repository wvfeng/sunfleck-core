#pragma once
#include <dxgi1_4.h>
#include <iomanip>
#include <iostream>

inline void printD3D12Adapter(IDXGIAdapter1* adapter, const DXGI_ADAPTER_DESC1& desc) {
    LARGE_INTEGER driver{};
    const auto driverStatus = adapter->CheckInterfaceSupport(__uuidof(IDXGIDevice), &driver);
    std::wcout << L"adapter=" << desc.Description
               << L" software=" << bool(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
               << L" vendor=0x" << std::hex << desc.VendorId
               << L" device=0x" << desc.DeviceId
               << L" luid=" << std::setfill(L'0') << std::setw(8)
               << static_cast<unsigned long>(desc.AdapterLuid.HighPart)
               << L':' << std::setw(8) << desc.AdapterLuid.LowPart
               << std::setfill(L' ') << std::dec;
    if (SUCCEEDED(driverStatus)) {
        std::wcout << L" driver=" << HIWORD(driver.HighPart) << L'.' << LOWORD(driver.HighPart)
                   << L'.' << HIWORD(driver.LowPart) << L'.' << LOWORD(driver.LowPart);
    } else {
        std::wcout << L" driverQueryHRESULT=0x" << std::hex
                   << static_cast<unsigned long>(driverStatus) << std::dec;
    }
}
