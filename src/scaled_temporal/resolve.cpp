#include "resolve.hpp"

namespace mhw::scaled_temporal {
continuous_dlaa::ResolveResult record_resolve(dlss_sdk::InlineFrame &pass, temporal_present::Output &recovery,
                                              ID3D12GraphicsCommandList *list, ID3D12Resource *output,
                                              const temporal_present::Bindings &bindings, dlss_sdk::HistoryFrame frame,
                                              bool reset, float frame_ms, bool evaluate) noexcept {
    using continuous_dlaa::ResolveOutcome;
    continuous_dlaa::ResolveResult result;
    const auto g = pass.geometry();
    if (!bindings.ready() || !dlss_sdk::valid_geometry(g) ||
        !recovery.spatial_contract({g.allocation.width, g.allocation.height, g.render.width, g.render.height},
                                   g.output.width, g.output.height))
        return result;
    // Allocate the binding restorer and reserve all bounded references before
    // SDK commands. Rejection cannot leave a half-recorded native output.
    std::unique_ptr<temporal_present::RestoreBindings> restore;
    try {
        restore = std::make_unique<temporal_present::RestoreBindings>(list, bindings);
        recovery.bind(pass.raw_color(), output);
        recovery.prepare_record(list);
    } catch (...) {
        return result;
    }
    if (evaluate) {
        const bool success = pass.record(list, output, bindings, frame, reset, frame_ms);
        result.sdk = pass.last_attempt();
        if (success && result.sdk.success && result.sdk.native_output_written && result.sdk.bindings_restored) {
            result.outcome = ResolveOutcome::reconstructed;
            return result;
        }
        if (result.sdk.commands && !result.sdk.bindings_restored) {
            result.outcome = ResolveOutcome::unrecoverable;
            return result;
        }
    }
    try {
        recovery.record(list, D3D12_RESOURCE_STATE_COPY_DEST);
        restore->apply();
        result.outcome = evaluate ? ResolveOutcome::recovered : ResolveOutcome::unfiltered;
    } catch (...) {
        // Output performs every throwing check before its first command. Its
        // handoff and state restoration have no throwing operations afterward.
        result.outcome = ResolveOutcome::unrecoverable;
    }
    return result;
}
} // namespace mhw::scaled_temporal
