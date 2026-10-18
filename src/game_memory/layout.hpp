#pragma once
// The game's memory the mod reads (and the few fields it writes), by
// structure: offsets in the tested executable (c2ebbbd2..., clean image
// a78ea69b...). A reader still verifies the game code that uses a field (its
// code gates) before trusting it, checks each read, and fails closed when one
// fails. Names follow the game's where Fexty's 15.23 database has them (the
// vtables), else what the research established (docs/research, cited here).
// Values are plain 32-bit offsets, so `base + field` reads as it always did.
#include <cstdint>

namespace mhw::layout {
using Offset = std::uint32_t;

// Globals and vtables: RVAs in the image.
namespace image {
inline constexpr Offset renderer = 0x51c4480;     // sMhRender*, the renderer (dlss-render-resolution-control.md)
inline constexpr Offset camera = 0x51c4400;       // the scene camera (projection-jitter-probe.md)
inline constexpr Offset scene = 0x51c4368;        // sScene*: the AA choice, the jitter table (full-size-post-chain.md)
inline constexpr Offset d3d_render = 0x51c7050;   // cD3DRender*, the D3D12 backend (ngx-startup-capabilities.md)
inline constexpr Offset engine = 0x51c4148;       // the engine object that links the renderer (live_frames)
inline constexpr Offset app_ext = 0x51c5320;      // sAppExt*: the window and render-scale basis (live-aspect.md)
inline constexpr Offset graphics_api = 0x51cc080; // byte: 1 = Direct3D 12 (projection-upload-binding.md)
inline constexpr Offset post_queue = 0x51cc238;   // the post-effect queue (temporal-activation-observation.md)
inline constexpr Offset unit_manager = 0x51238c8; // sMhUnit*: the unit lists (game-aa-option.md)
inline constexpr Offset game_dlss_state = 0x51c6c28;     // why the game has no DLSS 1: 1 NGX init, 2 SuperSampling,
                                                         // 3 ImageSignalProcessing unavailable (game-dlss1.md)
inline constexpr Offset ngx_slots = 0x51d8808;           // the game's NGX client slots (ngx-startup-capabilities.md)
inline constexpr Offset graphics_option_ini = 0x2f744b0; // {"app:\\graphics_option.ini", "GraphicsOption"}: the
                                                         // saved settings' file and section (game_options)

inline constexpr Offset camera_vtable = 0x350d188;       // the camera's constructor vtable (projection-jitter-probe.md)
inline constexpr Offset scene_camera_vtable = 0x33f82a8; // sCamera::vft (camera-type-probe.md)
inline constexpr Offset unit_manager_vtable = 0x33fcde8; // sMhUnit::vft
inline constexpr Offset taa2_vtable = 0x351bda8;         // uTemporalAAFilter2::vft, the game's TAA
inline constexpr Offset taa_vtable = 0x354dd78;          // uTemporalAAFilter::vft
inline constexpr Offset taa3_vtable = 0x354df48;         // uTemporalAAFilter3::vft
inline constexpr Offset fxaa_vtable = 0x351c1a8;         // uFXAAFilter::vft
inline constexpr Offset tone_map_vtable = 0x3538400;     // uToneMapFilter::vft
} // namespace image

// sMhRender, the renderer (image::renderer).
namespace renderer {
inline constexpr Offset backend = 0x78;            // cD3DRender*
inline constexpr Offset parallel_rendering = 0x93; // byte (mParallelRendering, dlaa-identity-path.md)
inline constexpr Offset use_dlss = 0xa5;           // byte: the game's DLSS use (jitter-sequence-probe.md)
inline constexpr Offset sizing_flags = 0xb4;       // bit 0: the game's DLSS sizing (scoped-camera-jitter.md)
inline constexpr Offset scene_rect = 0x188;        // int[4]: the scene rectangle in the window (live-aspect.md)
inline constexpr Offset scene_width = 0x198;       // the scene size (live-aspect.md)
inline constexpr Offset scene_height = 0x19c;
inline constexpr Offset scale_desired = 0x1f0; // float: the render scale asked for (the mod writes it;
inline constexpr Offset scale_applied = 0x1f4; // dlss-render-resolution-control.md: two coupled channels)
inline constexpr Offset scale_previous = 0x1f8;
inline constexpr Offset content_scale_desired = 0x200;
inline constexpr Offset content_scale = 0x204;
inline constexpr Offset combined_scale = 0x208; // the clamped product
inline constexpr Offset context = 0x710;        // the primary render context, embedded (depth-copy-frame-map.md)
inline constexpr Offset bank = 0x1f200;         // the command bank, toggled each frame (queued-work-provenance.md)
inline constexpr Offset epoch = 0x1f208;
inline constexpr Offset frame = 0x1f210;        // the counter temporal history and jitter use
inline constexpr Offset window_width = 0x1f448; // live-aspect.md
inline constexpr Offset window_height = 0x1f44c;
inline constexpr Offset primary_view = 0x1f450;        // the main view
inline constexpr Offset display_changes = 0x1f548;     // pending display changes: 0x40 a scale update, 0x4000 a new
                                                       // chain (fg-live-toggle.md)
inline constexpr Offset variable_resolution = 0x7b3b0; // byte (game-aa-option.md)
inline constexpr Offset aspect_applied = 0x7b43c;      // live-aspect.md
inline constexpr Offset aspect_requested = 0x7b440;
} // namespace renderer

// cD3DRender, the D3D12 backend (image::d3d_render, renderer::backend).
namespace d3d_render {
inline constexpr Offset device = 0x18;        // ID3D12Device*
inline constexpr Offset compute_root = 0x2a0; // the compute root signature recordings cache (dlaa-output-handoff.md)
inline constexpr Offset latency_waitable = 0x10f0; // the swap chain's frame latency waitable (fg-live-toggle.md)
inline constexpr Offset swap_chain = 0x1470;       // IDXGISwapChain*
inline constexpr Offset game_dlss = 0x42700;       // the game's own DLSS 1 NGX context (game-dlss1.md)
} // namespace d3d_render

// The render context (renderer::context): the frame's draw state.
namespace context {
inline constexpr Offset attachment = 0x50;     // the depth copy's source attachment (depth-copy-frame-map.md)
inline constexpr Offset viewport = 0x5c;       // int[4]: the rectangle setViewport stores (full-size-post-chain.md)
inline constexpr Offset unit_mask = 0x152;     // uint16: the units this context draws
inline constexpr Offset sort_stage = 0x154;    // the stage in each queued packet's sort key (dlaa-identity-path.md)
inline constexpr Offset technique = 0x158;     // the current shader technique
inline constexpr Offset display = 0x15a;       // uint16: the display this context draws for
inline constexpr Offset sort_order = 0x15c;    // the order in the sort key (queued-work-provenance.md)
inline constexpr Offset state = 0x1c8;         // the current render state
inline constexpr Offset view = 0x1d0;          // the view being drawn (native-aa-entry-timing.md)
inline constexpr Offset bindings = 0x710;      // owners by binding id: +8 * (id & 0xfff) (ordinary-taa-input-map.md)
inline constexpr Offset packets = 0x2b78;      // the published packets: 16-byte entries, the packet at +8
inline constexpr Offset packet_count = 0x2b80; // (depth-copy-frame-map.md)
inline constexpr Offset flags = 0x1ea74;       // the input readers require 0x54 clear
} // namespace context

// A view (renderer::primary_view, context::view).
namespace view {
inline constexpr Offset flags = 0x58;     // the readers require 0x840000 clear (the main view)
inline constexpr Offset gbuffers = 0x620; // the G-buffer container (full-size-post-chain.md)
inline constexpr Offset depth = 0x628;    // the scene depth container
inline constexpr Offset scale = 0x6b0;    // float (dlss-render-resolution-control.md)
inline constexpr Offset content_scale = 0x6b4;
inline constexpr Offset combined_scale = 0x6b8;
} // namespace view

// The G-buffer container (view::gbuffers): texture owners.
namespace gbuffers {
inline constexpr Offset gbuffer1 = 0x60;
inline constexpr Offset gbuffer4 = 0x78;     // R32_UINT: motion (full-size-post-chain.md)
inline constexpr Offset gbuffer5 = 0x80;     // R8G8_SNORM
inline constexpr Offset previous_ids = 0xc0; // tPrevGBufferID
} // namespace gbuffers

// A depth container (view::depth).
namespace depth_container {
inline constexpr Offset texture = 0xa0;
} // namespace depth_container

// The scene camera (image::camera).
namespace camera {
inline constexpr Offset jitter = 0x38;    // float x, y: the jitter the projection applies (the mod exchanges it)
inline constexpr Offset stamp = 0x40;     // the renderer epoch's low 32 bits (projection-jitter-probe.md)
inline constexpr Offset viewports = 0x50; // the viewports begin here
inline constexpr Offset draw_slot = 0x48; // the vtable slot of sCamera::draw (camera-type-probe.md)
} // namespace camera

// sScene (image::scene).
namespace scene {
inline constexpr Offset jitter_table = 0xe6a0; // 20 projection candidates, 8 bytes each
inline constexpr Offset taa = 0xe9e4;          // byte: the player's TAA (game-aa-option.md)
inline constexpr Offset fxaa = 0xe9e5;         // byte: the player's FXAA
} // namespace scene

// sAppExt (image::app_ext).
namespace app_ext {
inline constexpr Offset basis_width = 0x2cd0;         // the render-scale basis: the window's client size
inline constexpr Offset basis_height = 0x2cd4;        // when the flag below is set (live-aspect.md)
inline constexpr Offset window_from_display = 0x2cda; // byte: 1 on PC
} // namespace app_ext

// The engine object (image::engine).
namespace engine {
inline constexpr Offset renderer = 0xaf7b0; // the renderer it links
} // namespace engine

// sMhUnit, the unit manager (image::unit_manager), and its units.
namespace unit_manager {
inline constexpr Offset units = 0x15d0;    // the unit list's first node (game-aa-option.md)
inline constexpr Offset excluded = 0x4a50; // unit categories not drawn
} // namespace unit_manager
namespace unit {
inline constexpr Offset state = 0xc;     // TAA is drawn at 8 (activation's native_gates)
inline constexpr Offset flags = 0x14;    // and with 0x10 set
inline constexpr Offset displays = 0x1c; // bit per display
inline constexpr Offset next = 0x30;     // the next unit in the manager's list
inline constexpr Offset category = 0x40;
inline constexpr Offset mask = 0x48; // matched against context::unit_mask
} // namespace unit

// uTemporalAAFilter2, the game's TAA (a unit; image::taa2_vtable).
namespace taa_filter {
inline constexpr Offset queue_entry = 0x140; // its post-queue entry, embedded
inline constexpr Offset owner = 0x148;       // itself while registered
inline constexpr Offset base_color = 0x150;
inline constexpr Offset skip = 0x160;        // byte
inline constexpr Offset passthrough = 0x161; // byte
inline constexpr Offset history = 0x168;     // three owners, by frame % 3
inline constexpr Offset taa_bytes = 0x184;   // read with the inputs (inputs.cpp)
} // namespace taa_filter

// The post-effect queue (image::post_queue) and its entries.
namespace post_queue {
inline constexpr Offset enabled = 0x38;    // byte
inline constexpr Offset collecting = 0x39; // byte
inline constexpr Offset count = 0x3c;
inline constexpr Offset entries = 0x40; // 16 pointers; two scratch targets follow at +0xc0
} // namespace post_queue
namespace post_entry {
inline constexpr Offset stage = 0x0;
inline constexpr Offset order = 0x4;
inline constexpr Offset unit = 0x8;
inline constexpr Offset skip = 0x20; // byte
} // namespace post_entry
} // namespace mhw::layout
