#pragma once
#include <windows.h>
#include <sl.h>
#include <sl_reflex.h>
#include <sl_pcl.h>
#include <sl_dlss_g.h>
#include <filesystem>

namespace mhw::reflex {
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
    sl::Result attach(void* native_device);
    sl::Result supported(sl::Feature feature, const sl::AdapterInfo& adapter) const;
    sl::Result requirements(sl::Feature feature, sl::FeatureRequirements& value) const;
    // Off is also used to suspend interpolation while retaining SDK routing.
    sl::Result frame_generation_off(const sl::ViewportHandle& viewport) const;
    // Ordinary FG only: one generated frame. Caller must qualify support,
    // tag the matching frame's inputs and retain them until SDK consumption.
    sl::Result frame_generation_ordinary(const sl::ViewportHandle& viewport) const;
    // Fixed-count MFG only, bounded by this viewport's reported capability.
    // State and option calls must run on the serialized presentation owner.
    sl::Result frame_generation_multi(const sl::ViewportHandle& viewport, unsigned generated_frames) const;
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
    PFun_slInit* init_{};
    PFun_slShutdown* shutdown_{};
    PFun_slSetD3DDevice* attach_{};
    PFun_slIsFeatureSupported* supported_{};
    PFun_slGetFeatureRequirements* requirements_{};
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
