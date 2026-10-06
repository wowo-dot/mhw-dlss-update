#include "stream_pass.hpp"
#include <wrl/client.h>
#include <iomanip>
#include <stdexcept>
namespace mhw::aa_control {
namespace {
using Microsoft::WRL::ComPtr;
void require(bool ok,const char *why){if(!ok)throw std::runtime_error(why);}
void transition(ID3D12GraphicsCommandList *list,input_readback::Barrier barrier,ID3D12Resource *r,D3D12_RESOURCE_STATES a,D3D12_RESOURCE_STATES b){
    D3D12_RESOURCE_BARRIER v{};v.Transition={r,UINT_MAX,a,b};barrier(list,1,&v);
}
constexpr std::array<DXGI_FORMAT,3> formats={DXGI_FORMAT_R11G11B10_FLOAT,DXGI_FORMAT_R32_FLOAT,DXGI_FORMAT_R32_UINT};
}
struct StreamPass::Impl {
    ComPtr<ID3D12Device> device;std::array<ComPtr<ID3D12Resource>,3> raw;
    ComPtr<ID3D12Resource> destination;std::unique_ptr<dlss_sdk::InlineFrame> sdk;
    unsigned width{},height{};std::uint64_t frames{};bool issued{},finished{};
    dlss_sdk::RecordAttempt last;std::string failure;
};
StreamPass::StreamPass(ID3D12Device *device,unsigned width,unsigned height,const std::filesystem::path &runtime,
                       const dlss_sdk::BorrowedInitialization &initialization):impl_(std::make_unique<Impl>()){
    auto &p=*impl_;require(device&&width&&height&&UINT64(width)*height<=4194304,"stream dimensions");p.device=device;p.width=width;p.height=height;
    D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;d.Width=width;d.Height=height;d.DepthOrArraySize=d.MipLevels=1;d.SampleDesc.Count=1;
    for(unsigned i=0;i<3;++i){d.Format=formats[i];require(SUCCEEDED(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&p.raw[i]))),"stream raw");}
    d.Format=formats[0];d.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    require(SUCCEEDED(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,nullptr,IID_PPV_ARGS(&p.destination))),"stream private destination");
    p.sdk=std::make_unique<dlss_sdk::InlineFrame>(device,width,height,std::array<ID3D12Resource *,3>{p.raw[0].Get(),p.raw[1].Get(),p.raw[2].Get()},runtime,initialization,true,true);
}
StreamPass::~StreamPass(){if(impl_&&impl_->issued&&!impl_->finished)static_cast<void>(impl_.release());}
bool StreamPass::record(ID3D12GraphicsCommandList *list,input_readback::Barrier barrier,const std::array<ID3D12Resource *,3> &native,
                       const temporal_present::Bindings &bindings,dlss_sdk::HistoryFrame history,bool reset,float ms,dlss_sdk::RecordFault fault,
                       bool(*restored)(ID3D12GraphicsCommandList *,const temporal_present::Bindings &) noexcept) noexcept {
    auto &p=*impl_;p.last={};if(!p.failure.empty()||p.finished)return false;
    try {
        require(list&&barrier&&bindings.ready()&&list->GetType()==D3D12_COMMAND_LIST_TYPE_DIRECT,"stream recording");
        ComPtr<ID3D12Device> owner;require(SUCCEEDED(list->GetDevice(IID_PPV_ARGS(&owner)))&&owner==p.device,"stream list device");
        for(unsigned i=0;i<3;++i){require(native[i]!=nullptr,"stream native input");const auto d=native[i]->GetDesc();owner.Reset();
            require(SUCCEEDED(native[i]->GetDevice(IID_PPV_ARGS(&owner)))&&owner==p.device&&d.Dimension==D3D12_RESOURCE_DIMENSION_TEXTURE2D&&d.Width==p.width&&d.Height==p.height&&
                d.Format==formats[i]&&d.MipLevels==1&&d.DepthOrArraySize==1&&d.SampleDesc.Count==1&&!d.SampleDesc.Quality,"stream input layout");
            for(unsigned j=0;j<i;++j)require(native[i]!=native[j],"stream native alias");}
        p.issued=true;p.last.commands=true;
        for(unsigned i=0;i<3;++i){transition(list,barrier,native[i],D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COPY_SOURCE);
            list->CopyResource(p.raw[i].Get(),native[i]);transition(list,barrier,native[i],D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);}
        const bool ok=p.sdk->record_continuous(list,p.destination.Get(),bindings,history,reset,ms,false,fault);
        p.last.bindings_restored=p.sdk->last_attempt().bindings_restored;
        require(ok&&p.last.bindings_restored&&(!restored||restored(list,bindings)),"stream evaluation/restoration");
        // Packed SDK output is COPY_SOURCE; native scene color is PS-SRV.
        // No fallible allocation/query/shader follows the first native write.
        transition(list,barrier,native[0],D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COPY_DEST);
        list->CopyResource(native[0],p.sdk->private_images()[4]);
        transition(list,barrier,native[0],D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        p.last.native_output_written=p.last.success=true;++p.frames;return true;
    }catch(const std::exception &e){p.failure=e.what();return false;}
}
dlss_sdk::RecordAttempt StreamPass::last_attempt() const noexcept{return impl_->last;}
ID3D12Resource *StreamPass::packed() const noexcept{return impl_->sdk->private_images()[4];}
bool StreamPass::finish(ID3D12Fence *fence,std::uint64_t value,bool retired) noexcept {
    auto &p=*impl_;try {require(p.failure.empty()&&p.frames&&!p.finished,"stream finish");p.sdk->finish(fence,value,retired);p.finished=true;return true;}
    catch(const std::exception &e){p.failure=e.what();return false;}
}
void StreamPass::write(std::ostream &out) const {const auto &p=*impl_;out<<"{\"schema\":1,\"native_handoffs\":"<<p.frames<<",\"finished\":"<<p.finished
    <<",\"native_state\":128,\"failure\":"<<std::quoted(p.failure)<<",\"sdk\":";p.sdk->write(out);out<<'}';}
}
