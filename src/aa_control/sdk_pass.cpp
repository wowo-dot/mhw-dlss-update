#include "sdk_pass.hpp"
#include "../projection_binding/memory.hpp"
#include <wrl/client.h>
#include <bit>
#include <fstream>
#include <iomanip>
#include <stdexcept>

namespace mhw::aa_control {
namespace {
using Microsoft::WRL::ComPtr;
void require(bool ok,const char *why){if(!ok)throw std::runtime_error(why);}
void hr(HRESULT code,const char *why){require(SUCCEEDED(code),why);}
ComPtr<ID3D12Resource> allocate(ID3D12Device *device,D3D12_RESOURCE_DESC desc,D3D12_HEAP_TYPE type,D3D12_RESOURCE_STATES state){
    D3D12_HEAP_PROPERTIES heap{};heap.Type=type;ComPtr<ID3D12Resource> value;
    hr(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,state,nullptr,IID_PPV_ARGS(&value)),"AA SDK allocation");return value;
}
void transition(ID3D12GraphicsCommandList *list,input_readback::Barrier barrier,ID3D12Resource *resource,D3D12_RESOURCE_STATES a,D3D12_RESOURCE_STATES b){
    D3D12_RESOURCE_BARRIER item{};item.Transition={resource,UINT_MAX,a,b};barrier(list,1,&item);
}
struct Image {
    ComPtr<ID3D12Resource> buffer;D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout{};
    UINT rows{};UINT64 row_bytes{},bytes{};bool copied{},saved{};std::string file,hash;
    void prepare(ID3D12Device *device,const D3D12_RESOURCE_DESC &desc){
        device->GetCopyableFootprints(&desc,0,1,0,&layout,&rows,&row_bytes,&bytes);
        require(!layout.Offset&&rows==desc.Height&&bytes&&bytes<=80ULL*1024*1024,"AA SDK footprint");
        D3D12_RESOURCE_DESC b{};b.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;b.Width=bytes;b.Height=1;
        b.DepthOrArraySize=b.MipLevels=1;b.SampleDesc.Count=1;b.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        buffer=allocate(device,b,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
    }
    void copy(ID3D12GraphicsCommandList *list,ID3D12Resource *source){
        D3D12_TEXTURE_COPY_LOCATION a{},b{};a.pResource=source;a.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        b.pResource=buffer.Get();b.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;b.PlacedFootprint=layout;
        list->CopyTextureRegion(&b,0,0,0,&a,nullptr);copied=true;
    }
    void save(const std::wstring &name){
        require(copied&&!saved&&!std::filesystem::exists(name),"AA SDK preserve image");
        void *memory{};D3D12_RANGE read{0,static_cast<SIZE_T>(bytes)};
        hr(buffer->Map(0,&read,&memory),"AA SDK map");
        std::ofstream out(name,std::ios::binary);
        for(unsigned y=0;y<rows&&out;++y)out.write(static_cast<const char *>(memory)+SIZE_T(y)*layout.Footprint.RowPitch,static_cast<std::streamsize>(row_bytes));
        out.flush();bool ok=bool(out);out.close();ok=ok&&!out.fail();D3D12_RANGE none{};buffer->Unmap(0,&none);
        require(ok,"AA SDK write");file=std::filesystem::path(name).filename().string();hash=projection_binding::file_sha256(name);
        require(hash.size()==64,"AA SDK image hash");saved=true;
    }
};
}
struct SdkPass::Impl {
    std::array<ComPtr<ID3D12Resource>,3> native,raw;
    ComPtr<ID3D12Resource> destination;
    std::unique_ptr<dlss_sdk::InlineFrame> sdk;
    struct Receipt {std::uint64_t frame{},epoch{},generation{},begin{},end{};std::array<unsigned,2> jitter{};
        std::array<float,2> sdk_jitter{};bool attempted{},previous_completed{},inputs_restored{},bindings_restored{},success{};};
    std::array<Receipt,8> receipts;
    std::array<std::array<Image,5>,3> images;
    unsigned count{};bool issued{},finished{},saved{};std::string failure;
};
SdkPass::SdkPass(ID3D12Device *device,const std::array<ID3D12Resource *,3> &native,const std::filesystem::path &runtime,
                 const dlss_sdk::BorrowedInitialization &initialization):impl_(std::make_unique<Impl>()){
    auto &p=*impl_;require(device&&native[0],"AA SDK device/input");
    const auto first=native[0]->GetDesc();constexpr std::array<DXGI_FORMAT,3> formats={DXGI_FORMAT_R11G11B10_FLOAT,DXGI_FORMAT_R32_FLOAT,DXGI_FORMAT_R32_UINT};
    for(unsigned i=0;i<3;++i){require(native[i]!=nullptr,"AA SDK input");const auto d=native[i]->GetDesc();
        ComPtr<ID3D12Device> owner;hr(native[i]->GetDevice(IID_PPV_ARGS(&owner)),"AA SDK input device");
        require(owner.Get()==device&&d.Dimension==D3D12_RESOURCE_DIMENSION_TEXTURE2D&&d.Width==first.Width&&d.Height==first.Height&&
            d.MipLevels==1&&d.DepthOrArraySize==1&&d.SampleDesc.Count==1&&!d.SampleDesc.Quality&&d.Format==formats[i],"AA SDK input layout/device");
        for(unsigned j=0;j<i;++j)require(native[i]!=native[j],"AA SDK input alias");
        p.native[i]=native[i];auto copy=d;copy.Flags=D3D12_RESOURCE_FLAG_NONE;copy.Layout=D3D12_TEXTURE_LAYOUT_UNKNOWN;
        p.raw[i]=allocate(device,copy,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);
    }
    auto output=first;output.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;output.Layout=D3D12_TEXTURE_LAYOUT_UNKNOWN;
    p.destination=allocate(device,output,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    p.sdk=std::make_unique<dlss_sdk::InlineFrame>(device,static_cast<unsigned>(first.Width),first.Height,
        std::array<ID3D12Resource *,3>{p.raw[0].Get(),p.raw[1].Get(),p.raw[2].Get()},runtime,initialization,true);
    const auto images=p.sdk->private_images();for(auto &frame:p.images)for(unsigned i=0;i<5;++i)frame[i].prepare(device,images[i]->GetDesc());
}
SdkPass::~SdkPass(){if(impl_&&impl_->issued&&!impl_->finished)static_cast<void>(impl_.release());}
bool SdkPass::record(unsigned receipt,ID3D12GraphicsCommandList *list,input_readback::Barrier barrier,const temporal_present::Bindings &bindings,
                     temporal_bindings::jitter::HistorySample history,bool previous_completed,dlss_sdk::RecordFault fault) noexcept {
    auto &p=*impl_;if(!p.failure.empty()||p.finished)return false;
    try {
        require(receipt==p.count&&receipt<8&&list&&barrier&&bindings.ready()&&history.frame&&history.epoch,"AA SDK receipt contract");
        require(!receipt||(previous_completed&&history.frame==p.receipts[receipt-1].frame+1&&history.epoch==p.receipts[receipt-1].epoch+1),"AA SDK history/completion");
        auto &r=p.receipts[receipt];r.frame=history.frame;r.epoch=history.epoch;r.generation=bindings.generation;r.jitter=history.jitter_bits;
        r.previous_completed=previous_completed;r.attempted=true;r.begin=GetTickCount64();
        const float x=std::bit_cast<float>(history.jitter_bits[0]),y=std::bit_cast<float>(history.jitter_bits[1]);
        require(std::isfinite(x)&&std::isfinite(y)&&std::abs(x)<=.5F&&std::abs(y)<=.5F,"AA SDK jitter");
        // The captured projection scalar is a clip-space shift of j/extent.
        // Convert to actual raster pixels (+X right, +Y down), as in the
        // previously verified DLAA route: half X, inverted half Y.
        r.sdk_jitter={x*.5F,-y*.5F};
        p.issued=true;
        for(unsigned i=0;i<3;++i){transition(list,barrier,p.native[i].Get(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COPY_SOURCE);
            list->CopyResource(p.raw[i].Get(),p.native[i].Get());
            transition(list,barrier,p.native[i].Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);}
        r.inputs_restored=true;
        r.success=p.sdk->record_continuous(list,p.destination.Get(),bindings,{history.frame,history.epoch,r.sdk_jitter[0],r.sdk_jitter[1]},receipt==0,dlss_sdk::diagnostic_frame_ms,false,fault);
        r.bindings_restored=p.sdk->last_attempt().bindings_restored;require(r.success&&r.bindings_restored,"AA SDK evaluation/restoration");
        if(receipt==0||receipt==3||receipt==7){
            unsigned at{};while(input_readback::receipts[at]!=receipt)++at;const auto images=p.sdk->private_images();
            for(unsigned i=0;i<5;++i){if(i<3)transition(list,barrier,images[i],D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COPY_SOURCE);
                p.images[at][i].copy(list,images[i]);
                if(i<3)transition(list,barrier,images[i],D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);}
        }
        r.end=GetTickCount64();++p.count;return true;
    }catch(const std::exception &e){p.failure=e.what();return false;}
}
bool SdkPass::finish(ID3D12Fence *fence,bool all_completed_retired) noexcept {
    auto &p=*impl_;if(p.finished)return true;
    try {require(p.failure.empty()&&p.count==8&&all_completed_retired,"AA SDK completion contract");
        p.sdk->finish(fence,1,true);p.finished=true;return true;
    }catch(const std::exception &e){p.failure=e.what();return false;}
}
bool SdkPass::save(const std::wstring &prefix){
    auto &p=*impl_;try {require(p.finished&&!p.saved,"AA SDK save contract");
        for(unsigned frame=0;frame<3;++frame)for(unsigned role=0;role<5;++role)
            p.images[frame][role].save(prefix+L"-"+std::to_wstring(input_readback::receipts[frame])+L"-"+std::to_wstring(role)+L".bin");
        p.saved=true;return true;
    }catch(const std::exception &e){p.failure=e.what();return false;}
}
void SdkPass::write(std::ostream &out) const {
    const auto &p=*impl_;out<<std::setprecision(9)<<"{\"event\":\"aa_sdk\",\"schema\":1,\"native_output_written\":false,\"count\":"<<p.count
        <<",\"finished\":"<<p.finished<<",\"saved\":"<<p.saved<<",\"failure\":"<<std::quoted(p.failure)<<",\"private_destination\":"<<reinterpret_cast<std::uintptr_t>(p.destination.Get())<<",\"receipts\":[";
    for(unsigned i=0;i<8;++i){const auto &r=p.receipts[i];out<<(i?",":"")<<"{\"receipt\":"<<i<<",\"frame\":"<<r.frame<<",\"epoch\":"<<r.epoch<<",\"generation\":"<<r.generation
        <<",\"begin_tick\":"<<r.begin<<",\"end_tick\":"<<r.end<<",\"jitter_bits\":["<<r.jitter[0]<<','<<r.jitter[1]<<"],\"attempted\":"<<r.attempted
        <<",\"sdk_jitter\":["<<r.sdk_jitter[0]<<','<<r.sdk_jitter[1]<<"],\"previous_completed\":"<<r.previous_completed<<",\"inputs_restored\":"<<r.inputs_restored<<",\"bindings_restored\":"<<r.bindings_restored<<",\"success\":"<<r.success<<'}';}
    out<<"],\"images\":[";for(unsigned f=0;f<3;++f)for(unsigned i=0;i<5;++i){const auto &v=p.images[f][i];out<<(f||i?",":"")<<"{\"receipt\":"<<input_readback::receipts[f]<<",\"role\":"<<i
        <<",\"format\":"<<v.layout.Footprint.Format<<",\"bytes\":"<<v.row_bytes*v.rows<<",\"copied\":"<<v.copied<<",\"saved\":"<<v.saved<<",\"file\":"<<std::quoted(v.file)<<",\"sha256\":"<<std::quoted(v.hash)<<'}';}
    out<<"],\"sdk\":";p.sdk->write(out);out<<"}\n";
}
}
