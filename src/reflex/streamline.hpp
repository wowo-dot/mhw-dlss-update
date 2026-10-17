#pragma once
#include <windows.h>
#include <sl.h>
#include <sl_reflex.h>
#include <sl_pcl.h>
#include <sl_dlss_g.h>
#include <filesystem>

namespace mhw::reflex {
// Research builds log Streamline verbosely with the DLSS-G plugin; the alpha
// sets this to false before initialize() (default level: errors, warnings, info).
inline bool sdk_verbose_log = true;
enum class StreamlineFeatures { reflex_only, reflex_and_frame_generation };
// The caller serializes initialization, options, state reads and shutdown.
// Shutdown must precede destruction of the borrowed DX12 device/swap chain.
// FG is opt-in; the default retains the accepted Reflex-only SDK contract.
// No game hooks or manual feature evaluation. FG inputs use explicit tokens.
class Streamline final {
public:
    Streamline() = default;
    ~Streamline();
    Streamline(const Streamline&) = delete;
    Streamline& operator=(const Streamline&) = delete;
    void initialize(const std::filesystem::path& runtime,
                    const std::filesystem::path& logs,
                    StreamlineFeatures features = StreamlineFeatures::reflex_only);
    // With frame generation requested: DLSS-G's functions are resolved only
    // if its plugin is still loaded afterwards (frame_generation()).
    sl::Result attach(void* native_device);
    // Before attach: unloads the requested DLSS-G plugin (a card or system
    // without frame generation); Streamline then leaves the swap chain alone.
    sl::Result unload_frame_generation();
    // v0.27.4: loads or unloads the requested DLSS-G plugin at any time
    // (slSetFeatureLoaded); after attach a load resolves its functions again.
    // Only with no swap chain alive (DLSS-G guide 18.0: the chain is made
    // again after every switch). The caller serializes it with every other
    // DLSS-G call.
    sl::Result frame_generation_load(bool loaded);
    // DLSS-G was requested at slInit (it can be loaded later).
    bool frame_generation_requested() const noexcept { return frame_generation_requested_; }
    // DLSS-G is loaded now (Streamline owns the swap chain's pacing).
    bool frame_generation() const noexcept { return frame_generation_; }
    // Frees DLSS-G's resources after a long-term Off (the player turned frame
    // generation off; DLSS-G guide 6.4). The present thread, with it Off.
    sl::Result frame_generation_free(const sl::ViewportHandle& viewport) const;
    sl::Result supported(sl::Feature feature, const sl::AdapterInfo& adapter) const;
    sl::Result requirements(sl::Feature feature, sl::FeatureRequirements& value) const;
    // Off is also used to suspend interpolation while retaining SDK routing.
    // retain keeps FG resources allocated across Off (no re-enable stutter).
    sl::Result frame_generation_off(const sl::ViewportHandle& viewport, bool retain = false) const;
    // FG in `mode`. The caller qualifies support, tags the matching frame's
    // inputs and retains them until SDK consumption. eOn shows `generated_frames`; eAuto
    // too, but DLSS-G pauses it when the game would run faster without; eDynamic
    // picks the count itself toward `target_fps` (0: the display's refresh rate)
    // and needs DLSSGState::bIsDynamicMFGSupported (the caller checks it).
    sl::Result frame_generation_mode(const sl::ViewportHandle& viewport, sl::DLSSGMode mode, unsigned generated_frames,
                                     float target_fps, bool retain = false) const;
    // State and option calls must run on the serialized presentation owner.
    sl::Result frame_generation_state(const sl::ViewportHandle& viewport, sl::DLSSGState& value) const;
    sl::Result constants(const sl::Constants&, const sl::FrameToken&, const sl::ViewportHandle&) const;
    sl::Result tags(const sl::FrameToken&, const sl::ViewportHandle&, const sl::ResourceTag*, unsigned count) const;
    sl::Result upgrade(void** owned_interface) const;
    // On success this returns a new owned native COM reference (AddRef).
    sl::Result native(void* proxy_interface, void** native_interface) const;
    sl::Result frame(sl::FrameToken*& token, const std::uint32_t* index) const;
    sl::Result sleep(const sl::FrameToken& token) const;
    sl::Result marker(sl::PCLMarker marker, const sl::FrameToken& token) const;
    sl::Result options(sl::ReflexMode mode) const;
    sl::Result state(sl::ReflexState& state) const;
    sl::Result shutdown() noexcept;
    bool initialized() const noexcept { return initialized_; }
private:
    HMODULE module_{};
    bool init_attempted_{};
    bool initialized_{};
    bool frame_generation_{};
    bool frame_generation_requested_{};
    bool attached_{};
    PFun_slInit* init_{};
    PFun_slShutdown* shutdown_{};
    PFun_slSetD3DDevice* attach_{};
    PFun_slIsFeatureSupported* supported_{};
    PFun_slGetFeatureRequirements* requirements_{};
    PFun_slIsFeatureLoaded* loaded_{};
    PFun_slSetFeatureLoaded* set_loaded_{};
    PFun_slFreeResources* free_{};
    PFun_slUpgradeInterface* upgrade_{};
    PFun_slGetNativeInterface* native_{};
    PFun_slGetFeatureFunction* feature_{};
    PFun_slGetNewFrameToken* frame_{};
    PFun_slSetConstants* constants_{};
    PFun_slSetTagForFrame* tags_{};
    PFun_slReflexSleep* sleep_{};
    PFun_slPCLSetMarker* marker_{};
    PFun_slReflexSetOptions* options_{};
    PFun_slReflexGetState* state_{};
    PFun_slDLSSGSetOptions* frame_generation_options_{};
    PFun_slDLSSGGetState* frame_generation_state_{};
};
}
