#pragma once
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <stdexcept>

namespace mhw::reflex {
// D3D12CreateDevice's native adapter belongs to the factory that enumerated it.
// GetParent returns an owned factory reference before any renderer global is
// published. Do not create a second factory or wait for publication here.
inline Microsoft::WRL::ComPtr<IDXGIFactory2> adapter_factory(IUnknown* native_adapter) {
    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    if (!native_adapter || FAILED(native_adapter->QueryInterface(IID_PPV_ARGS(&adapter))))
        throw std::runtime_error("startup-native-adapter-interface");
    Microsoft::WRL::ComPtr<IDXGIFactory2> factory;
    if (FAILED(adapter->GetParent(IID_PPV_ARGS(&factory))) || !factory)
        throw std::runtime_error("startup-native-adapter-parent");
    return factory;
}
}
