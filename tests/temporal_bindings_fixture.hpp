// Included by the recorder fixture after its common helpers. Exercise the
// production Reset->binding observer->consumer handshake on a real WARP list.
#include "../src/temporal_present/observer.hpp"
struct NativeBindingsFixture {
  ComPtr<ID3D12RootSignature> root;
  ComPtr<ID3D12PipelineState> pipeline;
  ComPtr<ID3D12DescriptorHeap> heap;
  explicit NativeBindingsFixture(ID3D12Device *device) {
    D3D12_DESCRIPTOR_RANGE range{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,1,0,0,0};
    D3D12_ROOT_PARAMETER args[2]{};args[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;args[0].DescriptorTable={1,&range};
    args[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;args[1].Constants={0,0,2};
    D3D12_ROOT_SIGNATURE_DESC desc{2,args,0,nullptr,D3D12_ROOT_SIGNATURE_FLAG_NONE};ComPtr<ID3DBlob> bytes,errors,shader;
    check(SUCCEEDED(D3D12SerializeRootSignature(&desc,D3D_ROOT_SIGNATURE_VERSION_1,&bytes,&errors)),"state fixture root bytes");
    check(SUCCEEDED(device->CreateRootSignature(0,bytes->GetBufferPointer(),bytes->GetBufferSize(),IID_PPV_ARGS(&root))),"state fixture root");
    const char source[]="[numthreads(1,1,1)]void main(){}";
    check(SUCCEEDED(D3DCompile(source,sizeof(source)-1,nullptr,nullptr,nullptr,"main","cs_5_0",D3DCOMPILE_WARNINGS_ARE_ERRORS,0,&shader,&errors)),"state fixture shader");
    D3D12_COMPUTE_PIPELINE_STATE_DESC pd{};pd.pRootSignature=root.Get();pd.CS={shader->GetBufferPointer(),shader->GetBufferSize()};
    check(SUCCEEDED(device->CreateComputePipelineState(&pd,IID_PPV_ARGS(&pipeline))),"state fixture pipeline");
    D3D12_DESCRIPTOR_HEAP_DESC hd{D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,1,D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,0};
    check(SUCCEEDED(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&heap))),"state fixture heap");
    D3D12_SHADER_RESOURCE_VIEW_DESC view{};view.Format=DXGI_FORMAT_R32_UINT;view.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;
    view.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;view.Texture2D.MipLevels=1;
    device->CreateShaderResourceView(nullptr,&view,heap->GetCPUDescriptorHandleForHeapStart());
    const auto status=MH_Initialize();check(status==MH_OK||status==MH_ERROR_ALREADY_INITIALIZED,"state fixture MinHook");
    std::string reason;check(mhw::temporal_present::observer::install(device,mhw::projection_binding::system_method,reason),reason.c_str());
  }
  void bind(ID3D12GraphicsCommandList *list,std::uintptr_t backend,std::uintptr_t recording){
    list->ClearState(pipeline.Get());ID3D12DescriptorHeap *heaps[]{heap.Get()};list->SetDescriptorHeaps(1,heaps);
    list->SetComputeRootSignature(root.Get());list->SetComputeRootDescriptorTable(0,heap->GetGPUDescriptorHandleForHeapStart());
    list->SetComputeRoot32BitConstant(1,42,0);list->SetComputeRoot32BitConstant(1,17,1);
    put(backend+0x2a0,reinterpret_cast<std::uintptr_t>(root.Get()));put(recording+0x118,reinterpret_cast<std::uintptr_t>(root.Get()));
    put(recording+0x108,reinterpret_cast<std::uintptr_t>(pipeline.Get()));
  }
};
NativeBindingsFixture *native_bindings_fixture{};
