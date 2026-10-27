#pragma once
// 0.32.0: which DLSS files the mod runs. NGX loads whatever it chooses: this
// mod's shipped nvngx_dlss.dll, NVIDIA's own copy from its store under the
// NVIDIA App's DLSS override (ProgramData\NVIDIA\NGX\models), or a file a
// player swapped in. A game with its own DLSS runs any of them, as NGX checks
// NVIDIA's signature itself. The mod does the same: it accepts a file that is
// NVIDIA's DLSS (its version resource's InternalName: "DLSS" for Super
// Resolution, "DLSS-G" for frame generation) at the API version this mod
// speaks (310, the SDK's presets), when it is the shipped file (its pinned
// hash, which also holds where Windows' signature check is missing, as under
// Wine) or carries NVIDIA Corporation's Authenticode signature. Pure, for
// tests/ngx_providers_tests.cpp; dlss_file_identity.hpp reads the files.
#include <string>
#include <string_view>

namespace mhw {
struct FileVersion {
    unsigned major{}, minor{}, build{}, revision{};
    constexpr auto operator<=>(const FileVersion &) const = default;
};
inline constexpr FileVersion minimum_dlss{310, 0, 0, 0};
inline constexpr std::string_view dlss_super_resolution = "DLSS", dlss_frame_generation = "DLSS-G";

struct DlssFile {
    std::string internal; // the version resource's InternalName
    FileVersion version;
    bool shipped{};       // this mod's file, by its pinned hash
    bool nvidia_signed{}; // a valid Authenticode signature by NVIDIA Corporation
    std::string signer;   // the signer's name, for the log
};
constexpr bool dlss_accepted(const DlssFile &f, std::string_view feature) noexcept {
    return f.internal == feature && f.version >= minimum_dlss && (f.shipped || f.nvidia_signed);
}
inline std::string version_text(FileVersion v) {
    return std::to_string(v.major) + '.' + std::to_string(v.minor) + '.' + std::to_string(v.build) + '.' +
           std::to_string(v.revision);
}
} // namespace mhw
