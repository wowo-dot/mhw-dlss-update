#pragma once
#include "sdk_pass.hpp"
namespace mhw::aa_control {
// Renderer-independent streaming pass. Caller serializes recording and GPU
// execution, proves PS-SRV states, and retains native resources/list through
// successful Reset AND GPU completion. No per-frame allocation or readback.
class StreamPass {
public:
    StreamPass(ID3D12Device *,unsigned,unsigned,const std::filesystem::path &,
               const dlss_sdk::BorrowedInitialization &);
    ~StreamPass();
    bool record(ID3D12GraphicsCommandList *,input_readback::Barrier,
                const std::array<ID3D12Resource *,3> &,const temporal_present::Bindings &,
                dlss_sdk::HistoryFrame,bool reset,float milliseconds,
                dlss_sdk::RecordFault=dlss_sdk::RecordFault::none,
                bool(*restored)(ID3D12GraphicsCommandList *,const temporal_present::Bindings &) noexcept=nullptr) noexcept;
    dlss_sdk::RecordAttempt last_attempt() const noexcept;
    ID3D12Resource *packed() const noexcept;
    bool finish(ID3D12Fence *,std::uint64_t,bool retired) noexcept;
    void write(std::ostream &) const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
