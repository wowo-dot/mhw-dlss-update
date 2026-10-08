// RestoreBindings on a graphics-only recording (WARP, debug layer): the FG
// HUD-less pass runs compute on a list that never set a compute root signature
// (a worker's final-scene list), then restores it by unbinding the compute
// signature. The game's next draw must validate and the CPU mirror (fed the
// same calls, as the observer is) must equal the snapshot again.
#include "../src/temporal_present/bindings.hpp"
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <d3dcompiler.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <span>
#include <stdexcept>
using Microsoft::WRL::ComPtr;
namespace tp = mhw::temporal_present;
void check(bool value, const char *why) { if (!value) { std::cerr << why << '\n'; std::exit(1); } }
ComPtr<ID3DBlob> compile(const char *source, const char *target) {
    ComPtr<ID3DBlob> code, errors;
    check(SUCCEEDED(D3DCompile(source, std::strlen(source), nullptr, nullptr, nullptr, "main", target, 0, 0, &code, &errors)), target);
    return code;
}
ComPtr<ID3D12RootSignature> root(ID3D12Device *device, D3D12_ROOT_SIGNATURE_FLAGS flags) {
    D3D12_ROOT_PARAMETER p{}; p.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS; p.Constants = {0, 0, 1};
    p.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    D3D12_ROOT_SIGNATURE_DESC d{1, &p, 0, nullptr, flags}; ComPtr<ID3DBlob> blob, errors; ComPtr<ID3D12RootSignature> r;
    check(SUCCEEDED(D3D12SerializeRootSignature(&d, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &errors)) &&
          SUCCEEDED(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&r))), "root signature");
    return r;
}
ComPtr<ID3D12DescriptorHeap> heap(ID3D12Device *device, D3D12_DESCRIPTOR_HEAP_TYPE type, bool visible) {
    D3D12_DESCRIPTOR_HEAP_DESC d{type, 1, visible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 0};
    ComPtr<ID3D12DescriptorHeap> h; check(SUCCEEDED(device->CreateDescriptorHeap(&d, IID_PPV_ARGS(&h))), "descriptor heap"); return h;
}
std::uintptr_t id(IUnknown *p) { return reinterpret_cast<std::uintptr_t>(p); }
int main() {
    ComPtr<ID3D12Debug> debug; check(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))), "debug interface"); debug->EnableDebugLayer();
    ComPtr<IDXGIFactory4> factory; ComPtr<IDXGIAdapter> warp; ComPtr<ID3D12Device> device;
    check(SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))) && SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp))) &&
          SUCCEEDED(D3D12CreateDevice(warp.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device))), "WARP device");
    ComPtr<ID3D12InfoQueue> info; check(SUCCEEDED(device.As(&info)), "info queue");
    // The game's graphics state: root signature with one root constant, a PSO, one heap.
    const auto game_root = root(device.Get(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    const auto vs = compile("float4 main(uint i : SV_VertexID) : SV_Position { return float4((i & 1) * 4 - 1, (i >> 1) * 4 - 1, 0, 1); }", "vs_5_0");
    const auto ps = compile("float4 main() : SV_Target { return 1; }", "ps_5_0");
    D3D12_GRAPHICS_PIPELINE_STATE_DESC g{}; g.pRootSignature = game_root.Get();
    g.VS = {vs->GetBufferPointer(), vs->GetBufferSize()}; g.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
    g.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL; g.SampleMask = UINT_MAX;
    g.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID; g.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; g.RasterizerState.DepthClipEnable = TRUE;
    g.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; g.NumRenderTargets = 1; g.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM; g.SampleDesc.Count = 1;
    ComPtr<ID3D12PipelineState> game_pso; check(SUCCEEDED(device->CreateGraphicsPipelineState(&g, IID_PPV_ARGS(&game_pso))), "game PSO");
    // Our compute pass: its own root signature, PSO and heap.
    const auto our_root = root(device.Get(), D3D12_ROOT_SIGNATURE_FLAG_NONE);
    const auto cs = compile("[numthreads(1,1,1)] void main() {}", "cs_5_0");
    D3D12_COMPUTE_PIPELINE_STATE_DESC c{}; c.pRootSignature = our_root.Get(); c.CS = {cs->GetBufferPointer(), cs->GetBufferSize()};
    ComPtr<ID3D12PipelineState> our_pso; check(SUCCEEDED(device->CreateComputePipelineState(&c, IID_PPV_ARGS(&our_pso))), "our PSO");
    const auto game_heap = heap(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true), our_heap = heap(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);
    const auto rtv_heap = heap(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, false);
    D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC rd{}; rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; rd.Width = rd.Height = 8; rd.DepthOrArraySize = rd.MipLevels = 1;
    rd.Format = DXGI_FORMAT_R8G8B8A8_UNORM; rd.SampleDesc.Count = 1; rd.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    ComPtr<ID3D12Resource> target; check(SUCCEEDED(device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_RENDER_TARGET, nullptr, IID_PPV_ARGS(&target))), "target");
    const auto rtv = rtv_heap->GetCPUDescriptorHandleForHeapStart(); device->CreateRenderTargetView(target.Get(), nullptr, rtv);
    ComPtr<ID3D12CommandAllocator> allocator; ComPtr<ID3D12GraphicsCommandList> list; ComPtr<ID3D12CommandQueue> queue; ComPtr<ID3D12Fence> fence;
    D3D12_COMMAND_QUEUE_DESC qd{}; qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    check(SUCCEEDED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator))) &&
          SUCCEEDED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), game_pso.Get(), IID_PPV_ARGS(&list))) &&
          SUCCEEDED(device->CreateCommandQueue(&qd, IID_PPV_ARGS(&queue))) && SUCCEEDED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))), "list");
    const auto before = info->GetNumStoredMessages();

    // The game's recording up to its final-scene draw: graphics state only.
    ID3D12DescriptorHeap *game_heaps[]{game_heap.Get()}; list->SetDescriptorHeaps(1, game_heaps);
    list->SetGraphicsRootSignature(game_root.Get()); list->SetGraphicsRoot32BitConstant(0, 7, 0);
    tp::Bindings snapshot; snapshot.reset(1, id(game_pso.Get()));
    const std::uintptr_t game_heap_ids[]{id(game_heap.Get())}; snapshot.descriptor_heaps(game_heap_ids);
    snapshot.root(true, id(game_root.Get())); const std::uint32_t seven[]{7}; snapshot.constants(true, 0, 0, seven);
    check(!snapshot.ready() && snapshot.ready_graphics(), "graphics-only snapshot");
    bool refused = false; try { tp::RestoreBindings strict(list.Get(), snapshot); } catch (const std::runtime_error &) { refused = true; }
    check(refused, "strict restore still refuses a missing compute signature");

    // Our compute pass, then the restore. The mirror receives every call.
    auto mirror = snapshot;
    {
        tp::RestoreBindings restore(list.Get(), snapshot, true);
        ID3D12DescriptorHeap *ours[]{our_heap.Get()}; list->SetDescriptorHeaps(1, ours);
        list->SetComputeRootSignature(our_root.Get()); list->SetPipelineState(our_pso.Get());
        list->SetComputeRoot32BitConstant(0, 1, 0); list->Dispatch(1, 1, 1);
        const std::uintptr_t our_heap_ids[]{id(our_heap.Get())}; mirror.descriptor_heaps(our_heap_ids);
        mirror.root(false, id(our_root.Get())); mirror.pipeline = id(our_pso.Get()); const std::uint32_t one[]{1}; mirror.constants(false, 0, 0, one);
        check(!(mirror == snapshot), "the pass changed the mirror");
        restore.apply();
        // apply() order: heaps, compute signature (null), graphics signature, PSO, arguments.
        mirror.descriptor_heaps(game_heap_ids); mirror.root(false, 0); mirror.root(true, id(game_root.Get()));
        mirror.pipeline = id(game_pso.Get()); mirror.constants(true, 0, 0, seven);
        check(mirror == snapshot, "the mirror matches the snapshot after restore");
    }
    // The game continues with its restored graphics state.
    list->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
    const D3D12_VIEWPORT viewport{0, 0, 8, 8, 0, 1}; const D3D12_RECT scissor{0, 0, 8, 8};
    list->RSSetViewports(1, &viewport); list->RSSetScissorRects(1, &scissor); list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    list->DrawInstanced(3, 1, 0, 0);
    check(SUCCEEDED(list->Close()), "close");
    ID3D12CommandList *lists[]{list.Get()}; queue->ExecuteCommandLists(1, lists);
    check(SUCCEEDED(queue->Signal(fence.Get(), 1)), "signal");
    const auto event = CreateEventW(nullptr, FALSE, FALSE, nullptr); check(event && SUCCEEDED(fence->SetEventOnCompletion(1, event)), "fence event");
    WaitForSingleObject(event, 10000); CloseHandle(event);
    check(fence->GetCompletedValue() == 1 && device->GetDeviceRemovedReason() == S_OK, "GPU completion");
    const auto after = info->GetNumStoredMessages(); unsigned problems = 0;
    for (auto i = before; i < after; ++i) {
        SIZE_T size = 0; info->GetMessage(i, nullptr, &size); auto *m = static_cast<D3D12_MESSAGE *>(std::malloc(size));
        if (m && SUCCEEDED(info->GetMessage(i, m, &size)) && m->Severity <= D3D12_MESSAGE_SEVERITY_WARNING) {
            ++problems; std::cerr << "debug layer: " << m->pDescription << '\n'; }
        std::free(m);
    }
    check(problems == 0, "no debug-layer warnings or errors");
    std::cout << "PASS graphics-only restore: compute signature unbound, mirror equal, game draw valid on WARP\n";
}
