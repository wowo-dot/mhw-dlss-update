#include "streamline.hpp"
#include "frame_generation_policy.hpp"
#include <bcrypt.h>
#include <wintrust.h>
#include <softpub.h>
#include <array>
#include <fstream>
#include <stdexcept>
#include <string_view>

namespace mhw::reflex {
namespace {
struct Hash {
    BCRYPT_ALG_HANDLE algorithm{};
    BCRYPT_HASH_HANDLE hash{};
    Hash() {
        if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0 ||
            BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) != 0) {
            if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
            throw std::runtime_error("Streamline SHA256 provider");
        }
    }
    ~Hash() { if (hash) BCryptDestroyHash(hash); if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0); }
};
std::string digest(const std::filesystem::path& path) {
    Hash h;
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Streamline dependency missing");
    std::array<char, 65536> buffer{};
    while (file) {
        file.read(buffer.data(), buffer.size());
        const auto n = file.gcount();
        if (n > 0 && BCryptHashData(h.hash, reinterpret_cast<PUCHAR>(buffer.data()), static_cast<ULONG>(n), 0) != 0)
            throw std::runtime_error("Streamline dependency read");
    }
    std::array<UCHAR, 32> bytes{};
    if (!file.eof() || BCryptFinishHash(h.hash, bytes.data(), static_cast<ULONG>(bytes.size()), 0) != 0)
        throw std::runtime_error("Streamline dependency hash");
    constexpr char hex[] = "0123456789abcdef";
    std::string value;
    for (const auto b : bytes) { value += hex[b >> 4]; value += hex[b & 15]; }
    return value;
}
void verify(const std::filesystem::path& path, std::string_view expected) {
    if (digest(path) != expected) throw std::runtime_error("Streamline pinned dependency mismatch");
    WINTRUST_FILE_INFO file{};
    file.cbStruct = sizeof(file); file.pcwszFilePath = path.c_str();
    WINTRUST_DATA data{};
    data.cbStruct = sizeof(data); data.dwUIChoice = WTD_UI_NONE;
    data.fdwRevocationChecks = WTD_REVOKE_WHOLECHAIN;
    data.dwUnionChoice = WTD_CHOICE_FILE; data.pFile = &file;
    data.dwStateAction = WTD_STATEACTION_VERIFY;
    data.dwProvFlags = WTD_REVOCATION_CHECK_CHAIN_EXCLUDE_ROOT;
    GUID action = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    const auto result = WinVerifyTrust(nullptr, &action, &data);
    data.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust(nullptr, &action, &data);
    if (result != ERROR_SUCCESS) throw std::runtime_error("Streamline signature verification failed");
}
template<class T> void core(HMODULE module, const char* name, T*& fn) {
    // GetProcAddress returns a function pointer; MSVC diagnoses the ABI cast.
#pragma warning(push)
#pragma warning(disable:4191)
    fn = reinterpret_cast<T*>(GetProcAddress(module, name));
#pragma warning(pop)
    if (!fn) throw std::runtime_error(name);
}
template<class T> void feature(PFun_slGetFeatureFunction* get, sl::Feature id, const char* name, T*& fn) {
    void* address{};
    if (get(id, name, address) != sl::Result::eOk || !address) throw std::runtime_error(name);
    fn = reinterpret_cast<T*>(address);
}
}
Streamline::~Streamline() { shutdown(); }
void Streamline::initialize(const std::filesystem::path& runtime, const std::filesystem::path& logs,
                           StreamlineFeatures selection) {
    if (module_ || !runtime.is_absolute() || !logs.is_absolute() ||
        (selection != StreamlineFeatures::reflex_only && selection != StreamlineFeatures::reflex_and_frame_generation))
        throw std::runtime_error("Streamline initialization arguments");
    frame_generation_ = selection == StreamlineFeatures::reflex_and_frame_generation;
    const auto root = std::filesystem::canonical(runtime);
    // Official 2.12.0 production binaries only. Verify every requested plugin
    // before executing any SDK code. No OTA or downloaded plugin loading.
    verify(root/L"sl.interposer.dll", "2a79db6857ae8c75bbd871a9489c48bc6a39f7fcc88b9b02afd53d0376cbec66");
    verify(root/L"sl.common.dll", "c57930ef5a8a3fe9be85efdf71a61d8107c1148e8a6aed456464547128f7f4ae");
    verify(root/L"sl.pcl.dll", "699ab461e64e95189a7fe6a21c79ad237cf56b60ea748cb6c840cd5431ba91d1");
    verify(root/L"sl.reflex.dll", "7e6e4ccc4b561bd449fb0da90709d9b96b08c3f6f4697362caaa359e72a58a67");
    if (frame_generation_) {
        verify(root/L"sl.dlss_g.dll", "1fec3f8fdfc59d78c4445c276c1a0fb798bf251985f348597dc2b44d0c995e52");
        verify(root/L"nvngx_dlssg.dll", "135eaf0733c1e37381a8c28abcf7a862404a54132b81787c04e35d09efc5e36f");
        for (const auto name : {L"sl.dlss_g.dll", L"nvngx_dlssg.dll"})
            if (GetModuleHandleW(name)) throw std::runtime_error("Frame generation already loaded by another integration");
    }
    for (const auto name : {L"sl.interposer.dll", L"sl.common.dll", L"sl.pcl.dll", L"sl.reflex.dll"})
        if (GetModuleHandleW(name)) throw std::runtime_error("Streamline already loaded by another integration");
    std::filesystem::create_directories(logs);
    module_ = LoadLibraryExW((root/L"sl.interposer.dll").c_str(), nullptr,
                            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!module_) throw std::runtime_error("Streamline secure load failed");
    core(module_, "slInit", init_); core(module_, "slShutdown", shutdown_);
    core(module_, "slSetD3DDevice", attach_); core(module_, "slIsFeatureSupported", supported_);
    core(module_, "slGetFeatureRequirements", requirements_);
    core(module_, "slIsFeatureLoaded", loaded_); core(module_, "slSetFeatureLoaded", set_loaded_);
    core(module_, "slFreeResources", free_);
    core(module_, "slUpgradeInterface", upgrade_); core(module_, "slGetNewFrameToken", frame_);
    core(module_, "slGetNativeInterface", native_);
    core(module_, "slGetFeatureFunction", feature_);
    if (frame_generation_) {
        core(module_, "slSetConstants", constants_);
        core(module_, "slSetTagForFrame", tags_);
    }
    const auto plugin = root.wstring(); const wchar_t* paths[]{plugin.c_str()};
    const sl::Feature features[]{sl::kFeatureReflex, sl::kFeaturePCL, sl::kFeatureDLSS_G};
    sl::Preferences pref{};
    if (frame_generation_ && sdk_verbose_log) pref.logLevel = sl::LogLevel::eVerbose;
    pref.pathsToPlugins = paths; pref.numPathsToPlugins = 1;
    pref.pathToLogsAndData = logs.c_str(); pref.showConsole = false;
    pref.flags = sl::PreferenceFlags::eUseManualHooking | sl::PreferenceFlags::eDisableCLStateTracking |
                 sl::PreferenceFlags::eUseFrameBasedResourceTagging;
    pref.featuresToLoad = features; pref.numFeaturesToLoad = frame_generation_ ? 3U : 2U;
    pref.engine = sl::EngineType::eCustom; pref.engineVersion = "0.1.0";
    // Our existing public NGX project identity, not a borrowed application ID.
    // sl.common uses Init_with_ProjectID when both project/engine are provided.
    if (frame_generation_) pref.projectId = "956cdd19-7d3c-4660-b664-91c20d2bc9bd";
    pref.renderAPI = sl::RenderAPI::eD3D12;
    init_attempted_ = true;
    const auto result = init_(pref, sl::kSDKVersion);
    if (result != sl::Result::eOk)
        throw std::runtime_error("slInit result " + std::to_string(static_cast<unsigned>(result)));
    initialized_ = true;
    // Feature exports become available after the device is attached, not here.
}
sl::Result Streamline::attach(void* native_device) {
    if (!initialized_ || !attach_) return sl::Result::eErrorNotInitialized;
    const auto result = attach_(native_device);
    if (result == sl::Result::eOk) {
        feature(feature_, sl::kFeatureReflex, "slReflexSleep", sleep_);
        feature(feature_, sl::kFeaturePCL, "slPCLSetMarker", marker_);
        feature(feature_, sl::kFeatureReflex, "slReflexSetOptions", options_);
        feature(feature_, sl::kFeatureReflex, "slReflexGetState", state_);
        // A plugin unloaded before attach, or one that failed to start on
        // this device (programming guide 2.2.1), is not used.
        bool loaded{};
        if (frame_generation_ && (loaded_(sl::kFeatureDLSS_G, loaded) != sl::Result::eOk || !loaded)) frame_generation_ = false;
        if (frame_generation_) {
            feature(feature_, sl::kFeatureDLSS_G, "slDLSSGSetOptions", frame_generation_options_);
            feature(feature_, sl::kFeatureDLSS_G, "slDLSSGGetState", frame_generation_state_);
        }
    }
    return result;
}
sl::Result Streamline::unload_frame_generation() {
    if (!initialized_ || !set_loaded_ || !frame_generation_) return sl::Result::eErrorNotInitialized;
    const auto result = set_loaded_(sl::kFeatureDLSS_G, false);
    if (result == sl::Result::eOk) frame_generation_ = false;
    return result;
}
sl::Result Streamline::frame_generation_free(const sl::ViewportHandle& viewport) const {
    return frame_generation_ && free_ ? free_(sl::kFeatureDLSS_G, viewport) : sl::Result::eErrorNotInitialized;
}
sl::Result Streamline::supported(sl::Feature id, const sl::AdapterInfo& adapter) const {
    return initialized_ && supported_ ? supported_(id, adapter) : sl::Result::eErrorNotInitialized;
}
sl::Result Streamline::requirements(sl::Feature id, sl::FeatureRequirements& value) const {
    return initialized_ && requirements_ ? requirements_(id, value) : sl::Result::eErrorNotInitialized;
}
sl::Result Streamline::frame_generation_off(const sl::ViewportHandle& viewport, bool retain) const {
    if (!frame_generation_options_) return sl::Result::eErrorNotInitialized;
    sl::DLSSGOptions value{}; value.mode = sl::DLSSGMode::eOff; value.numFramesToGenerate = 1;
    if (retain) value.flags = sl::DLSSGFlags::eRetainResourcesWhenOff;
    return frame_generation_options_(viewport, value);
}
sl::Result Streamline::frame_generation_state(const sl::ViewportHandle& viewport, sl::DLSSGState& value) const {
    return frame_generation_state_ ? frame_generation_state_(viewport, value, nullptr) : sl::Result::eErrorNotInitialized;
}
sl::Result Streamline::frame_generation_ordinary(const sl::ViewportHandle& viewport, bool retain) const {
    return frame_generation_generate(viewport, 1, retain);
}
sl::Result Streamline::frame_generation_generate(const sl::ViewportHandle& viewport, unsigned generated_frames,
                                                 bool retain) const {
    return frame_generation_mode(viewport, sl::DLSSGMode::eOn, generated_frames, 0.0f, retain);
}
sl::Result Streamline::frame_generation_mode(const sl::ViewportHandle& viewport, sl::DLSSGMode mode, unsigned generated_frames,
                                             float target_fps, bool retain) const {
    if (!frame_generation_options_) return sl::Result::eErrorNotInitialized;
    const bool known = mode == sl::DLSSGMode::eOn || mode == sl::DLSSGMode::eAuto || mode == sl::DLSSGMode::eDynamic;
    if (!known || generated_frames == 0 || !(target_fps >= 0.0f)) return sl::Result::eErrorInvalidParameter;
    sl::DLSSGOptions value{}; value.mode = mode; value.numFramesToGenerate = generated_frames;
    if (mode == sl::DLSSGMode::eDynamic) value.dynamicTargetFrameRate = target_fps;
    if (retain) value.flags = sl::DLSSGFlags::eRetainResourcesWhenOff;
    return frame_generation_options_(viewport, value);
}
sl::Result Streamline::frame_generation_multi(const sl::ViewportHandle& viewport, unsigned generated_frames) const {
    if (!frame_generation_options_) return sl::Result::eErrorNotInitialized;
    if (generated_frames < 2) return sl::Result::eErrorInvalidParameter;
    sl::DLSSGState capability{};
    const auto result = frame_generation_state(viewport, capability);
    if (result != sl::Result::eOk) return result;
    if (!generated_frames_supported(generated_frames, capability.numFramesToGenerateMax))
        return sl::Result::eErrorInvalidParameter;
    sl::DLSSGOptions value{}; value.mode = sl::DLSSGMode::eOn; value.numFramesToGenerate = generated_frames;
    return frame_generation_options_(viewport, value);
}
sl::Result Streamline::constants(const sl::Constants& values, const sl::FrameToken& frame,
                                const sl::ViewportHandle& viewport) const {
    return initialized_ && constants_ ? constants_(values, frame, viewport) : sl::Result::eErrorNotInitialized;
}
sl::Result Streamline::tags(const sl::FrameToken& frame, const sl::ViewportHandle& viewport,
                          const sl::ResourceTag* values, unsigned count) const {
    // This wrapper admits immutable Present-time tags only. Copy-at-tag-time
    // lifetimes need a command-list contract and are not silently supported.
    if (!initialized_ || !tags_) return sl::Result::eErrorNotInitialized;
    if (!values || !count || count > 16) return sl::Result::eErrorInvalidParameter;
    for (unsigned i = 0; i < count; ++i)
        if (values[i].lifecycle != sl::ResourceLifecycle::eValidUntilPresent)
            return sl::Result::eErrorInvalidParameter;
    return tags_(frame, viewport, values, count, nullptr);
}
sl::Result Streamline::upgrade(void** owned) const { return initialized_ && upgrade_ ? upgrade_(owned) : sl::Result::eErrorNotInitialized; }
sl::Result Streamline::native(void* proxy, void** owned) const {
    return initialized_ && native_ ? native_(proxy, owned) : sl::Result::eErrorNotInitialized;
}
sl::Result Streamline::frame(sl::FrameToken*& token, const std::uint32_t* index) const {
    return initialized_ && frame_ ? frame_(token, index) : sl::Result::eErrorNotInitialized;
}
sl::Result Streamline::sleep(const sl::FrameToken& token) const {
    return sleep_ ? sleep_(token) : sl::Result::eErrorNotInitialized;
}
sl::Result Streamline::marker(sl::PCLMarker value, const sl::FrameToken& token) const {
    return marker_ ? marker_(value, token) : sl::Result::eErrorNotInitialized;
}
sl::Result Streamline::options(sl::ReflexMode mode) const {
    if (!options_) return sl::Result::eErrorNotInitialized;
    sl::ReflexOptions options{}; options.mode = mode; return options_(options);
}
sl::Result Streamline::state(sl::ReflexState& value) const {
    return state_ ? state_(value) : sl::Result::eErrorNotInitialized;
}
sl::Result Streamline::shutdown() noexcept {
    if (!module_) return sl::Result::eOk;
    const auto result = init_attempted_ && shutdown_ ? shutdown_() : sl::Result::eOk;
    // The manual proxies still have interposer vtables after slShutdown.
    // The SDK requires shutdown BEFORE their final releases, so retain this
    // module until process exit. Unloading it here invalidates those vtables.
    // No reinitialization in this wrapper; game integration is process-lived.
    if (result == sl::Result::eOk) {
        if (!init_attempted_ && !initialized_) return result;
        init_attempted_ = false; initialized_ = false;
        init_ = nullptr; shutdown_ = nullptr; attach_ = nullptr; supported_ = nullptr;
        upgrade_ = nullptr; native_ = nullptr; frame_ = nullptr; feature_ = nullptr;
        constants_ = nullptr; tags_ = nullptr;
        sleep_ = nullptr; marker_ = nullptr; options_ = nullptr; state_ = nullptr;
        requirements_ = nullptr; frame_generation_options_ = nullptr; frame_generation_state_ = nullptr;
        loaded_ = nullptr; set_loaded_ = nullptr; free_ = nullptr;
        frame_generation_ = false;
    }
    return result;
}
}
