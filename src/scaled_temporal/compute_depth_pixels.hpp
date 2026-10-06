#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <array>
#include <filesystem>
#include <ostream>
#include <span>
#include <utility>

namespace mhw::scaled_temporal {
// Caller serializes access. Two instances only, allocated before hook admission.
// A copy is inserted immediately before the observed native depth reduction.
// GPU completion AND native Reset are required before CPU readback/release.
class ComputeDepthPixels {
    template<class T>using Ptr=Microsoft::WRL::ComPtr<T>;
    Ptr<ID3D12Resource> buffer_,source_;
    Ptr<ID3D12GraphicsCommandList> list_;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint_{};
    std::uint64_t bytes_{},row_bytes_{},list_id_{},generation_{},resource_{},frame_{},packet_{},queue_{},signal_{},completed_{};
    unsigned width_{},height_{},state_{},error_{};
    bool attempted_{},recorded_{},reset_{},saved_{},released_{};
public:
    bool prepare(ID3D12Device *device,unsigned w,unsigned h) noexcept {
        if(buffer_||attempted_||!device||!w||!h||w>8192||h>8192||std::uint64_t(w)*h*4>64*1024*1024)return false;
        width_=w;height_=h;
        D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;d.Width=w;d.Height=h;
        d.DepthOrArraySize=d.MipLevels=1;d.SampleDesc.Count=1;d.Format=DXGI_FORMAT_R32_FLOAT;d.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        unsigned rows{};device->GetCopyableFootprints(&d,0,1,0,&footprint_,&rows,&row_bytes_,&bytes_);
        const auto &f=footprint_.Footprint;
        if(footprint_.Offset||rows!=h||row_bytes_!=std::uint64_t(w)*4||f.Width!=w||f.Height!=h||f.Depth!=1||
           f.Format!=d.Format||f.RowPitch%256||f.RowPitch<row_bytes_||bytes_!=std::uint64_t(f.RowPitch)*(h-1)+row_bytes_||bytes_>64*1024*1024)return false;
        d={};d.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;d.Width=bytes_;d.Height=1;d.DepthOrArraySize=d.MipLevels=1;
        d.SampleDesc.Count=1;d.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        D3D12_HEAP_PROPERTIES hp{};hp.Type=D3D12_HEAP_TYPE_READBACK;
        return SUCCEEDED(device->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&buffer_)));
    }
    bool available()const noexcept{return buffer_&&!attempted_&&!error_;}
    void reject(unsigned e)noexcept{if(!attempted_&&!error_)error_=e;}
    bool record(ID3D12GraphicsCommandList *list,ID3D12Resource *source,unsigned state,std::uint64_t gen,std::uint64_t frame,std::uint64_t packet)noexcept {
        if(!available()||!list||!source||!gen||!frame||!packet||(state!=0x40&&state!=0xc0))return false;
        const auto d=source->GetDesc();
        if(d.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D||d.Width!=width_||d.Height!=height_||d.DepthOrArraySize!=1||d.MipLevels!=1||
           d.Format!=DXGI_FORMAT_R32_FLOAT||d.SampleDesc.Count!=1||d.SampleDesc.Quality||d.Layout!=D3D12_TEXTURE_LAYOUT_UNKNOWN||d.Flags!=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)return false;
        source_=source;list_=list;list_id_=reinterpret_cast<std::uintptr_t>(list);resource_=reinterpret_cast<std::uintptr_t>(source);
        generation_=gen;frame_=frame;packet_=packet;state_=state;
        D3D12_RESOURCE_BARRIER b{};b.Transition={source,0,static_cast<D3D12_RESOURCE_STATES>(state),D3D12_RESOURCE_STATE_COPY_SOURCE};
        D3D12_TEXTURE_COPY_LOCATION src{},dst{};src.pResource=source;src.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst.pResource=buffer_.Get();dst.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;dst.PlacedFootprint=footprint_;
        attempted_=true; // Retain even if a void API fails after partially recording.
        list->ResourceBarrier(1,&b);list->CopyTextureRegion(&dst,0,0,0,&src,nullptr);
        std::swap(b.Transition.StateBefore,b.Transition.StateAfter);list->ResourceBarrier(1,&b);recorded_=true;return true;
    }
    bool tracked(std::uint64_t list,std::uint64_t gen)const noexcept{return attempted_&&!reset_&&list_id_==list&&generation_==gen;}
    bool validate(std::uint64_t queue,std::span<const std::array<std::uint64_t,2>> batch,std::uint64_t signal)const noexcept {
        unsigned found{};for(const auto &b:batch)if(tracked(b[0],b[1]))++found;
        return !found||(found==1&&recorded_&&!signal_&&queue&&signal&&signal!=UINT64_MAX);
    }
    void submitted(std::uint64_t queue,std::span<const std::array<std::uint64_t,2>> batch,std::uint64_t signal)noexcept {
        for(const auto &b:batch)if(tracked(b[0],b[1])){queue_=queue;signal_=signal;}
    }
    bool reset(std::uint64_t list,std::uint64_t gen)noexcept {
        if(!tracked(list,gen))return true;if(!recorded_||!signal_)return false;reset_=true;return true;
    }
    void collect(std::uint64_t completed)noexcept{if(signal_&&completed!=UINT64_MAX&&completed>=signal_)completed_=signal_;}
    bool pending_submission()const noexcept{return attempted_&&!signal_;}
    bool ready()const noexcept{return recorded_&&reset_&&signal_&&completed_==signal_;}
    void disable()noexcept{if(!attempted_){buffer_.Reset();released_=true;}}
    bool save(const std::filesystem::path &path)noexcept {
        if(!ready()||saved_||error_||!buffer_)return false;
        // CREATE_NEW prevents accidental replacement of earlier evidence.
        const HANDLE file=CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(file==INVALID_HANDLE_VALUE){error_=10;return false;}
        void *mapped{};D3D12_RANGE range{0,static_cast<SIZE_T>(bytes_)};
        if(FAILED(buffer_->Map(0,&range,&mapped))||!mapped){CloseHandle(file);error_=11;return false;}
        bool ok=true;
        for(unsigned y=0;y<height_&&ok;++y){DWORD written{};
            ok=WriteFile(file,static_cast<const char *>(mapped)+SIZE_T(y)*footprint_.Footprint.RowPitch,static_cast<DWORD>(row_bytes_),&written,nullptr)&&written==row_bytes_;}
        ok=FlushFileBuffers(file)&&ok;CloseHandle(file);D3D12_RANGE none{};buffer_->Unmap(0,&none);
        if(!ok){error_=12;return false;}saved_=true;buffer_.Reset();source_.Reset();list_.Reset();released_=true;return true;
    }
    void write(std::ostream &o)const {
        o<<"{\"error\":"<<error_<<",\"attempted\":"<<attempted_<<",\"recorded\":"<<recorded_<<",\"reset\":"<<reset_<<",\"saved\":"<<saved_<<",\"released\":"<<released_
         <<",\"list\":"<<list_id_<<",\"generation\":"<<generation_<<",\"resource\":"<<resource_<<",\"frame\":"<<frame_<<",\"packet\":"<<packet_<<",\"state\":"<<state_
         <<",\"queue\":"<<queue_<<",\"signal\":"<<signal_<<",\"completed\":"<<completed_<<",\"width\":"<<width_<<",\"height\":"<<height_
         <<",\"format\":41,\"subresource\":0,\"row_pitch\":"<<footprint_.Footprint.RowPitch<<",\"allocation_bytes\":"<<bytes_<<",\"pixel_bytes\":"<<(saved_?row_bytes_*height_:0)<<'}';
    }
};
}
