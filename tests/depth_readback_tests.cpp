#include "depth_readback.hpp"
#include <wrl/client.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace dr=mhw::projection_binding::depth_readback;
using Microsoft::WRL::ComPtr;
void check(bool value,const char *why){if(!value)throw std::runtime_error(why);}
template<class T>std::uintptr_t address(T *p){return reinterpret_cast<std::uintptr_t>(p);}
int main(int argc,char **argv) {
  try {
    check(argc==2,"case required");const std::string mode=argv[1];
    if(mode=="extent") {
      check(!dr::Capture::extent(0,16) && !dr::Capture::extent(16,0) && !dr::Capture::extent(8193,1),"empty/oversized extent");
      check(dr::Capture::extent(8192,2048) && !dr::Capture::extent(8192,2049) && !dr::Capture::extent(UINT_MAX,UINT_MAX),"byte bound/overflow");
      std::cout<<"pass: extent\n";return 0;
    }
    ComPtr<ID3D12Device> device;check(SUCCEEDED(D3D12CreateDevice(nullptr,D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device))),"device");
    ComPtr<ID3D12CommandQueue> queue;D3D12_COMMAND_QUEUE_DESC q{};
    check(SUCCEEDED(device->CreateCommandQueue(&q,IID_PPV_ARGS(&queue))),"queue");
    ComPtr<ID3D12CommandAllocator> allocator;ComPtr<ID3D12GraphicsCommandList> list;
    check(SUCCEEDED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator))) &&
      SUCCEEDED(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&list))),"list");
    ComPtr<ID3D12Resource> source;D3D12_RESOURCE_DESC d{};
    d.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;d.Width=17;d.Height=13;d.DepthOrArraySize=1;d.MipLevels=1;
    d.Format=DXGI_FORMAT_R32_TYPELESS;d.SampleDesc.Count=1;d.Flags=D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    D3D12_HEAP_PROPERTIES hp{};hp.Type=D3D12_HEAP_TYPE_DEFAULT;
    check(SUCCEEDED(device->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_DEPTH_WRITE,nullptr,IID_PPV_ARGS(&source))),"source");
    ComPtr<ID3D12DescriptorHeap> heap;D3D12_DESCRIPTOR_HEAP_DESC h{};h.Type=D3D12_DESCRIPTOR_HEAP_TYPE_DSV;h.NumDescriptors=1;
    check(SUCCEEDED(device->CreateDescriptorHeap(&h,IID_PPV_ARGS(&heap))),"heap");
    D3D12_DEPTH_STENCIL_VIEW_DESC view{};view.Format=DXGI_FORMAT_D32_FLOAT;view.ViewDimension=D3D12_DSV_DIMENSION_TEXTURE2D;
    device->CreateDepthStencilView(source.Get(),&view,heap->GetCPUDescriptorHandleForHeapStart());
    list->ClearDepthStencilView(heap->GetCPUDescriptorHandleForHeapStart(),D3D12_CLEAR_FLAG_DEPTH,.375F,0,0,nullptr);
    D3D12_RESOURCE_BARRIER b{};b.Transition.pResource=source.Get();b.Transition.Subresource=UINT_MAX;
    b.Transition.StateBefore=D3D12_RESOURCE_STATE_DEPTH_WRITE;b.Transition.StateAfter=D3D12_RESOURCE_STATE_COPY_SOURCE;
    list->ResourceBarrier(1,&b);
    ComPtr<IUnknown> canonical;check(SUCCEEDED(source.As(&canonical)),"identity");
    const auto refs=source->AddRef();source->Release();
    dr::Capture capture;check(capture.prepare(device.Get(),17,13),"prepare");
    check(!capture.prepare(device.Get(),17,13),"repeated prepare");
    if(mode=="description") {
      check(capture.source_matches(d),"valid description");
      for(unsigned i=0;i<9;++i){auto bad=d;
        switch(i){case 0:++bad.Width;break;case 1:++bad.Height;break;case 2:bad.Format=DXGI_FORMAT_D32_FLOAT;break;
          case 3:bad.SampleDesc.Count=2;break;case 4:bad.MipLevels=2;break;case 5:bad.DepthOrArraySize=2;break;
          case 6:bad.Flags=D3D12_RESOURCE_FLAG_NONE;break;case 7:bad.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;break;case 8:bad.SampleDesc.Quality=1;break;}
        check(!capture.source_matches(bad),"changed description accepted");}
      capture.abort();check(!capture.issued() && capture.fixture_release_after_reset(false),"unissued teardown");
      std::cout<<"pass: description\n";return 0;
    }
    dr::Site site{};site.marker=1;site.reset=2;site.barrier=4;site.copy=6;site.copy_exit=7;site.begin=8;
    site.command=address(list.Get());site.resource=address(source.Get());site.identity=address(canonical.Get());site.allocator=address(allocator.Get());
    // Unique per run (a reused process ID found the old file) and removed on return.
    const auto path=std::filesystem::path("readback-"+mode+"-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64())+".f32");
    struct Remove{std::filesystem::path p;~Remove(){std::error_code ignored;std::filesystem::remove(p,ignored);}} remove_output{path};
    check(!capture.save(path.wstring()),"map before issue");
    if(mode=="identity")++site.identity;
    const bool issued=capture.issue(list.Get(),source.Get(),site);
    if(mode=="identity") {
      check(!issued && !capture.issued(),"identity mismatch emitted command");capture.abort();
      const auto after=source->AddRef();source->Release();check(after==refs,"pre-issue failure pin leak");
      std::cout<<"pass: identity\n";return 0;
    }
    check(issued,"issue");capture.issue_end(9);
    check(!capture.issue(list.Get(),source.Get(),site),"second copy admitted");
    check(!capture.save(path.wstring()) && !capture.fixture_release_after_reset(true),"recorded resources released/mapped without completion");
    check(SUCCEEDED(list->Close()),"close");
    ComPtr<ID3D12Fence> hold,done;check(SUCCEEDED(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&hold))) &&
      SUCCEEDED(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&done))),"test fences");
    check(SUCCEEDED(queue->Wait(hold.Get(),1)),"test-owned GPU delay");
    ID3D12CommandList *batch=list.Get();queue->ExecuteCommandLists(1,&batch);
    site.close=10;site.execute=12;site.execute_exit=13;site.queue=address(queue.Get());site.signal_begin=14;
    if(mode=="signal_failure")capture.fixture_signal_failure();
    if(mode=="wrong_generation")++site.reset;
    const bool submitted=capture.submit(queue.Get(),site);
    if(submitted)capture.signal_end(15);
    check(submitted==(mode!="signal_failure" && mode!="wrong_generation"),"submit contract");
    capture.poll();check(!capture.save(path.wstring()),"mapped before fence completion");
    check(!capture.fixture_release_after_reset(true),"pending buffer released");
    if(mode=="cancel")capture.abort();
    check(SUCCEEDED(queue->Signal(done.Get(),1)) && SUCCEEDED(hold->Signal(1)),"release fixture GPU delay");
    const auto deadline=GetTickCount64()+5000;
    while(done->GetCompletedValue()<1 && GetTickCount64()<deadline)Sleep(1);
    check(done->GetCompletedValue()==1,"fixture GPU retirement");
    capture.poll();
    const bool abandoned=mode=="cancel" || mode=="signal_failure" || mode=="wrong_generation";
    if(abandoned) {
      check(capture.phase()==dr::Phase::quarantined && !capture.save(path.wstring()),"uncertain work was mapped");
      std::ostringstream report;capture.write(report);check(report.str().find("\"resources_retained\":true")!=std::string::npos,"quarantine lost resources");
      std::cout<<"pass: "<<mode<<" (retained until process exit)\n";return 0;
    }
    check(capture.phase()==dr::Phase::complete,"private completion");
    check(!capture.save(path.wstring()),"mapped before recording retirement");
    check(SUCCEEDED(allocator->Reset()) && SUCCEEDED(list->Reset(allocator.Get(),nullptr)),"discard completed recording");
    capture.retire(16,17);
    if(mode=="map_failure")capture.fixture_map_failure();
    const bool saved=capture.save(path.wstring());check(saved==(mode!="map_failure"),"save result");
    if(saved) {
      check(std::filesystem::file_size(path)==17*13*4,"padding leaked into output");
      std::ifstream in(path,std::ios::binary);std::vector<float> pixels(17*13);
      in.read(reinterpret_cast<char *>(pixels.data()),static_cast<std::streamsize>(pixels.size()*4));
      check(static_cast<bool>(in),"pixel bytes");for(float v:pixels)check(v==.375F,"GPU depth values");
      check(!capture.save(path.wstring()),"second save/overwrite");
    }
    check(!capture.fixture_release_after_reset(false),"closed list could be resubmitted");
    check(capture.fixture_release_after_reset(true),"fixture safe retirement");
    const auto after=source->AddRef();source->Release();check(after==refs,"retired source ref balance");
    std::cout<<"pass: "<<mode<<" (221 GPU depth pixels, padded rows)\n";return 0;
  } catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
