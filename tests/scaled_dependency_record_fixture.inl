// Exercise the actual record_effect branch, its predicates, restored bindings,
// extra resource pins and two-part retirement proof using a real copy draw.
{
 auto native_recipe=route::recipe(qualified->draws[4],4,f.root.Get());
 D3D12_GRAPHICS_PIPELINE_STATE_DESC pd{};pd.pRootSignature=f.root.Get();pd.VS={copy_vs.data(),copy_vs.size()};pd.PS={copy_ps.data(),copy_ps.size()};
 pd.BlendState=native_recipe.blend;pd.RasterizerState=native_recipe.raster;pd.DepthStencilState=native_recipe.depth;pd.SampleMask=UINT_MAX;
 pd.InputLayout={native_recipe.input_layout.data(),2};pd.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;pd.NumRenderTargets=1;pd.RTVFormats[0]=DXGI_FORMAT_R10G10B10A2_UNORM;pd.SampleDesc.Count=1;
 ComPtr<ID3D12PipelineState> native;hr(f.device->CreateGraphicsPipelineState(&pd,IID_PPV_ARGS(&native)),"dependency copy native PSO");
 auto vb=buffer(f.device.Get(),256,D3D12_HEAP_TYPE_UPLOAD),cb=buffer(f.device.Get(),1024,D3D12_HEAP_TYPE_UPLOAD);
 std::array<std::byte,256> vertex_data{};std::memcpy(vertex_data.data()+192,qualified->draws[4].dependency_vertices.data(),64);upload(vb.Get(),vertex_data);
 std::array<std::byte,1024> constants{};upload(cb.Get(),constants);
 D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.NumDescriptors=2;hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;ComPtr<ID3D12DescriptorHeap> rtvs,srvs;
 hr(f.device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&rtvs)),"dependency targets");const auto rtstep=f.device->GetDescriptorHandleIncrementSize(hd.Type);
 const std::array<D3D12_CPU_DESCRIPTOR_HANDLE,2> handles{{rtvs->GetCPUDescriptorHandleForHeapStart(),{rtvs->GetCPUDescriptorHandleForHeapStart().ptr+rtstep}}};
 f.device->CreateRenderTargetView(resources[2].Get(),nullptr,handles[0]);f.device->CreateRenderTargetView(extra[0].Get(),nullptr,handles[1]);
 hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;hd.NumDescriptors=5;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;hr(f.device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&srvs)),"dependency SRVs");
 const auto step=f.device->GetDescriptorHandleIncrementSize(hd.Type);D3D12_SHADER_RESOURCE_VIEW_DESC sd{};sd.Format=DXGI_FORMAT_R11G11B10_FLOAT;sd.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;sd.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;sd.Texture2D.MipLevels=1;
 for(unsigned i=0;i<5;++i)f.device->CreateShaderResourceView(i?nullptr:resources[2].Get(),&sd,{srvs->GetCPUDescriptorHandleForHeapStart().ptr+SIZE_T(i)*step});
 Pixels pixels(f.device.Get(),extra[0]->GetDesc(),D3D12_HEAP_TYPE_READBACK);
 Pixels source_upload(f.device.Get(),resources[2]->GetDesc(),D3D12_HEAP_TYPE_UPLOAD),target_upload(f.device.Get(),extra[0]->GetDesc(),D3D12_HEAP_TYPE_UPLOAD);
 const std::vector<unsigned> source_values(128*64,832U|(896U<<11)|(464U<<22)),zeros(128*64);
 source_upload.write(std::as_bytes(std::span(source_values)));target_upload.write(std::as_bytes(std::span(zeros)));
 for(unsigned frame_index=0;frame_index<2;++frame_index){const auto generation=700+frame_index;
  auto &run=route::plans[31];std::destroy_at(&run);std::construct_at(&run);run.resources=qualified->resources;run.extra_resources=3;run.stamp=s;run.stamp.frame=generation;
  run.overlay_count=1;run.ordered_count=4;run.order={0,4,1,2};run.draws[4]=qualified->draws[4];run.dependency_bindings[0]=qualified->dependency_bindings[0];run.targets[4]=qualified->targets[4];
  hr(f.allocator->Reset(),"dependency allocator");hr(f.list->Reset(f.allocator.Get(),native.Get()),"dependency list");mhw::temporal_present::observer::reset(f.list.Get(),generation,native.Get());ViewportFeed::expected=f.list.Get();
  const auto transition=[&](ID3D12Resource *texture,D3D12_RESOURCE_STATES before,D3D12_RESOURCE_STATES after){D3D12_RESOURCE_BARRIER b{};b.Transition={texture,UINT_MAX,before,after};f.list->ResourceBarrier(1,&b);route::barriers(f.list.Get(),1,&b);};
  ID3D12DescriptorHeap *heaps[]{srvs.Get()};f.list->SetDescriptorHeaps(1,heaps);f.list->SetGraphicsRootSignature(f.root.Get());
  for(unsigned i=0;i<4;++i)f.list->SetGraphicsRootConstantBufferView(i,cb->GetGPUVirtualAddress()+256*i);f.list->SetGraphicsRootDescriptorTable(4,srvs->GetGPUDescriptorHandleForHeapStart());
  f.list->SetPipelineState(native.Get());const D3D12_VERTEX_BUFFER_VIEW copy_view{vb->GetGPUVirtualAddress(),256,16};f.list->IASetVertexBuffers(0,1,&copy_view);f.list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
  const D3D12_VIEWPORT viewport{0,0,64,32,0,1};const D3D12_RECT scissor{0,0,64,32};f.list->RSSetViewports(1,&viewport);f.list->RSSetScissorRects(1,&scissor);f.list->OMSetRenderTargets(1,&handles[1],FALSE,nullptr);
  sf::EffectBindings bindings{f.list.Get(),native.Get(),f.root.Get(),extra[0].Get(),vb.Get(),copy_view,viewport,scissor,D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,4,12};bool replaced=false;
  require(route::dependency_enter(run,4),"copy entry binding capture");
  auto prepared=route::recipe(run.draws[4],4,f.root.Get());require(!route::record_effect(31,4,prepared,bindings,handles[1].ptr,replaced),"dependency warmup leaves native draw");
  const std::array<std::uintptr_t,6> command_packets{11,12,13,run.draws[4].draw.packet,15,16};require(route::prepare_selector(run)&&run.handoff.prepare({generation,1,1,1},command_packets)==sf::FrameHandoff::Result::ok,"dependency frame choice");run.prepared=true;
  for(unsigned i=0;i<3;++i)require(run.handoff.record(i,command_packets[i],{reinterpret_cast<std::uintptr_t>(f.list.Get()),generation})==sf::FrameHandoff::Result::ok,"preceding commands");
  require(route::choose(run,frame_index?sf::FrameHandoff::Choice::native:sf::FrameHandoff::Choice::reconstructed),"dependency atomic choice");run.resolved=true;
  transition(resources[2].Get(),frame_index?D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE:D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_COPY_DEST);
  source_upload.copy(f.list.Get(),resources[2].Get(),true);transition(resources[2].Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  transition(extra[0].Get(),frame_index?D3D12_RESOURCE_STATE_COPY_SOURCE:D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_COPY_DEST);target_upload.copy(f.list.Get(),extra[0].Get(),true);transition(extra[0].Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_RENDER_TARGET);
  require(route::record_effect(31,4,prepared,bindings,handles[1].ptr,replaced)&&replaced,"production dependency copy route");
  mhw::temporal_present::observer::Predicate predicate;require(mhw::temporal_present::observer::predicate_current(f.list.Get(),predicate)&&!predicate.buffer,"dependency predicate restored");
  for(unsigned i=4;i<6;++i)require(run.handoff.record(i,command_packets[i],{reinterpret_cast<std::uintptr_t>(f.list.Get()),generation})==sf::FrameHandoff::Result::ok,"following commands");
  transition(extra[0].Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_COPY_SOURCE);pixels.copy(f.list.Get(),extra[0].Get(),false);hr(f.list->Close(),"dependency close");
  const std::array<std::array<std::uint64_t,2>,1> batch{{{reinterpret_cast<std::uintptr_t>(f.list.Get()),generation}}};const auto queue_id=reinterpret_cast<std::uintptr_t>(f.queue.Get());
  require(route::validate_submission(queue_id,batch,f.sequence+1)&&route::submitted_batch(queue_id,batch,f.sequence+1),"dependency submission order");ID3D12CommandList *lists[]{f.list.Get()};f.queue->ExecuteCommandLists(1,lists);f.wait();route::collect(f.sequence);require(route::lifetime.live()==1,"fence alone keeps temporary pins");
  const auto actual=pixels.read();require(actual[0]!=0,"copy writes color");for(unsigned y=0;y<64;++y)for(unsigned x=0;x<128;++x)require(actual[y*128+x]==(!frame_index||(x<64&&y<32)?actual[0]:0),"full dependency output or unchanged native footprint");
  hr(f.allocator->Reset(),"dependency retire allocator");hr(f.list->Reset(f.allocator.Get(),nullptr),"dependency retire list");hr(f.list->Close(),"dependency retired close");require(route::reset(f.list.Get(),generation),"dependency retirement Reset");route::collect(f.sequence);require(!route::lifetime.live(),"temporary pins released after fence and Reset");run.resources={};
 }
 for(auto &cache:route::dependency_pipelines)if(cache.effect&&cache.effect->recorded())cache.effect->retire(f.fence.Get(),f.sequence,true);
 require(route::healthy(),"healthy dependency route");std::cout<<"PASS production dependency recording: expanded/native pixels, restored bindings, completed+Reset pins\n";
}
