// Exact live reproduction: same mask owner, new 256x256 storage/view/descriptor
// after a plan captured 128x128. Uses production entry/API/retention functions.
{
 auto replacement=resource(f.device.Get(),texture(256,256,DXGI_FORMAT_R8G8B8A8_UNORM),D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COMMON);
 const auto replacement_backend=a.take(0x40),replacement_view=a.take(0x20);
 put(replacement_backend+0x20,reinterpret_cast<std::uintptr_t>(replacement.Get()));
 D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;hd.NumDescriptors=1;
 ComPtr<ID3D12DescriptorHeap> view_heap;hr(f.device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&view_heap)),"replacement CPU view");
 f.device->CreateShaderResourceView(replacement.Get(),nullptr,view_heap->GetCPUDescriptorHandleForHeapStart());
 put(replacement_view+0x10,view_heap->GetCPUDescriptorHandleForHeapStart().ptr);
 const auto original=qualified->dependency_bindings[1].textures[2];
 const auto restore=[&]{put(mask_owner+0x60,original.primary);put(mask_owner+0x88,128U+(128U<<16));put(mask_owner+0x78,mask_view);};
 const auto replace=[&]{put(mask_owner+0x60,replacement_backend);put(mask_owner+0x88,256U+(256U<<16));put(mask_owner+0x78,replacement_view);};
 auto run=std::make_unique<route::Plan>(*qualified);
 const auto old_index=run->draws[5].reads[2],old_count=route::resource_count(*run);
 require(route::dependency_enter(*run,5)&&route::dependency_current_stable(*run,5),"unchanged radial entry/API");
 replace();const auto old_gate=route::dependency_binding_check(*run,5);
 require(!old_gate.accepted()&&old_gate.differences==7&&old_gate.fields==896,"reproduce seven-field live replacement failure");
 require(route::dependency_enter(*run,5)&&route::dependency_current_stable(*run,5),"replacement accepted and stable at API");
 require(route::resource_count(*run)==old_count+1&&run->resources[old_index]==extra[2]&&run->resources[run->draws[5].reads[2]]==replacement,"old and new mask pinned separately");
 require(route::dependency_order(*run),"scene dependency graph preserved");
 put(replacement_view+0x10,view_heap->GetCPUDescriptorHandleForHeapStart().ptr+32);
 require(!route::dependency_current_stable(*run,5),"post-entry descriptor mutation rejects");
 put(replacement_view+0x10,view_heap->GetCPUDescriptorHandleForHeapStart().ptr);
 unsigned negatives{};
 const auto reject=[&](unsigned reason){auto test=std::make_unique<route::Plan>(*qualified);const auto count=route::resource_count(*test);
  require(!route::dependency_enter(*test,5)&&route::dependency_refresh.reason==reason&&!test->draws[5].binding_ready&&route::resource_count(*test)==count,"rejected replacement does not publish partial snapshot");++negatives;};
 put(mask_owner+0x70,2U);reject(2);put(mask_owner+0x70,0U);
 put(graphics_objects[1]+0x60,saved_object+8);reject(2);put(graphics_objects[1]+0x60,saved_object);
 put(graphics_objects[1]+0x48,owners[2]);reject(2);put(graphics_objects[1]+0x48,extra_owners[0]);
 put(mask_owner+0x88,128U+(128U<<16));reject(3);put(mask_owner+0x88,256U+(256U<<16));
 put(mode_arrays[1]+3,static_cast<unsigned char>(1));reject(1);put(mode_arrays[1]+3,static_cast<unsigned char>(0));
 put(graphics_objects[1]+0x10,std::uintptr_t{0});reject(1);put(graphics_objects[1]+0x10,mode_arrays[1]);
 {
  auto invalid=resource(f.device.Get(),texture(256,256,DXGI_FORMAT_R8G8B8A8_UINT),D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COMMON);
  put(replacement_backend+0x20,reinterpret_cast<std::uintptr_t>(invalid.Get()));reject(3);put(replacement_backend+0x20,reinterpret_cast<std::uintptr_t>(replacement.Get()));
 }
 {auto test=std::make_unique<route::Plan>(*qualified);test->resources[0]=replacement;require(!route::dependency_enter(*test,5)&&route::dependency_refresh.reason==4,"scene alias rejected");++negatives;}
 {auto test=std::make_unique<route::Plan>(*qualified);test->extra_resources=route::resource_capacity-5;require(!route::dependency_enter(*test,5)&&route::dependency_refresh.reason==5,"capacity exhaustion rejected");++negatives;}
 // Replacement must also be watched if native barriers follow entry. A
 // resource state imported from another prepared plan stays contradictory.
 const auto list_id=reinterpret_cast<std::uintptr_t>(f.list.Get());
 require(route::recording_states.observe(list_id,811,reinterpret_cast<std::uintptr_t>(replacement.Get()),true,0,D3D12_RESOURCE_STATE_COPY_DEST,replacement)==route::StateJournal::Result::ok,"known replacement state");
 {auto test=std::make_unique<route::Plan>(*qualified);auto *track=route::track_recording(*test,f.list.Get(),811,true);
  require(track&&route::dependency_enter(*test,5),"entry updates existing recording");const auto index=test->draws[5].reads[2];
  require(track->known[index]&&track->states[index]==D3D12_RESOURCE_STATE_COPY_DEST,"contradictory state retained for draw rejection");}
 require(route::recording_states.reset(list_id,811),"state fixture retirement");
 // Drop all plan references while a real GPU recording retains both versions.
 // Fence alone may not release them; Reset plus fence releases exactly our pins.
 const auto refs=[](ID3D12Resource *r){r->AddRef();return r->Release();};
 run.reset();const auto old_refs=refs(extra[2].Get()),new_refs=refs(replacement.Get());
 run=std::make_unique<route::Plan>(*qualified);require(route::prepare_selector(*run),"replacement selector");
 require(route::retain_recording(*run,f.list.Get(),812),"old native recording pins");
 require(route::dependency_enter(*run,5)&&route::retain_recording(*run,f.list.Get(),812),"replacement native recording pins");
 Pixels old_upload(f.device.Get(),extra[2]->GetDesc(),D3D12_HEAP_TYPE_UPLOAD),new_upload(f.device.Get(),replacement->GetDesc(),D3D12_HEAP_TYPE_UPLOAD);
 Pixels old_read(f.device.Get(),extra[2]->GetDesc(),D3D12_HEAP_TYPE_READBACK),new_read(f.device.Get(),replacement->GetDesc(),D3D12_HEAP_TYPE_READBACK);
 const std::vector<unsigned> old_pixels(128*128,0x11223344U),new_pixels(256*256,0xffabcdefU);
 old_upload.write(std::as_bytes(std::span(old_pixels)));new_upload.write(std::as_bytes(std::span(new_pixels)));
 hr(f.allocator->Reset(),"refresh allocator");hr(f.list->Reset(f.allocator.Get(),nullptr),"refresh list");
 for(auto *r:{extra[2].Get(),replacement.Get()})barrier(f.list.Get(),r,D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_COPY_DEST);
 old_upload.copy(f.list.Get(),extra[2].Get(),true);new_upload.copy(f.list.Get(),replacement.Get(),true);
 for(auto *r:{extra[2].Get(),replacement.Get()})barrier(f.list.Get(),r,D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_COPY_SOURCE);
 old_read.copy(f.list.Get(),extra[2].Get(),false);new_read.copy(f.list.Get(),replacement.Get(),false);
 hr(f.list->Close(),"refresh close");run.reset();
 require(refs(extra[2].Get())==old_refs+1&&refs(replacement.Get())==new_refs+1,"both versions survive plan recycling");
 require(route::lifetime.submitted(f.list.Get(),812,f.sequence+1),"refresh submit");ID3D12CommandList *lists[]{f.list.Get()};f.queue->ExecuteCommandLists(1,lists);f.wait();route::lifetime.collect(f.sequence);
 require(old_read.read()==old_pixels&&new_read.read()==new_pixels,"old and replacement GPU contents exact");
 require(refs(extra[2].Get())==old_refs+1&&refs(replacement.Get())==new_refs+1,"fence alone keeps both versions");
 hr(f.allocator->Reset(),"refresh retire allocator");hr(f.list->Reset(f.allocator.Get(),nullptr),"refresh retire list");hr(f.list->Close(),"refresh retired close");
 require(route::lifetime.reset(f.list.Get(),812),"refresh Reset receipt");route::lifetime.collect(f.sequence);
 require(refs(extra[2].Get())==old_refs&&refs(replacement.Get())==new_refs&&!route::lifetime.live(),"both versions released after fence and Reset");
 #include "radial_refresh_record_fixture.inl"
 route::dependency_refresh.write(std::cout);std::cout<<'\n';
 restore();route::dependency_binding_trace={};route::dependency_refresh={};
 std::cout<<"PASS 128-to-256 replacement, "<<negatives<<" unsafe cases, API mutation rejection, state import, 81920 exact GPU pixels and fence+Reset resource retention\n";
}
