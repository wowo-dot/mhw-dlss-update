#include "resolve.hpp"
#include "../temporal_present/unfiltered.hpp"

namespace mhw::continuous_dlaa {
ResolveResult record_resolve(dlss_sdk::InlineFrame &pass,ID3D12GraphicsCommandList *list,
    ID3D12Resource *raw_color,ID3D12Resource *output,unsigned width,unsigned height,
    const temporal_present::Bindings &bindings,dlss_sdk::HistoryFrame frame,bool reset,float frame_ms,
    bool evaluate,bool capture,dlss_sdk::RecordFault fault) noexcept {
    namespace tp=temporal_present;
    if(!bindings.ready()||!tp::can_copy_unfiltered(list,raw_color,output,width,height,D3D12_RESOURCE_STATE_COPY_DEST))return {};
    ResolveResult result;
    if(evaluate){
        const bool success=pass.record_continuous(list,output,bindings,frame,reset,frame_ms,capture,fault);
        result.sdk=pass.last_attempt();
        if(success&&result.sdk.success&&result.sdk.native_output_written&&result.sdk.bindings_restored){
            result.outcome=ResolveOutcome::reconstructed;return result;
        }
        // Never overwrite native output or resume its consumer with unknown
        // bindings after partial work. The caller must escalate this outcome.
        if(result.sdk.commands&&!result.sdk.bindings_restored){result.outcome=ResolveOutcome::unrecoverable;return result;}
    }
    const bool copied=tp::copy_unfiltered(list,raw_color,output,width,height,D3D12_RESOURCE_STATE_COPY_DEST);
    result.outcome=copied?(evaluate?ResolveOutcome::recovered:ResolveOutcome::unfiltered):ResolveOutcome::unrecoverable;
    return result;
}
}
