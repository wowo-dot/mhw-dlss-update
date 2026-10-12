#pragma once
#include <filesystem>
#include "config.hpp"
#include "../../third_party/al/al_plugin.h"

// The mod's pages in CMFM's Mods menu (alpha), when CMFM 1.2 or later is
// installed: "DLSS" with a "Settings" page and a "Status" page.
// - CMFM is optional: without it, nothing changes and the ini stays the only
//   way to change the settings.
// - The ini stays the source of truth. The menu reads it when a Mods page opens
//   and writes it when the page closes after a change: only the value changes,
//   every comment and other key stays.
namespace mhw::alpha::menu {

// al_plugin_init, after the worker has read the ini. `fg_loaded`: DLSS-G was
// loaded at start, so a new multiplier applies at once. No throw.
void add_pages(const al_api_t *api, al_plugin_t *self, const Config &config, const std::filesystem::path &folder,
               bool fg_loaded) noexcept;

}  // namespace mhw::alpha::menu
