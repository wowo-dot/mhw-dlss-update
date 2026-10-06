#pragma once
#include "../dlss_sdk/inline.hpp"
#include <cstdint>
#include <ostream>

namespace mhw::continuous_dlaa {
enum class ResolveOutcome {rejected,reconstructed,unfiltered,recovered,unrecoverable};
struct ResolveResult {
    ResolveOutcome outcome{ResolveOutcome::rejected};
    dlss_sdk::RecordAttempt sdk{};
    bool output_written() const noexcept {
        return outcome==ResolveOutcome::reconstructed||outcome==ResolveOutcome::unfiltered||outcome==ResolveOutcome::recovered;
    }
};
// One already-identified native resolve. Raw color is the unfiltered current
// frame in COPY_DEST; destination is the native full-resolution UAV. Caller
// proves states, frame association, immutable bindings, and retains all objects
// through Reset AND GPU completion. No native TAA dispatch is callable here.
// Preflight rejection records no commands. A partial SDK failure overwrites the
// entire output with raw color after binding restoration; it cannot undo raster
// jitter. Feature-wide activation/latched failure belongs to the renderer owner.
ResolveResult record_resolve(dlss_sdk::InlineFrame &,ID3D12GraphicsCommandList *,
    ID3D12Resource *raw_color,ID3D12Resource *output,unsigned width,unsigned height,
    const temporal_present::Bindings &,dlss_sdk::HistoryFrame,bool reset,float frame_ms,
    bool evaluate=true,bool capture=false,dlss_sdk::RecordFault=dlss_sdk::RecordFault::none) noexcept;

struct DispatchResult {bool claimed{};ResolveResult resolve;};
// Only a caller that has qualified the entire invocation may claim it. Once
// claimed, no failed replacement/recovery can fall through into native TAA.
// Unclaimed/unrelated work keeps its original rendering path. This dispatch
// transaction alone is NOT a feature-wide exclusive-AA controller.
template<class Replace,class Native>
DispatchResult dispatch_resolve(bool claimed,Replace replace,Native native){
    if(!claimed){native();return {};}
    return {true,replace()};
}
struct ResolveTelemetry {
    std::uint64_t candidates{},pending{},unclaimed_native{},claimed{},reconstructed{},unfiltered{},recovered{},rejected{},unrecoverable{};
    std::uint64_t shader_checks{},shader_rejected{},sdk_commands{};
    void begin() noexcept {++candidates;++pending;}
    void end(const DispatchResult &r) noexcept {
        --pending;if(!r.claimed){++unclaimed_native;return;}++claimed;
        if(r.resolve.sdk.commands)++sdk_commands;
        switch(r.resolve.outcome){
        case ResolveOutcome::reconstructed:++reconstructed;break;
        case ResolveOutcome::unfiltered:++unfiltered;break;
        case ResolveOutcome::recovered:++recovered;break;
        case ResolveOutcome::rejected:++rejected;break;
        case ResolveOutcome::unrecoverable:++unrecoverable;break;
        }
    }
    void write(std::ostream &o) const {
        o<<"{\"schema\":1,\"policy\":\"qualified-resolve-no-native-retry-v1\",\"exclusive_mode\":false"
         <<",\"candidates\":"<<candidates<<",\"pending\":"<<pending<<",\"unclaimed_native\":"<<unclaimed_native
         <<",\"claimed\":"<<claimed<<",\"reconstructed\":"<<reconstructed<<",\"unfiltered\":"<<unfiltered
         <<",\"recovered\":"<<recovered<<",\"rejected\":"<<rejected<<",\"unrecoverable\":"<<unrecoverable
         <<",\"sdk_commands\":"<<sdk_commands<<",\"shader_checks\":"<<shader_checks<<",\"shader_rejected\":"<<shader_rejected<<'}';
    }
};
}
