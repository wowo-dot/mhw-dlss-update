#pragma once
#include "sdk_pass.hpp"
namespace mhw::aa_control::sdk_runtime {
void configure(const std::filesystem::path &);
bool prepare(ID3D12Device *,const std::array<ID3D12Resource *,3> &,std::string &) noexcept;
bool record(unsigned,ID3D12GraphicsCommandList *,input_readback::Barrier,
            const temporal_present::Bindings &,temporal_bindings::jitter::HistorySample) noexcept;
bool finish(bool all_completed_retired) noexcept;
bool save(const std::wstring &);
void write(std::ostream &);
}
