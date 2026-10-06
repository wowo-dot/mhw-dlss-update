#pragma once
// Real auxiliary textures and bounded synthetic native binding tables for the
// production haze admission/stability checks, independent of a game process.
struct StreamHazeFixture {
 using Arena=stream_plan_fixture::Arena;
 Arena arena{4096};
 std::array<fx::ComPtr<ID3D12Resource>,3> inputs;
 std::vector<fx::Pixels> uploads;
 std::uintptr_t set{},graphics{},color_owner{},color_backend{};
 explicit StreamHazeFixture(fx::Fixture &f){
  using namespace fx;const auto put=[](std::uintptr_t at,const auto &v){stream_plan_fixture::put(at,v);};
  set=arena.take(0x90);graphics=arena.take(0x90);const auto words=arena.take(64),ids=arena.take(64),modes=arena.take(16);
  put(set,words);put(set+8,std::array<unsigned short,2>{0,0});put(set+0x14,std::array<unsigned short,2>{9,0});
  put(set+0x80,ids);put(set+0x88,9U);put(graphics+0x10,modes);
  put(words,std::array<unsigned,9>{1060865,17440785,790562,17059890,35762242,52531282,45152,16826480,52543616});
  for(unsigned i=0;i<9;++i){put(ids+4*i,i+1);put(graphics+0x30+8*i,set);}
  const auto owner=[&](ID3D12Resource *resource,unsigned w,unsigned h,unsigned format,unsigned kind){
   const auto o=arena.take(0x100),b=arena.take(0x40),v=arena.take(0x20);put(o,std::uintptr_t{1});put(o+0x58,kind);put(o+0x60,b);
   put(o+0x78,v);put(v+0x10,v);put(o+0x88,w+(h<<16));put(o+0x90,format);put(b+0x20,reinterpret_cast<std::uintptr_t>(resource));
   if(!resource){color_owner=o;color_backend=b;}return o;};
  put(graphics+0x40,owner(nullptr,128,64,34,5));
  uploads.reserve(3);
  for(unsigned i=0;i<3;++i){
   auto desc=texture(i==0?128:i==1?32:1,i==0?64:i==1?32:1,i?DXGI_FORMAT_R8G8B8A8_UNORM:DXGI_FORMAT_R32_FLOAT);
   if(i==1){desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE3D;desc.DepthOrArraySize=32;}
   inputs[i]=resource(f.device.Get(),desc,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);
   const auto name=L"fixture-haze-aux-"+std::to_wstring(i);hr(inputs[i]->SetName(name.c_str()),"aux name");
   uploads.emplace_back(f.device.Get(),desc,D3D12_HEAP_TYPE_UPLOAD);
   std::vector<unsigned> values(static_cast<std::size_t>(desc.Width)*desc.Height*desc.DepthOrArraySize,i?0x80808080U:0x3f000000U);
   uploads.back().write(std::as_bytes(std::span(values)));
   put(graphics+0x48+8*i,owner(inputs[i].Get(),static_cast<unsigned>(desc.Width),desc.Height,i?7U:16U,i==1?1U:5U));
  }
 }
 void upload(fx::Fixture &f){
  using namespace fx;require(uploads.size()==3,"aux upload once");
  hr(f.allocator->Reset(),"aux allocator");hr(f.list->Reset(f.allocator.Get(),nullptr),"aux list");
  for(unsigned i=0;i<3;++i){uploads[i].copy(f.list.Get(),inputs[i].Get(),true);
   barrier(f.list.Get(),inputs[i].Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);}
  ComPtr<ID3D12DebugCommandList> debug;hr(f.list.As(&debug),"aux debug state");
  for(auto &input:inputs)require(debug->AssertResourceState(input.Get(),0,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)!=FALSE,"auxiliary transition state");
  hr(f.list->Close(),"aux close");ID3D12CommandList *lists[]{f.list.Get()};f.queue->ExecuteCommandLists(1,lists);f.wait();
  hr(f.allocator->Reset(),"aux retired allocator");hr(f.list->Reset(f.allocator.Get(),nullptr),"aux retired list");hr(f.list->Close(),"aux retired close");
  uploads.clear();
 }
 void prepare(route::Plan &p){
  stream_plan_fixture::put(color_backend+0x20,reinterpret_cast<std::uintptr_t>(p.resources[2].Get()));
  fx::require(route::tb::read_owner(route::reader,color_owner,p.draws[0].output),"fixture color owner");
  p.draws[3].output.resource=reinterpret_cast<std::uintptr_t>(p.resources[5].Get());
  p.draws[3].draw.shader_set=set;p.draws[3].packet_bindings[2]=graphics;
  fx::require(route::qualify_haze(p),"fixture actual haze admission");
 }
};

