// Included in the owned recorder fixture's anonymous namespace.
unsigned device_refs{};bool query_lock_ok=true;
ULONG STDMETHODCALLTYPE device_addref(IUnknown *){return ++device_refs;}
ULONG STDMETHODCALLTYPE device_release(IUnknown *){return --device_refs;}
HRESULT STDMETHODCALLTYPE device_query(IUnknown *,REFIID,void **){return E_NOINTERFACE;}
ID3D12Resource *other_texture();
void unlocked_query() {
  const bool acquired=pb::hold_owned(pb::consume,false);query_lock_ok=query_lock_ok && acquired;
  if(acquired)pb::release_owned();
}
class OwnedTexture final : public ID3D12Resource {
public:
  class Identity final : public IUnknown {
    OwnedTexture &owner;
  public:
    explicit Identity(OwnedTexture &value):owner(value){}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void **out)override {
      unlocked_query();*out=nullptr;
      if(iid==__uuidof(IUnknown)) {
        if(mode=="attach_identity_self_hr")return E_NOINTERFACE;
        *out=mode=="attach_identity_mismatch"?&owner.alternate:this;AddRef();return S_OK;
      }
      if(iid==__uuidof(ID3D12Resource)) {
        if(mode=="attach_identity_roundtrip_hr")return E_NOINTERFACE;
        if(mode=="attach_identity_roundtrip")return other_texture()->QueryInterface(iid,out);
        return owner.QueryInterface(iid,out);
      }
      return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef()override{return owner.AddRef();}
    ULONG STDMETHODCALLTYPE Release()override{return owner.Release();}
  } canonical{*this},alternate{*this};
  unsigned refs=1,descriptions{},unknown_queries{};DXGI_FORMAT format=DXGI_FORMAT_R32_TYPELESS;
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void **out) override {
    unlocked_query();
    *out=nullptr;if(mode=="attach_interface" || (iid!=__uuidof(IUnknown) && iid!=__uuidof(ID3D12Resource)))return E_NOINTERFACE;
    if(iid==__uuidof(IUnknown)) {
      ++unknown_queries;
      if(mode=="attach_identity_hr" || (mode=="attach_identity_partial_fail" && this!=other_texture()))return E_NOINTERFACE;
      if(mode=="attach_identity_null")return S_OK;
      if(mode=="attach_identity_exception")RaiseException(0xe0123456,0,0,nullptr);
      const bool changed=(mode=="attach_identity_roundtrip_mismatch" && unknown_queries==2) ||
        (mode=="attach_identity_draw_change" && unknown_queries>=3) ||
        (mode=="attach_identity_return_change" && unknown_queries>=5);
      *out=mode.starts_with("attach_identity_")?static_cast<void *>(changed?&alternate:&canonical):static_cast<void *>(this);
    } else *out=this;
    AddRef();return S_OK;
  }
  ULONG STDMETHODCALLTYPE AddRef() override{return ++refs;}
  ULONG STDMETHODCALLTYPE Release() override{return --refs;}
  HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID,UINT *,void *)override{return E_NOTIMPL;}
  HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID,UINT,const void *)override{return E_NOTIMPL;}
  HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID,const IUnknown *)override{return E_NOTIMPL;}
  HRESULT STDMETHODCALLTYPE SetName(LPCWSTR)override{return E_NOTIMPL;}
  HRESULT STDMETHODCALLTYPE GetDevice(REFIID,void **out)override {
    static U other{};other=device_object;
    *out=mode=="attach_device"?reinterpret_cast<void *>(&other):reinterpret_cast<void *>(&device_object);
    ++device_refs;return S_OK;
  }
  HRESULT STDMETHODCALLTYPE Map(UINT,const D3D12_RANGE *,void **)override{return E_NOTIMPL;}
  void STDMETHODCALLTYPE Unmap(UINT,const D3D12_RANGE *)override{}
  D3D12_RESOURCE_DESC STDMETHODCALLTYPE GetDesc()override {
    unlocked_query();
    ++descriptions;
    D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;d.Width=2560;d.Height=1440;
    d.DepthOrArraySize=1;d.MipLevels=1;d.Format=format;d.SampleDesc.Count=1;d.Flags=D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    if(mode=="attach_desc_changed" && descriptions>1)d.Width=1920;
    return d;
  }
  D3D12_GPU_VIRTUAL_ADDRESS STDMETHODCALLTYPE GetGPUVirtualAddress()override{return 0;}
  HRESULT STDMETHODCALLTYPE WriteToSubresource(UINT,const D3D12_BOX *,const void *,UINT,UINT)override{return E_NOTIMPL;}
  HRESULT STDMETHODCALLTYPE ReadFromSubresource(void *,UINT,UINT,UINT,const D3D12_BOX *)override{return E_NOTIMPL;}
  HRESULT STDMETHODCALLTYPE GetHeapProperties(D3D12_HEAP_PROPERTIES *,D3D12_HEAP_FLAGS *)override{return E_NOTIMPL;}
} resource0,resource1;
ID3D12Resource *other_texture(){return &resource1;}
std::array<std::byte,0x80> attachment_packet{},attachment_block{},attachment_graphics{},attachment_render{};
std::array<std::byte,0x100> color_owner{},depth_owner{},color_back{},depth_back{},raster_owner{},raster_back{},depth_state_owner{},depth_state_back{},blend_owner{};
std::array<std::byte,0x180> blend_back{};
std::vector<std::byte> default_renderer(0x2500);
U default_depth{};
void setup_attachments() {
  device_vt[0]=reinterpret_cast<U>(&device_query);device_vt[1]=reinterpret_cast<U>(&device_addref);device_vt[2]=reinterpret_cast<U>(&device_release);
  check(VirtualAlloc(reinterpret_cast<void *>(image+0x51c7000),4096,MEM_COMMIT,PAGE_READWRITE)!=nullptr,"default renderer global");
  default_depth=address(depth_state_owner);put(image,0x51c73d0,address(default_renderer));put(address(default_renderer),0x2480,reinterpret_cast<U>(&default_depth));
  put(address(attachment_packet),0x10,address(attachment_block));put(address(attachment_packet),0x20,address(attachment_graphics));
  put(address(attachment_packet),0x28,address(attachment_render));
  put(address(attachment_packet),0x38,mode=="attach_created" || mode.starts_with("attach_create_")?U{}:U{0x11110000});
  put(address(attachment_graphics),8,reinterpret_cast<U>(&shader_set));put(address(attachment_graphics),0,12U);
  put(address(attachment_render),8,address(blend_owner));put(address(attachment_render),0x10,address(depth_state_owner));
  put(address(attachment_render),0x18,address(raster_owner));
  put(address(depth_state_owner),0x60,address(depth_state_back));put(address(blend_owner),0x98,address(blend_back));
  put(address(raster_owner),0x68,address(raster_back));put(address(depth_state_back),0x10,1U);put(address(depth_state_back),0x14,1U);put(address(depth_state_back),0x18,3U);
  put(address(depth_owner),0x60,address(depth_back));put(address(depth_owner),0x74,39U);
  put(address(depth_back),0x10,U{0x9000});put(address(depth_back),0x18,reinterpret_cast<U>(&resource0));
  put(address(color_owner),0x60,address(color_back));put(address(color_back),0x10,U{0x8000});put(address(color_back),0x20,28U);
  put(address(color_back),0x18,reinterpret_cast<U>(&resource1));resource1.format=DXGI_FORMAT_R8G8B8A8_UNORM;
  const bool colors=mode=="attach_color_depth" || mode=="attach_no_depth" || mode=="attach_rotation" || mode=="attach_null_color" || mode=="attach_identity_partial_fail";
  if(colors) {put(address(attachment_block),0x48,mode=="attach_null_color"?2U:1U);if(mode!="attach_null_color")put(address(attachment_block),0,address(color_owner));}
  if(mode!="attach_no_depth")put(address(attachment_block),0x40,address(depth_owner));
  if(mode=="attach_rotation")put<unsigned char>(address(color_back),0x28,1);
  if(mode=="attach_cached" || mode=="attach_same_handles") {
    put(address(state),0xd8,U{0x9000});put(address(state),0x140,reinterpret_cast<U>(&resource0));
    put(address(backend_context),0x10,mode=="attach_cached"?address(attachment_block):U{0x8888});
  }
}
U attachment_body(U a,U b,U c,U d,U,U,U,U kind) {
  if(kind==pb::draw0) {
    const U cm=reinterpret_cast<U>(&command_object);const U st=address(state);
    U depth{};std::memcpy(&depth,attachment_block.data()+0x40,8);
    unsigned count{};std::memcpy(&count,attachment_block.data()+0x48,4);count&=7;
    std::array<U,8> handles{};if(count && mode!="attach_null_color")handles[0]=0x8000;
    U dsv=depth?0x9000:0;
    if(mode!="attach_cached" && mode!="attach_same_handles") {
      const auto passed=count+(mode=="attach_count"?1:0);
      invoke(pb::output_binding,cm,passed,address(handles),0,depth?reinterpret_cast<U>(&dsv):0);
    }
    put(st,0x98,handles);put(st,0xd8,dsv);put(st,0x140,depth?reinterpret_cast<U>(&resource0):U{0xdeadbeef});
    put(address(backend_context),0x10,address(attachment_block));
    put(st,0xe0,std::array<int,4>{0,0,2560,1440});put(st,0xf0,std::array<int,4>{0,0,2560,1440});
    const std::array<float,6> vp={0,0,2560,1440,0,1};const std::array<int,4> sc={0,0,2560,1440};
    invoke(pb::viewport,cm,1,reinterpret_cast<U>(vp.data()));invoke(pb::scissor,cm,1,reinterpret_cast<U>(sc.data()));
    invoke(pb::state_setup,a,b,address(attachment_graphics),c);
    if(mode=="attach_state_changed")put(address(depth_state_back),0x14,0U);
    if(mode=="attach_pointer")put(address(depth_back),0x18,reinterpret_cast<U>(&resource1));
    if(mode=="attach_reset")invoke(pb::reset,cm);
    if(mode=="attach_pso_changed")put(st,0x108,U{0x9999});
    D3D12_RESOURCE_BARRIER barrier{};barrier.Transition.pResource=&resource0;barrier.Transition.Subresource=UINT_MAX;
    barrier.Transition.StateBefore=D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;barrier.Transition.StateAfter=D3D12_RESOURCE_STATE_DEPTH_WRITE;
    if(mode=="attach_split")barrier.Flags=D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY;
    invoke(pb::barriers,cm,mode=="attach_overflow"?65:1,reinterpret_cast<U>(&barrier));
    if(mode=="attach_total_overflow")for(unsigned j=0;j<64;++j)invoke(pb::barriers,cm,1,reinterpret_cast<U>(&barrier));
    invoke(pb::api_draw,cm,3,1,0,0);return 1;
  }
  if(kind==pb::state_setup) {
    if(mode=="attach_created" || mode.starts_with("attach_create_")) {
      D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};desc.pRootSignature=reinterpret_cast<ID3D12RootSignature *>(0x22220000);
      desc.VS={blob.data(),blob.size()};desc.DSVFormat=DXGI_FORMAT_D32_FLOAT;desc.SampleDesc.Count=1;
      std::memcpy(&desc.DepthStencilState,depth_state_back.data()+0x10,sizeof(desc.DepthStencilState));
      GUID iid=mode=="attach_create_iid"?__uuidof(ID3D12Resource):__uuidof(ID3D12PipelineState);U out{};
      if(mode=="attach_create_bytes")desc.VS.BytecodeLength=40;
      invoke(pb::create_pipeline,rt.device,reinterpret_cast<U>(&desc),reinterpret_cast<U>(&iid),reinterpret_cast<U>(&out));
    }
    invoke(pb::pipeline,reinterpret_cast<U>(&command_object),0x11110000);
    invoke(pb::root_signature,reinterpret_cast<U>(&command_object),0x22220000);
    invoke(pb::root_cbv,reinterpret_cast<U>(&command_object),2,gpu);
    return 1;
  }
  if(kind==pb::create_pipeline) {
    put(d,0,mode=="attach_create_output"?U{0x9999}:U{0x11110000});
    if(mode=="attach_create_changed")put(b,0,U{0x9999});
    return mode=="attach_create_hr"?static_cast<U>(E_FAIL):S_OK;
  }
  if(kind==pb::api_draw && mode=="attach_exception")RaiseException(0xe0123456,0,0,nullptr);
  return 1;
}
bool attachment_exception() {
  __try {invoke(pb::draw0,address(executor),address(backend_context),address(attachment_packet));}
  __except(GetExceptionCode()==0xe0123456?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){return true;}
  return false;
}
void resource_sdk(const char *output) {
  using Microsoft::WRL::ComPtr;
  ComPtr<ID3D12Device> device;
  check(SUCCEEDED(D3D12CreateDevice(nullptr,D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device))),"owned SDK device");
  struct Pins {~Pins(){pb::release_attachment_resources();}} cleanup;
  std::ostringstream report;report<<std::boolalpha;
  const auto luid=device->GetAdapterLuid();
  report<<"{\"game_process_opened\":false,\"gpu_commands_added\":0,\"production_validator_used\":true,\"adapter_luid_low\":"
    <<luid.LowPart<<",\"adapter_luid_high\":"<<luid.HighPart<<",\"resources\":[";
  for(unsigned i=0;i<2;++i) {
    D3D12_HEAP_PROPERTIES heap{};heap.Type=i?D3D12_HEAP_TYPE_DEFAULT:D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC requested{};requested.Dimension=i?D3D12_RESOURCE_DIMENSION_TEXTURE2D:D3D12_RESOURCE_DIMENSION_BUFFER;
    requested.Width=i?16:4096;requested.Height=i?16:1;requested.DepthOrArraySize=1;requested.MipLevels=1;
    requested.Format=i?DXGI_FORMAT_R32_TYPELESS:DXGI_FORMAT_UNKNOWN;requested.SampleDesc.Count=1;
    requested.Layout=i?D3D12_TEXTURE_LAYOUT_UNKNOWN:D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    requested.Flags=i?D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL:D3D12_RESOURCE_FLAG_NONE;
    ComPtr<ID3D12Resource> resource;
    check(SUCCEEDED(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&requested,
      i?D3D12_RESOURCE_STATE_DEPTH_WRITE:D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&resource))),"owned SDK resource");
    pb::AttachmentCapture c{};c.ready.views[8].resource=reinterpret_cast<U>(resource.Get());
    const bool accepted=pb::describe_attachments(c,reinterpret_cast<U>(device.Get()));
    check(accepted==(i!=0),"owned SDK qualification");
    if(accepted)check(pb::recheck_resources(c,1) && pb::recheck_resources(c,2),"owned SDK rechecks");
    check(pb::release_attachment_resources(),"owned SDK release");
    const auto &q=c.resource_checks[0][0];
    check(q.checkpoint==(i?15U:13U) && q.temporaries_released,"owned SDK checkpoint");
    if(i)report<<',';
    report<<"{\"texture\":"<<(i!=0)<<",\"accepted\":"<<accepted<<",\"checkpoint\":"<<q.checkpoint
      <<",\"identity_differs\":"<<(q.resource!=q.identity)<<",\"roundtrip_matches\":"<<(q.resource==q.roundtrip)
      <<",\"canonical_stable\":"<<(q.identity==q.identity_repeat && q.identity==q.roundtrip_identity)
      <<",\"queries_released\":"<<q.temporaries_released<<",\"recheck_count\":"<<(c.resource_check_counts[1]+c.resource_check_counts[2])<<'}';
  }
  report<<"]}\n";
  if(output) {
    check(!std::filesystem::exists(output),"preserve SDK report");std::ofstream file(output,std::ios::binary);file<<report.str();check(static_cast<bool>(file),"SDK report write");
  }
  std::cout<<report.str();
}
