#include "input_readback.hpp"
#include "../projection_binding/memory.hpp"
#include <filesystem>
#include <ostream>
namespace mhw::aa_control::input_readback {
namespace {
struct Image {
    ID3D12Resource *buffer{};D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    UINT rows{};UINT64 row_bytes{},bytes{};bool attempted{},transitioned{},copied{},restored{},saved{};
    unsigned map_result{0x8000000a};std::string file,hash;
};
std::array<std::array<Image,3>,3> images;
std::array<ID3D12Resource *,3> sources{};
bool prepared{},safe{},released{};
void release() noexcept {
    for(auto &frame:images)for(auto &v:frame)if(v.buffer){v.buffer->Release();v.buffer=nullptr;}
    for(auto &v:sources)if(v){v->Release();v=nullptr;}released=true;
}
bool copy(Image &v,ID3D12Resource *source,ID3D12GraphicsCommandList *list,Barrier barrier) noexcept {
    D3D12_RESOURCE_BARRIER b{};b.Transition={source,UINT_MAX,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COPY_SOURCE};
    D3D12_TEXTURE_COPY_LOCATION src{},dst{};src.pResource=source;src.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst.pResource=v.buffer;dst.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;dst.PlacedFootprint=v.footprint;
    v.attempted=true;
    __try {
        __try {barrier(list,1,&b);v.transitioned=true;list->CopyTextureRegion(&dst,0,0,0,&src,nullptr);v.copied=true;}
        __finally {if(v.transitioned){b.Transition.StateBefore=D3D12_RESOURCE_STATE_COPY_SOURCE;b.Transition.StateAfter=D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            barrier(list,1,&b);v.restored=true;}}
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
    return v.copied&&v.restored;
}
bool save_image(Image &v,const std::wstring &name) {
    const auto file=CreateFileW(name.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    void *memory{};D3D12_RANGE read{0,static_cast<SIZE_T>(v.bytes)};
    v.map_result=static_cast<unsigned>(v.buffer->Map(0,&read,&memory));bool ok=v.map_result<0x80000000&&memory;
    if(ok){for(unsigned y=0;y<v.rows&&ok;++y){DWORD written{};
        ok=WriteFile(file,static_cast<const char *>(memory)+std::size_t(y)*v.footprint.Footprint.RowPitch,
            static_cast<DWORD>(v.row_bytes),&written,nullptr)&&written==v.row_bytes;}
        D3D12_RANGE empty{};v.buffer->Unmap(0,&empty);}
    const bool flushed=FlushFileBuffers(file)!=0,closed=CloseHandle(file)!=0;
    if(!(ok&&flushed&&closed))return false;
    v.file=std::filesystem::path(name).filename().string();v.hash=projection_binding::file_sha256(name);
    v.saved=v.hash.size()==64;return v.saved;
}
}
bool prepare(ID3D12Device *device,const std::array<ID3D12Resource *,3> &input) noexcept {
    if(prepared||released||!device)return false;
    constexpr std::array<unsigned,3> formats={26,41,42};
    for(unsigned role=0;role<3;++role){
        auto *source=input[role];if(!source){release();return false;}const auto desc=source->GetDesc();
        if(desc.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D||desc.Width!=2560||desc.Height!=1440||desc.DepthOrArraySize!=1||desc.MipLevels!=1||
           desc.SampleDesc.Count!=1||desc.SampleDesc.Quality||static_cast<unsigned>(desc.Format)!=formats[role]){release();return false;}
        sources[role]=source;source->AddRef();
        for(auto &frame:images){auto &v=frame[role];device->GetCopyableFootprints(&desc,0,1,0,&v.footprint,&v.rows,&v.row_bytes,&v.bytes);
            const auto &f=v.footprint.Footprint;
            if(v.footprint.Offset||v.rows!=1440||v.row_bytes!=10240||v.bytes!=14745600||f.RowPitch!=10240||f.Width!=2560||f.Height!=1440||f.Depth!=1||f.Format!=desc.Format){release();return false;}
            D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_READBACK;
            D3D12_RESOURCE_DESC buffer{};buffer.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;buffer.Width=v.bytes;buffer.Height=1;
            buffer.DepthOrArraySize=buffer.MipLevels=1;buffer.SampleDesc.Count=1;buffer.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            if(FAILED(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&buffer,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&v.buffer)))){release();return false;}
        }
    }
    prepared=true;return true;
}
bool selected(unsigned receipt) noexcept {for(auto v:receipts)if(v==receipt)return true;return false;}
bool record(unsigned receipt,ID3D12GraphicsCommandList *list,Barrier barrier) noexcept {
    if(!prepared||!list||!barrier||!selected(receipt))return false;
    unsigned index{};while(receipts[index]!=receipt)++index;
    for(const auto &v:images[index])if(v.attempted)return false;
    std::uintptr_t target{};
    if(!projection_binding::system_method(reinterpret_cast<std::uintptr_t>(list),16,target))return false;
    for(unsigned role=0;role<3;++role)if(!copy(images[index][role],sources[role],list,barrier))return false;
    return true;
}
bool recorded() noexcept {
    if(!prepared)return false;
    for(const auto &frame:images)for(const auto &v:frame)if(!v.attempted||!v.transitioned||!v.copied||!v.restored)return false;
    return true;
}
void retire(bool complete) noexcept {
    safe=complete;
    bool attempted{};for(const auto &frame:images)for(const auto &v:frame)attempted|=v.attempted;
    if(!attempted)release();
}
bool save(const std::wstring &prefix) {
    if(!safe||released||!recorded())return false;
    for(unsigned frame=0;frame<3;++frame)for(unsigned role=0;role<3;++role)
        if(!save_image(images[frame][role],prefix+L"-"+std::to_wstring(receipts[frame])+L"-"+std::to_wstring(role)+L".bin"))return false;
    release();return true;
}
void write(std::ostream &out) {
    out<<"{\"event\":\"input_readbacks\",\"prepared\":"<<prepared<<",\"gpu_safe\":"<<safe<<",\"released\":"<<released<<",\"images\":[";
    for(unsigned frame=0;frame<3;++frame)for(unsigned role=0;role<3;++role){const auto &v=images[frame][role];
        out<<(frame||role?",":"")<<"{\"receipt\":"<<receipts[frame]<<",\"role\":"<<role<<",\"attempted\":"<<v.attempted<<",\"transitioned\":"<<v.transitioned
            <<",\"copied\":"<<v.copied<<",\"restored\":"<<v.restored<<",\"saved\":"<<v.saved<<",\"map_result\":"<<v.map_result
            <<",\"format\":"<<v.footprint.Footprint.Format<<",\"row_pitch\":"<<v.footprint.Footprint.RowPitch<<",\"bytes\":"<<v.bytes
            <<",\"state_before\":128,\"copy_state\":2048,\"state_after\":128,\"file\":\""<<v.file<<"\",\"sha256\":\""<<v.hash<<"\"}";
    }out<<"]}\n";
}
}