// Isolate auxiliary upload/state tracking from the renderer hooks and route.
inline void check_haze_auxiliary_state(fx::Fixture &f){
 using namespace fx;StreamHazeFixture auxiliary(f);
 constexpr char shader[]="Texture2D<float4> image:register(t3);float4 main(float4 p:SV_Position):SV_Target{return image.Load(int3(0,0,0));}";
 ComPtr<ID3DBlob> pixel,error;hr(D3DCompile(shader,sizeof(shader)-1,"auxiliary-state",nullptr,nullptr,"main","ps_5_0",D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_WARNINGS_ARE_ERRORS,0,&pixel,&error),"auxiliary shader");
 const auto geometry=vertices(sf::EffectStage::final_screen,{64,32,128,64},true);
 const auto r=recipe(sf::EffectStage::final_screen,{64,32,128,64},f.root.Get(),f.vs[2],{static_cast<const std::byte *>(pixel->GetBufferPointer()),pixel->GetBufferSize()},std::span(geometry.values).first(12));
 auto pso=pipeline(f.device.Get(),r,r.pixel_shader);auto vb=buffer(f.device.Get(),64,D3D12_HEAP_TYPE_UPLOAD);upload(vb.Get(),std::as_bytes(std::span(geometry.values)));
 auto desc=texture(128,64,DXGI_FORMAT_R11G11B10_FLOAT);desc.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
 auto output=resource(f.device.Get(),desc,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_RENDER_TARGET);
 D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;hd.NumDescriptors=1;ComPtr<ID3D12DescriptorHeap> rtv,srv;
 hr(f.device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&rtv)),"auxiliary target");f.device->CreateRenderTargetView(output.Get(),nullptr,rtv->GetCPUDescriptorHandleForHeapStart());
 hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;hd.NumDescriptors=5;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;hr(f.device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&srv)),"auxiliary descriptors");
 const auto step=f.device->GetDescriptorHandleIncrementSize(hd.Type);
 D3D12_SHADER_RESOURCE_VIEW_DESC sd{};sd.Format=DXGI_FORMAT_R8G8B8A8_UNORM;sd.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;sd.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;sd.Texture2D.MipLevels=1;
 for(unsigned i=0;i<5;++i)f.device->CreateShaderResourceView(i==3?auxiliary.inputs[2].Get():nullptr,&sd,{srv->GetCPUDescriptorHandleForHeapStart().ptr+SIZE_T(i)*step});
 auxiliary.upload(f);
 for(unsigned i=0;i<8;++i){
  hr(f.allocator->Reset(),"auxiliary allocator reset");hr(f.list->Reset(f.allocator.Get(),pso.Get()),"auxiliary list reset");
  ID3D12DescriptorHeap *heaps[]{srv.Get()};f.list->SetDescriptorHeaps(1,heaps);f.list->SetGraphicsRootSignature(f.root.Get());f.list->SetGraphicsRootDescriptorTable(4,srv->GetGPUDescriptorHandleForHeapStart());
  const auto handle=rtv->GetCPUDescriptorHandleForHeapStart();f.list->OMSetRenderTargets(1,&handle,FALSE,nullptr);
  const D3D12_VERTEX_BUFFER_VIEW view{vb->GetGPUVirtualAddress(),64,16};f.list->IASetVertexBuffers(0,1,&view);f.list->IASetPrimitiveTopology(geometry.topology);
  const D3D12_VIEWPORT viewport{0,0,128,64,0,1};const D3D12_RECT scissor{0,0,128,64};f.list->RSSetViewports(1,&viewport);f.list->RSSetScissorRects(1,&scissor);f.list->DrawInstanced(3,1,0,0);
  hr(f.list->Close(),"auxiliary list close");ID3D12CommandList *commands[]{f.list.Get()};f.queue->ExecuteCommandLists(1,commands);f.wait();f.messages();
 }
 std::cout<<"PASS auxiliary upload and shader read without hooks or predication\n";
}

