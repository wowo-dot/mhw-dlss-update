#include "../tests/scaled_effect_fixture.hpp"
#include "../tests/hook_queue_fixture.hpp"
namespace mhw::fixtures::effects {
void run(int argc,wchar_t **argv){const bool warp=argc==4&&std::wstring_view(argv[1])==L"--warp";require(warp||(argc==6&&std::wstring_view(argv[1])==L"--hardware"),"usage: --warp capture-prefix output-dir OR --hardware luid-low luid-high capture-prefix output-dir");
    LUID requested{};if(!warp){requested.LowPart=std::stoul(argv[2]);requested.HighPart=std::stol(argv[3]);}
    const std::filesystem::path prefix=argv[warp?2:4],out=argv[warp?3:5];require(!std::filesystem::exists(out),"preserve results");std::filesystem::create_directories(out);
    Fixture f(warp,requested,prefix);hr(MH_Initialize()==MH_OK?S_OK:E_FAIL,"observer hooks");mhw::fixtures::QueuedHookIsolation queued;std::string reason;
    const auto resolver=[](std::uintptr_t p,unsigned slot,std::uintptr_t &target)noexcept{target=(*reinterpret_cast<std::uintptr_t **>(p))[slot];return target!=0;};
    require(mhw::temporal_present::observer::install(f.device.Get(),resolver,reason,true,ViewportFeed::observe,true,true),reason.c_str());queued.unchanged();f.viewport_listener_tests();f.track_raster=true;
    const auto raster_checks=f.raster_tests();std::ofstream cases(out/"cases.jsonl");unsigned draws=0;UINT64 comparisons=0;
    const auto compare=[&](const auto &a,const auto &b,const char *why){require(a.size()==b.size(),"pixel lengths");for(std::size_t i=0;i<a.size();++i)if(a[i]!=b[i]){std::cerr<<why<<" pixel="<<i<<" actual="<<a[i]<<" expected="<<b[i]<<'\n';throw std::runtime_error(why);}comparisons+=a.size();};
    for(unsigned scale:{1U,2U})for(unsigned stage=0;stage<3;++stage)for(unsigned scenario=0;scenario<(stage==1?4U:1U);++scenario){const sf::FilterGeometry g{64,32,64*scale,32*scale};
        const auto expected=f.draw(stage,g,scenario,false,false),actual=f.draw(stage,g,scenario,true,false);compare(actual,expected,"full output mismatch");draws+=2;
        const auto expected_guard=f.draw(stage,g,scenario,false,true),actual_guard=f.draw(stage,g,scenario,true,true);compare(actual_guard,expected_guard,"restored native draw mismatch");draws+=3;
        cases<<"{\"scale\":"<<scale<<",\"stage\":"<<stage<<",\"scenario\":"<<scenario<<",\"full_and_restored_exact\":true}\n";cases.flush();}
    const sf::FilterGeometry g{64,32,128,64};std::vector<unsigned> actual,expected;
    for(unsigned stage=0;stage<3;++stage){actual=f.draw(stage,g,1,true,false,actual);expected=f.draw(stage,g,1,false,false,expected);compare(actual,expected,"chained output mismatch");draws+=2;}
    std::ofstream image(out/"chain.r11g11b10",std::ios::binary);image.write(reinterpret_cast<const char *>(actual.data()),static_cast<std::streamsize>(actual.size()*4));require(bool(image),"image save");f.messages();
    require(mhw::temporal_present::observer::finish(),"raster observer cleanup");
    queued.unchanged();queued.apply_and_check();
    const auto stopped=ViewportFeed::calls;hr(f.list->Reset(f.allocator.Get(),nullptr),"disabled listener reset");const D3D12_VIEWPORT view{0,0,128,64,0,1};f.list->RSSetViewports(1,&view);hr(f.list->Close(),"disabled listener close");require(ViewportFeed::calls==stopped,"disabled listener cannot notify");f.messages();
    std::ofstream observer(out/"observer.json");observer<<std::boolalpha;mhw::temporal_present::observer::write(observer);require(bool(observer),"observer report");
    std::ofstream result(out/"result.json");result<<"{\"passed\":true,\"warp\":"<<(warp?"true":"false")<<",\"adapter_luid\":["<<f.luid.HighPart<<','<<f.luid.LowPart<<"],\"vendor\":"<<f.vendor<<",\"device\":"<<f.device_id<<",\"draws\":"<<draws<<",\"packed_pixel_comparisons\":"<<comparisons<<",\"rejections\":"<<f.rejections<<",\"gpu_warnings_errors\":0,\"recordings_retired\":true,\"sdk_evaluated\":false}\n";require(bool(result),"result save");
    std::cout<<"PASS: full native effects, restored native draws, chained output; "<<comparisons<<" exact packed pixels; "<<raster_checks<<" raster admission checks; shared viewport listener lifecycle passed\n";
}
}
int wmain(int argc,wchar_t **argv){try{mhw::fixtures::effects::run(argc,argv);return 0;}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
