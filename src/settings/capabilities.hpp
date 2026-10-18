#pragma once
#include <cstdint>
#include <string>

// What this graphics card, driver and system offer (v0.27.5): filled at
// start from the SDKs' own answers (DXGI's adapter description, NGX's DLSS
// capability, Streamline's support results and requirements, DLSS-G's state)
// and completed when DLSS-G reports more later. The menu offers only what is
// available here, with the reason for the rest. Pure (tests/settings_menu_tests.cpp);
// the worker fills it and status.cpp keeps it.
namespace mhw::settings::capabilities {

enum class Support : unsigned { unknown, yes, no };
// Why frame generation is not available.
enum class Missing : unsigned {
    none,
    card,       // not an RTX 40 or 50 (or not NVIDIA)
    driver,     // the NVIDIA driver is older than DLSS-G requires
    os,         // Windows is older than DLSS-G requires
    scheduling, // Hardware-accelerated GPU scheduling is off
    plugin_off, // frame_generation_plugin = off in mhw_dlss.ini
    other,      // anything else (the log has the SDK's result)
};

struct Card {
    std::string name; // UTF-8, as DXGI describes the adapter
    unsigned vendor{}, device{};
    std::uint64_t memory_mb{};               // dedicated video memory
    unsigned driver_major{}, driver_minor{}; // as Streamline detects it (NVIDIA 610.88: 610, 88)
};

struct Capabilities {
    bool known{}; // filled at start
    Card card;
    Support dlss{}, reflex{}, frame_generation{}, dynamic{};
    Missing missing{};
    unsigned frame_generation_result{};                        // Streamline's result code, for the log
    unsigned driver_required_major{}, driver_required_minor{}; // DLSS-G's minimum
    unsigned max_multiplier{}; // total frames per rendered frame: 2 ordinary, up to 6; 0 not known
};

inline constexpr unsigned nvidia = 0x10de;

inline std::string version_text(unsigned major, unsigned minor) {
    return std::to_string(major) + "." + (minor < 10 ? "0" : "") + std::to_string(minor);
}

// The reason frame generation cannot be chosen, for the menu's help line.
inline std::string frame_generation_reason(const Capabilities &c) {
    switch (c.missing) {
    case Missing::none: break;
    case Missing::card: return "DLSS Frame Generation needs an NVIDIA GeForce RTX 40 or 50 series graphics card.";
    case Missing::driver:
        return c.driver_required_major
                   ? "DLSS Frame Generation needs NVIDIA driver " +
                         version_text(c.driver_required_major, c.driver_required_minor) + " or newer" +
                         (c.card.driver_major
                              ? " (installed: " + version_text(c.card.driver_major, c.card.driver_minor) + ")."
                              : ".")
                   : std::string("DLSS Frame Generation needs a newer NVIDIA driver.");
    case Missing::os: return "DLSS Frame Generation needs a newer version of Windows.";
    case Missing::scheduling:
        return "DLSS Frame Generation needs Hardware-accelerated GPU scheduling (Windows Settings > System > Display > Graphics).";
    case Missing::plugin_off:
        return "frame_generation_plugin = off in mhw_dlss.ini keeps DLSS Frame Generation unloaded.";
    case Missing::other: break;
    }
    return "DLSS Frame Generation is not available here (see the mod's log).";
}

// Menu choices that depend on the card.
enum class Option : unsigned {
    dlss,             // any DLSS mode
    frame_generation, // frame generation on
    multiplier,       // a fixed multiplier (`value`: total, 2 to 6)
    dynamic,          // the Dynamic multiplier
    reflex,           // Reflex On or Boost
};
// Whether `option` can be chosen; if not, the reason. Unknown support allows
// it (the mod refuses at runtime and the menu learns more later).
inline bool available(const Capabilities &c, Option option, unsigned value, std::string &why) {
    switch (option) {
    case Option::dlss:
        if (c.dlss != Support::no) return true;
        why = c.card.vendor && c.card.vendor != nvidia ? "DLSS needs an NVIDIA GeForce RTX graphics card."
                                                       : "DLSS is not available on this graphics card or driver.";
        return false;
    case Option::reflex:
        if (c.reflex != Support::no) return true;
        why = "NVIDIA Reflex is not available on this graphics card or driver.";
        return false;
    case Option::frame_generation:
    case Option::multiplier:
    case Option::dynamic:
        if (c.frame_generation == Support::no) {
            why = frame_generation_reason(c);
            return false;
        }
        if (option == Option::multiplier && c.max_multiplier && value > c.max_multiplier) {
            why = c.max_multiplier == 2 ? "This graphics card shows 2x at most; 3x and up need an RTX 50."
                                        : "This graphics card shows " + std::to_string(c.max_multiplier) + "x at most.";
            return false;
        }
        if (option == Option::dynamic && c.dynamic == Support::no) {
            why = "Dynamic needs an RTX 50 graphics card and NVIDIA driver 595.41 or newer.";
            return false;
        }
        return true;
    }
    return true;
}

// The Status page's System lines (v0.28.0: one fact each, short enough for
// the value box).
inline std::string card_name_line(const Capabilities &c) {
    return c.known && !c.card.name.empty() ? c.card.name : "Not known yet";
}
// Whole gigabytes, rounded up: DXGI reports a little less than the size on
// the box (32,187 MB on a 32 GB RTX 5090).
inline std::string memory_line(const Capabilities &c) {
    return c.known && c.card.memory_mb ? std::to_string((c.card.memory_mb + 1023) / 1024) + " GB" : "Not known yet";
}
inline std::string driver_line(const Capabilities &c) {
    return c.known && c.card.driver_major ? version_text(c.card.driver_major, c.card.driver_minor) : "Not known yet";
}
inline std::string generation_support_line(const Capabilities &c) {
    if (!c.known) return "Not known yet";
    if (c.frame_generation == Support::no) switch (c.missing) {
        case Missing::plugin_off: return "Not loaded (mhw_dlss.ini)";
        case Missing::card: return "No: needs an RTX 40 or 50";
        case Missing::driver:
            return c.driver_required_major
                       ? "No: needs driver " + version_text(c.driver_required_major, c.driver_required_minor)
                       : std::string("No: needs a newer driver");
        case Missing::os: return "No: needs a newer Windows";
        case Missing::scheduling: return "No: GPU scheduling is off";
        case Missing::none:
        case Missing::other: return "No (see the log)";
        }
    auto line = c.max_multiplier ? "Up to " + std::to_string(c.max_multiplier) + "x" : std::string("Yes");
    if (c.dynamic == Support::yes) line += ", Dynamic";
    return line;
}

} // namespace mhw::settings::capabilities
