// WARP draws from a depth SRV into an 8x6 rectangle of a 17x13 target.
bool draw_fixture{};
bool pixel_draw_fixture{};
bool pixel_writer(unsigned ordinal){return pixel_draw_fixture && (mode!="depth_multi" || ordinal%3==0);}
ComPtr<ID3D12RootSignature> draw_root;
ComPtr<ID3D12PipelineState> draw_pipeline;
ComPtr<ID3D12DescriptorHeap> draw_srv_heap,draw_rtv_heap;
void setup_depth_draw(ID3D12Device *device){
  D3D12_DESCRIPTOR_RANGE range{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,1,0,0,0};
  D3D12_ROOT_PARAMETER param{};param.ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  param.DescriptorTable={1,&range};param.ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
  D3D12_ROOT_SIGNATURE_DESC desc{1,&param,0,nullptr,D3D12_ROOT_SIGNATURE_FLAG_NONE};
  ComPtr<ID3DBlob> blob,error,vs,ps;
  check(SUCCEEDED(D3D12SerializeRootSignature(&desc,D3D_ROOT_SIGNATURE_VERSION_1,&blob,&error)),"draw root bytes");
  check(SUCCEEDED(device->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&draw_root))),"draw root");
  constexpr char shader[]="Texture2D<float> z:register(t0); float4 vs(uint i:SV_VertexID):SV_Position {i=i%3;float2 p=float2((i<<1)&2,i&2);return float4(p*float2(2,-2)+float2(-1,1),0,1);} float ps(float4 p:SV_Position):SV_Target {return z.Load(int3(int2(p.xy),0));}";
  check(SUCCEEDED(D3DCompile(shader,sizeof(shader)-1,nullptr,nullptr,nullptr,"vs","vs_5_0",0,0,&vs,&error)),"draw vertex shader");
  check(SUCCEEDED(D3DCompile(shader,sizeof(shader)-1,nullptr,nullptr,nullptr,"ps","ps_5_0",0,0,&ps,&error)),"draw pixel shader");
  D3D12_GRAPHICS_PIPELINE_STATE_DESC p{};p.pRootSignature=draw_root.Get();
  p.VS={vs->GetBufferPointer(),vs->GetBufferSize()};p.PS={ps->GetBufferPointer(),ps->GetBufferSize()};
  p.BlendState.RenderTarget[0].RenderTargetWriteMask=D3D12_COLOR_WRITE_ENABLE_ALL;
  p.SampleMask=UINT_MAX;p.SampleDesc.Count=1;p.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;
  p.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;p.RasterizerState.DepthClipEnable=TRUE;
  p.DepthStencilState.DepthFunc=D3D12_COMPARISON_FUNC_ALWAYS;
  p.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;p.NumRenderTargets=1;p.RTVFormats[0]=DXGI_FORMAT_R32_FLOAT;
  check(SUCCEEDED(device->CreateGraphicsPipelineState(&p,IID_PPV_ARGS(&draw_pipeline))),"draw pipeline");
  D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.NumDescriptors=1;hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  check(SUCCEEDED(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&draw_rtv_heap))),"draw RTV heap");
  device->CreateRenderTargetView(depth_resource,nullptr,draw_rtv_heap->GetCPUDescriptorHandleForHeapStart());
  hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  check(SUCCEEDED(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&draw_srv_heap))),"draw SRV heap");
  D3D12_SHADER_RESOURCE_VIEW_DESC srv{};srv.Format=DXGI_FORMAT_R32_FLOAT;srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;
  srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;srv.Texture2D.MipLevels=1;
  device->CreateShaderResourceView(route_source,&srv,draw_srv_heap->GetCPUDescriptorHandleForHeapStart());
}
void WINAPI mock_native_draw(ID3D12GraphicsCommandList *list,UINT n,UINT instances,UINT first,UINT instance){
  list->DrawInstanced(n,instances,first,instance);
  if(mode=="depth_draw_changed")put(recording_at+0x108,get<std::uintptr_t>(recording_at+0x108)+8);
}
void depth_draw_commands(std::uintptr_t packet){
  std::array<D3D12_RESOURCE_BARRIER,2> b{};
  const auto source_state=static_cast<D3D12_RESOURCE_STATES>(pixel_writer(route_consumers)?0x80:0xc0);
  b[0].Transition={route_source,UINT_MAX,D3D12_RESOURCE_STATE_DEPTH_WRITE,source_state};
  if(route_consumers==1){
    D3D12_RESOURCE_BARRIER initial{};initial.Transition={depth_resource,UINT_MAX,D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE};
    native_list->ResourceBarrier(1,&initial);
  }
  b[1].Transition={depth_resource,UINT_MAX,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_RENDER_TARGET};
  tb::gpu::fixture_barrier(native_list,2,b.data());
  auto rtv=draw_rtv_heap->GetCPUDescriptorHandleForHeapStart();float clear[4]{.875F,.875F,.875F,.875F};
  native_list->ClearRenderTargetView(rtv,clear,0,nullptr);native_list->OMSetRenderTargets(1,&rtv,FALSE,nullptr);
  native_list->SetGraphicsRootSignature(draw_root.Get());native_list->SetPipelineState(draw_pipeline.Get());
  ID3D12DescriptorHeap *heaps[]{draw_srv_heap.Get()};native_list->SetDescriptorHeaps(1,heaps);
  native_list->SetGraphicsRootDescriptorTable(0,draw_srv_heap->GetGPUDescriptorHandleForHeapStart());
  D3D12_VIEWPORT viewport{0,0,8,6,0,1};D3D12_RECT scissor{0,0,8,6};
  native_list->RSSetViewports(1,&viewport);native_list->RSSetScissorRects(1,&scissor);native_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  put(recording_at+0x108,reinterpret_cast<std::uintptr_t>(draw_pipeline.Get()));
  put(recording_at+0x158,get<std::uintptr_t>(packet+0x40));
  if(mode=="depth_draw_nested")put(recording_at+0x158,std::uintptr_t{});
  dr::fixture_draw(native_list,mode=="depth_draw_arguments"?4U:3U,1,pixel_writer(route_consumers)?3U:0U,0,cfg.image+0x259d72e+(mode=="depth_draw_caller"?1:0));
  b[0].Transition.StateBefore=source_state;b[0].Transition.StateAfter=D3D12_RESOURCE_STATE_DEPTH_WRITE;
  b[1].Transition.StateBefore=D3D12_RESOURCE_STATE_RENDER_TARGET;b[1].Transition.StateAfter=D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
  tb::gpu::fixture_barrier(native_list,2,b.data());
}
