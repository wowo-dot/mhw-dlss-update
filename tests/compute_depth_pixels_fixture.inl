// Included in wmain after observer admission; exercises the production queue
// submission path in scaled_runtime_route_fixture, using owned synthetic data.
if(mode=="compute_depth_pixels"){
#ifdef MHW_STREAM_RUNTIME_FIXTURE
    constexpr unsigned w=2560,h=1440;
    route::width=w;route::height=h;
    auto &capture=route::compute_depth_pixels[0];require(capture.prepare(f.device.Get(),w,h),"depth buffer prepare");
    require(route::compute_depth_pixels[1].prepare(f.device.Get(),w,h),"unused depth buffer prepare");
    const auto api=*reinterpret_cast<void ***>(f.list.Get());
    require(MH_CreateHook(api[16],reinterpret_cast<void *>(route::copy_hook),&route::originals[2])==MH_OK,"depth copy hook");
    require(MH_CreateHook(api[26],reinterpret_cast<void *>(cd::barrier_hook),&cd::originals[8])==MH_OK,"depth barrier hook");
    require(MH_EnableHook(api[16])==MH_OK&&MH_EnableHook(api[26])==MH_OK,"depth API hook admission");
    auto d=texture(w,h,DXGI_FORMAT_R32_FLOAT);d.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    auto source=resource(f.device.Get(),d,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);
    Pixels input(f.device.Get(),d,D3D12_HEAP_TYPE_UPLOAD);
    std::vector<unsigned> expected(std::size_t(w)*h);
    for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x){const float v=.125F+.25F*(unsigned(x>=w/2)+2U*unsigned(y>=h/2))+float((x+3*y)%128)/1024.F;
        expected[std::size_t(y)*w+x]=std::bit_cast<unsigned>(v);}
    input.write(std::as_bytes(std::span(expected)));
    hr(f.allocator->Reset(),"depth fixture allocator");hr(f.list->Reset(f.allocator.Get(),nullptr),"depth fixture reset");
    obs::reset(f.list.Get(),1,nullptr);input.copy(f.list.Get(),source.Get(),true);
    barrier(f.list.Get(),source.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
    require(!capture.record(f.list.Get(),source.Get(),0,1,71,99),"unknown state rejected before work");
    require(!capture.record(f.list.Get(),source.Get(),0xc0,0,71,99),"missing generation rejected");
    auto wrong=texture(128,64,DXGI_FORMAT_R32_FLOAT);wrong.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    auto small_texture=resource(f.device.Get(),wrong,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
    require(!capture.record(f.list.Get(),small_texture.Get(),0xc0,1,71,99),"wrong dimensions rejected");
    route::injecting=true;cd::injected=true;
    require(capture.record(f.list.Get(),source.Get(),0xc0,1,71,99),"complete native depth copy through hooks");
    require(route::compute_depth_pixels[1].record(f.list.Get(),source.Get(),0xc0,1,72,100),"second bounded depth capture");
    route::injecting=false;cd::injected=false;
    require(!capture.record(f.list.Get(),source.Get(),0xc0,1,72,100),"second copy refused");
    const auto file=out/"synthetic-depth.r32f";
    require(!capture.save(file)&&!std::filesystem::exists(file),"unsubmitted data cannot be mapped");
    require(!route::reset(f.list.Get(),1)&&route::fault.load()==12,"unsubmitted reset rejected");route::fault=0;
    // GPU validation checks that the original shader-resource state returned.
    barrier(f.list.Get(),source.Get(),D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COPY_SOURCE);
    Pixels check(f.device.Get(),d,D3D12_HEAP_TYPE_READBACK);check.copy(f.list.Get(),source.Get(),false);
    hr(f.list->Close(),"depth fixture close");
    const auto id=reinterpret_cast<std::uintptr_t>(f.list.Get());
    const std::array<std::array<std::uint64_t,2>,2> duplicate{{{id,1},{id,1}}};
    require(!route::validate_submission(reinterpret_cast<std::uintptr_t>(f.queue.Get()),duplicate,1),"duplicate generation refused");
    cd::cfg.device=reinterpret_cast<std::uintptr_t>(f.device.Get());cd::fence=f.fence;
    cd::originals[7]=reinterpret_cast<void *>(execute_forward);cd::list_count=1;
    cd::lists[0].value=f.list;cd::lists[0].generation=1;cd::lists[0].closed=true;
    ID3D12CommandList *lists[]{f.list.Get()};cd::execute_hook(f.queue.Get(),1,lists);
    require(!cd::fault.load()&&!route::fault.load()&&cd::route_only_signals==1&&cd::signal_value==1,"diagnostic-only batch signaled by continuous runtime");
    require(!route::validate_submission(reinterpret_cast<std::uintptr_t>(f.queue.Get()),std::span(duplicate).first(1),2),"resubmission refused");
    route::collect(UINT64_MAX);require(!capture.ready(),"device removal is not completion");
    f.wait();capture.collect(f.fence->GetCompletedValue());
    require(!capture.ready()&&!capture.save(file),"completed recording retained until Reset");
    require(check.read()==expected,"copy did not modify native depth");
    hr(f.allocator->Reset(),"depth retirement allocator");hr(f.list->Reset(f.allocator.Get(),nullptr),"depth retirement list");
    require(route::reset(f.list.Get(),1)&&capture.ready(),"native Reset and fence both established");hr(f.list->Close(),"depth retirement close");
    require(!route::compute_depth_pixels[1].ready(),"Reset alone does not publish completion");
    route::collect(f.fence->GetCompletedValue());require(route::compute_depth_pixels[1].ready(),"completion after Reset also accepted");
    require(route::compute_depth_pixels[1].save(out/"synthetic-depth-second.r32f"),"second sample saved");
    require(capture.save(file)&&std::filesystem::file_size(file)==std::uint64_t(w)*h*4,"full depth file saved");
    std::vector<unsigned> actual(expected.size());std::ifstream pixels(file,std::ios::binary);
    require(bool(pixels.read(reinterpret_cast<char *>(actual.data()),static_cast<std::streamsize>(actual.size()*4)))&&actual==expected,"all 3686400 pixels exact including outside active view");
    require(!capture.save(file),"saved evidence not overwritten");route::disabled(true);
    std::ofstream report(out/"depth.json");report<<std::boolalpha;capture.write(report);report<<'\n';require(bool(report),"depth report");
    require(MH_DisableHook(api[16])==MH_OK&&MH_DisableHook(api[26])==MH_OK,"depth API hook removal");
    require(obs::finish(),"depth observer cleanup");f.messages();
    std::cout<<"PASS full 2560x1440 depth, 3686400 exact pixels, original state/content, production submission, duplicate/reset/completion gates and release\n";return 0;
#else
    throw std::runtime_error("use runtime fixture for compute depth");
#endif
}
