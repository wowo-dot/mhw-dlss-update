// Owned GPU resources only. Distinct clear values test routing, not raster jitter.
void triplet_readback_case(const std::filesystem::path &path) {
  const auto stages=config.sequence_readback?5U:3U;
  const auto prefix=config.sequence_readback?"readback_sequence_":"readback_triplet_";
  auto is=[&](const char *suffix){return mode==std::string(prefix)+suffix;};
  construct();std::string why;runtime.width=16;runtime.height=16;
  check(pv::prepare_readback(runtime) && pb::arm(why),"triplet prepare/arm");
  const bool history_pressure=is("history_pressure"),history_overflow=is("history_overflow");
  const bool delayed_poll=history_pressure || history_overflow;
  if(delayed_poll) {
    // Real observed fence reads, no synthetic journal entries. Delay private
    // polling until all five recordings retire, as in the production worker.
    for(unsigned i=0;i<6000;++i)check(completion_fence->GetCompletedValue()==0,"history warmup fence");
    check(pv::healthy(),"history rejected below new capacity");
  }
  ComPtr<ID3D12CommandAllocator> draw_allocator;ComPtr<ID3D12GraphicsCommandList> draw;
  check(SUCCEEDED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&draw_allocator))) &&
    SUCCEEDED(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,draw_allocator.Get(),nullptr,IID_PPV_ARGS(&draw))) &&
    SUCCEEDED(draw->Close()),"triplet draw list");
  auto desc=texture->GetDesc();desc.Format=DXGI_FORMAT_R32_FLOAT;desc.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
  D3D12_HEAP_PROPERTIES hp{};hp.Type=D3D12_HEAP_TYPE_DEFAULT;ComPtr<ID3D12Resource> dst;
  check(SUCCEEDED(device->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&desc,static_cast<D3D12_RESOURCE_STATES>(0xc0),nullptr,IID_PPV_ARGS(&dst))),"triplet destination");
  put(addr(frame_destination_backing),0x20,reinterpret_cast<U>(dst.Get()));
  for(unsigned stage=0;stage<stages;++stage) {
    put(config.frame_renderer,0x1f210,UINT64{101+stage});put(config.frame_renderer,0x1f208,UINT64{91+stage});
    put(config.frame_renderer,0x1f200,stage%2);
    pb::Source selected{};selected.valid=selected.projection.valid=true;selected.order=1+10*stage;selected.thread=GetCurrentThreadId();
    selected.anchor.renderer=config.frame_renderer;selected.anchor.context=config.frame_renderer+0x710;
    selected.anchor.frame=101+stage;selected.anchor.epoch=91+stage;selected.anchor.bank=stage%2;
    selected.projection.constants=addr(backing);selected.projection.buffer_id=11;
    if(!(is("missing_baseline") && stage==0))check(pv::select_source(selected,stage),"triplet source selection");
    if((is("wrong_recovery") && stage+1==stages) || (is("wrong_middle") && stage==3))put(config.frame_renderer,0x1f210,UINT64{200});
    if(stage)check(SUCCEEDED(command->Close()),"triplet retired Close");
    check(SUCCEEDED(command->Reset(allocator.Get(),nullptr)),"triplet copy Reset");
    D3D12_RESOURCE_BARRIER b[2]{};b[0].Transition.pResource=texture.Get();b[0].Transition.Subresource=UINT_MAX;
    b[0].Transition.StateBefore=D3D12_RESOURCE_STATE_DEPTH_WRITE;b[0].Transition.StateAfter=D3D12_RESOURCE_STATE_COPY_SOURCE;
    b[1].Transition.pResource=dst.Get();b[1].Transition.Subresource=UINT_MAX;
    b[1].Transition.StateBefore=static_cast<D3D12_RESOURCE_STATES>(0xc0);b[1].Transition.StateAfter=D3D12_RESOURCE_STATE_COPY_DEST;
    command->ResourceBarrier(2,b);
    binding_fixture26(config.frame_renderer+0x710,addr(frame_rect),addr(frame_offset),addr(frame_destination),8,0,0);
    binding_fixture27(addr(frame_executor),addr(frame_context),addr(frame_packet),0,0,0,0);
    if(is("extra_copy")) {
      // The same request is consumed again; one slot must not issue twice.
      binding_fixture27(addr(frame_executor),addr(frame_context),addr(frame_packet),0,0,0,0);
    }
    b[1].Transition.StateBefore=D3D12_RESOURCE_STATE_COPY_DEST;b[1].Transition.StateAfter=static_cast<D3D12_RESOURCE_STATES>(0xc0);
    command->ResourceBarrier(1,b+1);check(SUCCEEDED(command->Close()),"triplet copy Close");
    check(SUCCEEDED(draw_allocator->Reset()) && SUCCEEDED(draw->Reset(draw_allocator.Get(),nullptr)),"triplet draw Reset");
    b[0].Transition.StateBefore=stage?D3D12_RESOURCE_STATE_COPY_SOURCE:D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    b[0].Transition.StateAfter=D3D12_RESOURCE_STATE_DEPTH_WRITE;draw->ResourceBarrier(1,b);
    draw->ClearDepthStencilView({config.heap_start},D3D12_CLEAR_FLAG_DEPTH,.25F+.125F*stage,0,0,nullptr);
    if(stage==1){pv::mark_binding(false);pv::mark_binding(true);mark(draw.Get());pv::end_frame();}
    check(SUCCEEDED(draw->Close()),"triplet draw Close");
    ID3D12CommandList *batch[]={draw.Get(),command.Get()};queue->ExecuteCommandLists(2,batch);
    check(SUCCEEDED(queue->Signal(completion_fence.Get(),stage+1)),"triplet native fence");
    const auto deadline=GetTickCount64()+5000;
    while(completion_fence->GetCompletedValue()<stage+1 && GetTickCount64()<deadline){if(!delayed_poll)pv::poll_watch();Sleep(1);}
    check(completion_fence->GetCompletedValue()==stage+1,"triplet GPU completion");
    check(SUCCEEDED(allocator->Reset()) && SUCCEEDED(command->Reset(allocator.Get(),nullptr)),"triplet retirement");
    if(history_overflow && stage+1==stages) {
      for(unsigned i=0;i<=pv::history_capacity && pv::healthy();++i)completion_fence->GetCompletedValue();
      check(pv::owned_error()==pv::Error::capacity,"history overflow did not close admission");
    }
    if(!delayed_poll || stage+1==stages)pv::poll_watch();
    if(stage+1<stages)check(pv::awaiting_submission(),"triplet stopped early");
  }
  if(is("view_changed"))put(addr(objects[0]),0x18,reinterpret_cast<U>(texture.Get())+16);
  pb::stop();pv::stop();check(pb::disable() && pv::disable(),"triplet cleanup");
  for(unsigned stage=0;stage<stages;++stage) {
    const auto target=stage==1?path:std::filesystem::path(path.wstring()+L".control-"+std::to_wstring(stage)+L".f32");
    const bool saved=pv::save_readback(target.wstring(),stage);
    const bool expected=!history_overflow && !(stage==0 && is("missing_baseline")) &&
      !(stage+1==stages && is("wrong_recovery")) && !(stage==3 && is("wrong_middle")) && !(stage!=1 && is("view_changed"));
    check(saved==expected,"triplet saved result");
    if(saved) {
      std::array<float,256> pixels{};std::ifstream input(target,std::ios::binary);input.read(reinterpret_cast<char *>(pixels.data()),sizeof(pixels));
      check(static_cast<bool>(input) && std::filesystem::file_size(target)==1024,"triplet pixel extent");
      for(float value:pixels)check(value==.25F+.125F*stage,"triplet pixel routing");
    }
    std::ostringstream report;pv::write_readback(report,stage);
    if(stage==1)readback_report=report.str();
    else {std::ofstream out(path.wstring()+L".control-"+std::to_wstring(stage)+L".json");out<<report.str();}
  }
  check(pv::owned_release_readback(true),"triplet private refs");
}
