// Owned-device test of manual initialization after native device creation.
// No game access, queues, swap chains, presentation, sleep or frame markers.
#include "../src/reflex/streamline.hpp"
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
void hr(HRESULT result, const char* why) { require(SUCCEEDED(result), why); }
void api(sl::Result result, const char* why) {
    if (result != sl::Result::eOk)
        throw std::runtime_error(std::string(why)+":"+std::to_string(static_cast<unsigned>(result)));
}
void run(int argc, wchar_t** argv) {
    require(argc == 3, "usage: reflex_startup_fixture runtime_directory new_output_directory");
    const auto output = std::filesystem::absolute(argv[2]);
    require(!std::filesystem::exists(output), "preserve previous fixture evidence");
    std::filesystem::create_directories(output);
    std::ofstream log(output/L"events.jsonl", std::ios::binary);
    require(bool(log), "fixture log"); log << std::boolalpha;
    log << "{\"event\":\"start\",\"schema\":1,\"experiment\":\"manual-init-after-native-device\","
           "\"sdk\":\"2.12.0\",\"game_access\":false,\"queues\":0,\"swapchains\":0,\"sleep_calls\":0,\"markers\":0}\n" << std::flush;
    using Microsoft::WRL::ComPtr;
    ComPtr<IDXGIFactory4> factory; ComPtr<IDXGIAdapter1> adapter; ComPtr<ID3D12Device> device;
    // SDK destruction/shutdown precedes release of its borrowed native device.
    mhw::reflex::Streamline sdk;
    hr(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory)), "native factory");
    for (UINT index = 0; SUCCEEDED(factory->EnumAdapters1(index,&adapter)); ++index) {
        DXGI_ADAPTER_DESC1 description{}; hr(adapter->GetDesc1(&description), "adapter description");
        if (description.VendorId == 0x10de && !(description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) break;
        adapter.Reset();
    }
    require(adapter != nullptr, "fixture NVIDIA adapter");
    hr(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device)), "native device");
    log << "{\"event\":\"native_device_created\",\"feature_level\":" << static_cast<unsigned>(D3D_FEATURE_LEVEL_12_0)
        << ",\"sdk_initialized\":false}\n" << std::flush;
    sdk.initialize(std::filesystem::canonical(argv[1]),output/L"sdk-logs");
    log << "{\"event\":\"sdk_initialized\",\"after_native_device_creation\":true,"
           "\"before_native_queues_and_swapchain\":true}\n" << std::flush;
    auto luid = device->GetAdapterLuid();
    sl::AdapterInfo info{}; info.deviceLUID = reinterpret_cast<std::uint8_t*>(&luid); info.deviceLUIDSizeInBytes = sizeof(luid);
    api(sdk.supported(sl::kFeatureReflex,info), "Reflex support");
    api(sdk.supported(sl::kFeaturePCL,info), "PCL support");
    api(sdk.attach(device.Get()), "native device attachment");
    api(sdk.options(sl::ReflexMode::eOff), "Reflex Off");
    sl::ReflexState state{}; api(sdk.state(state), "Reflex state");
    require(state.lowLatencyAvailable, "fixture Reflex capability");
    log << "{\"event\":\"device_checked\",\"mode\":0,\"low_latency_available\":" << state.lowLatencyAvailable
        << ",\"luid_low\":" << luid.LowPart << ",\"luid_high\":" << luid.HighPart << "}\n" << std::flush;
    api(sdk.shutdown(), "SDK shutdown");
    hr(device->GetDeviceRemovedReason(), "device health after SDK shutdown");
    log << "{\"event\":\"sdk_shutdown\",\"passed\":true,\"device_healthy\":true}\n" << std::flush;
    adapter.Reset(); factory.Reset();
    auto* native = device.Detach(); const auto references = native->Release();
    require(references == 0, "native device final references");
    log << "{\"event\":\"finished\",\"passed\":true,\"remaining_device_refs\":" << references
        << ",\"queues\":0,\"swapchains\":0,\"sleep_calls\":0,\"markers\":0}\n" << std::flush;
    require(bool(log), "fixture final log");
}
}
int wmain(int argc, wchar_t** argv) {
    try { run(argc,argv); return 0; }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
