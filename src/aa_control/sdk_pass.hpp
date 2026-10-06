#pragma once
#include "input_readback.hpp"
#include "../dlss_sdk/inline.hpp"
#include "../temporal_bindings/jitter.hpp"

namespace mhw::aa_control {
// Eight-frame private DLAA evaluation at the AA-Off PS-SRV read point.
// Every previous evaluation must complete and retire before resources/history
// are reused. This diagnostic deliberately does not replace native color.
class SdkPass {
public:
    SdkPass(ID3D12Device *,const std::array<ID3D12Resource *,3> &,
            const std::filesystem::path &,const dlss_sdk::BorrowedInitialization &);
    ~SdkPass();
    bool record(unsigned,ID3D12GraphicsCommandList *,input_readback::Barrier,
                const temporal_present::Bindings &,temporal_bindings::jitter::HistorySample,
                bool previous_completed,dlss_sdk::RecordFault=dlss_sdk::RecordFault::none) noexcept;
    bool finish(ID3D12Fence *,bool all_completed_retired) noexcept;
    bool save(const std::wstring &);
    void write(std::ostream &) const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
