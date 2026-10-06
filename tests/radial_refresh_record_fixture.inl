// Compare the actual refreshed route with a standalone effect using the same
// current native bindings. Shader/coordinate correctness has a separate oracle.
{
 D3D12_DESCRIPTOR_RANGE range{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,3,0,0,0};std::array<D3D12_ROOT_PARAMETER,7> params{};
 for(unsigned i=0;i<6;++i){params[i].ParameterType=D3D12_ROOT_PARAMETER_TYPE_CBV;params[i].Descriptor.ShaderRegister=i;params[i].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;}
 params[6].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;params[6].DescriptorTable={1,&range};params[6].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
 std::array<D3D12_STATIC_SAMPLER_DESC,3> samplers{};for(unsigned i=0;i<3;++i){auto &v=samplers[i];v.Filter=i==1?D3D12_FILTER_MIN_MAG_MIP_POINT:D3D12_FILTER_MIN_MAG_MIP_LINEAR;v.AddressU=v.AddressV=v.AddressW=D3D12_TEXTURE_ADDRESS_MODE_CLAMP;v.ShaderRegister=i;v.ComparisonFunc=D3D12_COMPARISON_FUNC_NEVER;v.ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;}
 D3D12_ROOT_SIGNATURE_DESC rd{7,params.data(),3,samplers.data(),D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT};ComPtr<ID3DBlob> serial,error;ComPtr<ID3D12RootSignature> radial_root;
 hr(D3D12SerializeRootSignature(&rd,D3D_ROOT_SIGNATURE_VERSION_1,&serial,&error),"radial refresh serialize");hr(f.device->CreateRootSignature(0,serial->GetBufferPointer(),serial->GetBufferSize(),IID_PPV_ARGS(&radial_root)),"radial refresh root");
 auto &actual=route::plans[30];actual=*qualified;require(route::dependency_enter(actual,5),"radial route refresh entry");
 auto recipe=route::recipe(actual.draws[5],5,radial_root.Get());
 D3D12_GRAPHICS_PIPELINE_STATE_DESC pd{};pd.pRootSignature=radial_root.Get();pd.VS={radial_vs.data(),radial_vs.size()};pd.PS={radial_ps.data(),radial_ps.size()};pd.BlendState=recipe.blend;pd.RasterizerState=recipe.raster;pd.DepthStencilState=recipe.depth;pd.SampleMask=UINT_MAX;
 pd.InputLayout={recipe.input_layout.data(),4};pd.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;pd.NumRenderTargets=1;pd.RTVFormats[0]=DXGI_FORMAT_R11G11B10_FLOAT;pd.SampleDesc.Count=1;
 ComPtr<ID3D12PipelineState> native;hr(f.device->CreateGraphicsPipelineState(&pd,IID_PPV_ARGS(&native)),"radial refresh native PSO");
 auto vb=buffer(f.device.Get(),112,D3D12_HEAP_TYPE_UPLOAD);upload(vb.Get(),actual.draws[5].dependency_vertices);
 std::array<float,704> constants{};const unsigned offsets[]{0,256,512,768,2048,2304};constants[70]=1.F/128;constants[71]=1.F/64;
 auto *blur=constants.data()+192;blur[0]=.205F;blur[1]=.285F;blur[2]=1.F;blur[3]=.24F;blur[4]=.22F;blur[5]=.18F;blur[6]=.25F;blur[7]=.7F;blur[8]=.03F;blur[9]=.3F;blur[10]=.7F;blur[11]=std::bit_cast<float>(4U);blur[12]=.3F;blur[13]=-.2F;blur[14]=.5F;blur[15]=.75F;
 for(unsigned i=0;i<256;++i)blur[16+i]=.1F+.8F*float((i*7)%37)/36.F;
 constants[512]=.5F/128;constants[513]=.5F/64;constants[514]=.5F-.5F/128;constants[515]=.5F-.5F/64;for(unsigned i=576;i<580;++i)constants[i]=std::bit_cast<float>(1U);
 auto cb=buffer(f.device.Get(),sizeof(constants),D3D12_HEAP_TYPE_UPLOAD);upload(cb.Get(),std::as_bytes(std::span(constants)));
 ComPtr<ID3D12DescriptorHeap> srvs,rtvs;D3D12_DESCRIPTOR_HEAP_DESC h{};h.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;h.NumDescriptors=3;h.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;hr(f.device->CreateDescriptorHeap(&h,IID_PPV_ARGS(&srvs)),"radial refresh SRVs");
 const auto stride=f.device->GetDescriptorHandleIncrementSize(h.Type);const std::array<ID3D12Resource *,3> inputs{extra[0].Get(),extra[1].Get(),replacement.Get()};
 for(unsigned i=0;i<3;++i)f.device->CreateShaderResourceView(inputs[i],nullptr,{srvs->GetCPUDescriptorHandleForHeapStart().ptr+SIZE_T(i)*stride});
 h.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;h.NumDescriptors=1;h.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_NONE;hr(f.device->CreateDescriptorHeap(&h,IID_PPV_ARGS(&rtvs)),"radial refresh RTV");const auto rtv=rtvs->GetCPUDescriptorHandleForHeapStart();f.device->CreateRenderTargetView(resources[2].Get(),nullptr,rtv);
 Pixels temp_upload(f.device.Get(),extra[0]->GetDesc(),D3D12_HEAP_TYPE_UPLOAD),occ_upload(f.device.Get(),extra[1]->GetDesc(),D3D12_HEAP_TYPE_UPLOAD),out_read(f.device.Get(),resources[2]->GetDesc(),D3D12_HEAP_TYPE_READBACK);
 std::vector<unsigned> temp_pixels(128*64);for(unsigned i=0;i<temp_pixels.size();++i)temp_pixels[i]=0xc0000000U|((i%1024)<<20)|(((i*7)%1024)<<10)|((i*13)%1024);
 temp_upload.write(std::as_bytes(std::span(temp_pixels)));const std::array<unsigned,1> occ{0xffaaaaaaU};occ_upload.write(std::as_bytes(std::span(occ)));
 sf::Effect reference(f.device.Get(),recipe);std::vector<unsigned> result;
 for(unsigned pass=0;pass<2;++pass){const unsigned generation=820+pass;
  hr(f.allocator->Reset(),"radial allocator");hr(f.list->Reset(f.allocator.Get(),native.Get()),"radial list");mhw::temporal_present::observer::reset(f.list.Get(),generation,native.Get());ViewportFeed::expected=f.list.Get();
  const auto transition=[&](ID3D12Resource *r,D3D12_RESOURCE_STATES before,D3D12_RESOURCE_STATES after){D3D12_RESOURCE_BARRIER b{};b.Transition={r,UINT_MAX,before,after};f.list->ResourceBarrier(1,&b);route::barriers(f.list.Get(),1,&b);};
  if(!pass){
   transition(extra[0].Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_COPY_DEST);temp_upload.copy(f.list.Get(),extra[0].Get(),true);transition(extra[0].Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
   transition(extra[1].Get(),D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_COPY_DEST);occ_upload.copy(f.list.Get(),extra[1].Get(),true);transition(extra[1].Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
   transition(replacement.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  }
  ID3D12DescriptorHeap *heaps[]{srvs.Get()};f.list->SetDescriptorHeaps(1,heaps);f.list->SetGraphicsRootSignature(radial_root.Get());for(unsigned i=0;i<6;++i)f.list->SetGraphicsRootConstantBufferView(i,cb->GetGPUVirtualAddress()+offsets[i]);f.list->SetGraphicsRootDescriptorTable(6,srvs->GetGPUDescriptorHandleForHeapStart());f.list->SetPipelineState(native.Get());
  const D3D12_VERTEX_BUFFER_VIEW radial_vertex_view{vb->GetGPUVirtualAddress(),112,28};const D3D12_VIEWPORT viewport{0,0,64,32,0,1};const D3D12_RECT scissor{0,0,64,32};f.list->IASetVertexBuffers(0,1,&radial_vertex_view);f.list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);f.list->RSSetViewports(1,&viewport);f.list->RSSetScissorRects(1,&scissor);f.list->OMSetRenderTargets(1,&rtv,FALSE,nullptr);
  sf::EffectBindings b{f.list.Get(),native.Get(),radial_root.Get(),resources[2].Get(),vb.Get(),radial_vertex_view,viewport,scissor,D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,4,0};bool replaced=false;
  if(!pass){
   require(!route::record_effect(30,5,recipe,b,rtv.ptr,replaced),"radial warmup");require(route::prepare_selector(actual),"radial selector");
   const std::array<std::uintptr_t,7> radial_packets{101,102,103,104,actual.draws[5].draw.packet,106,107};require(actual.handoff.prepare({generation,1,1,1},radial_packets)==sf::FrameHandoff::Result::ok,"radial handoff");actual.prepared=true;
   for(unsigned i=0;i<4;++i)require(actual.handoff.record(i,radial_packets[i],{list_id,generation})==sf::FrameHandoff::Result::ok,"radial predecessors");
   require(route::choose(actual,sf::FrameHandoff::Choice::reconstructed),"radial selected");actual.resolved=true;
  }
  transition(resources[2].Get(),pass?D3D12_RESOURCE_STATE_COPY_SOURCE:D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_RENDER_TARGET);const float black[4]{64,32,16,1};f.list->ClearRenderTargetView(rtv,black,0,nullptr);
  if(!pass)require(route::record_effect(30,5,recipe,b,rtv.ptr,replaced)&&replaced,"actual refreshed radial DrawInstanced");else require(reference.record(b),"reference radial DrawInstanced");
  transition(resources[2].Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_COPY_SOURCE);out_read.copy(f.list.Get(),resources[2].Get(),false);hr(f.list->Close(),"radial close");
  if(!pass)require(route::lifetime.submitted(f.list.Get(),generation,f.sequence+1),"radial lifetime submission");
  ID3D12CommandList *radial_lists[]{f.list.Get()};f.queue->ExecuteCommandLists(1,radial_lists);f.wait();const auto pixels=out_read.read();
  if(!pass){result=pixels;require(std::any_of(pixels.begin(),pixels.end(),[](unsigned v){return v!=(1344U|(1280U<<11)|(608U<<22));}),"radial image differs from clear");}else require(pixels==result,"refreshed radial matches current-binding reference exactly");
  hr(f.allocator->Reset(),"radial retire allocator");hr(f.list->Reset(f.allocator.Get(),nullptr),"radial retire list");hr(f.list->Close(),"radial retired close");
  if(!pass){require(route::lifetime.reset(f.list.Get(),generation),"radial lifetime Reset");route::lifetime.collect(f.sequence);actual.prepared=false;}else reference.retire(f.fence.Get(),f.sequence,true);
 }
 for(auto &cache:route::dependency_pipelines)if(cache.effect&&cache.effect->recorded()&&!cache.effect->retired())cache.effect->retire(f.fence.Get(),f.sequence,true);
 actual.resources={};route::recording_states.clear();require(!route::lifetime.live(),"radial pins drained");
 std::cout<<"PASS refreshed production radial draw: 8192 pixels equal current-binding reference, native bindings restored and pins retired\n";
}
