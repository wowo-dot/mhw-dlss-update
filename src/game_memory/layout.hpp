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
inline constexpr Offset area = 0x51c42b8;         // sArea* (sArea's destructor clears it; marie-2)
inline constexpr Offset game_dlss_state = 0x51c6c28;     // why the game has no DLSS 1: 1 NGX init, 2 SuperSampling,
                                                         // 3 ImageSignalProcessing unavailable (game-dlss1.md)
inline constexpr Offset ngx_slots = 0x51d8808;           // the game's NGX client slots (ngx-startup-capabilities.md)
inline constexpr Offset graphics_option_ini = 0x2f744b0; // {"app:\\graphics_option.ini", "GraphicsOption"}: the
                                                         // saved settings' file and section (game_options)
inline constexpr Offset final_base_map_id = 0x4be7450;   // the final quad's tBaseMap binding id (stream_route)
inline constexpr Offset shader = 0x51c73d0;              // sShader*: the default depth owner, the sampler banks
                                                         // (attachments.cpp, texture-mip-bias.md)

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
inline constexpr Offset cas = 0xbc;                // byte: FidelityFX CAS on (0x43de50, game-cas-option.md)
inline constexpr Offset cas_sharpness = 0xc0;      // float
inline constexpr Offset cas_scale = 0xc4;          // float: 0.8 with CAS on; content_scale_desired = it x 0xb0
inline constexpr Offset scene_rect = 0x188;        // int[4]: the scene rectangle in the window (live-aspect.md)
inline constexpr Offset scene_width = 0x198;       // the scene size (live-aspect.md)
inline constexpr Offset scene_height = 0x19c;
inline constexpr Offset scale_desired = 0x1f0; // float: the render scale asked for (the mod writes it;
inline constexpr Offset scale_applied = 0x1f4; // dlss-render-resolution-control.md: two coupled channels)
inline constexpr Offset scale_previous = 0x1f8;
inline constexpr Offset content_scale_desired = 0x200;
inline constexpr Offset content_scale = 0x204;
inline constexpr Offset combined_scale = 0x208; // the clamped product
// CBRenderFrame's HDR10 fields come from here (0x142288bd0, nvsharpen.md).
inline constexpr Offset hdr_output = 0x21a;      // byte: bHdrOutput (latched from +0x219, 0x142293435)
inline constexpr Offset hdr_white_level = 0x21c; // float: fHdrOutputWhiteLevel, nits (init 300)
inline constexpr Offset context = 0x710;         // the primary render context, embedded (depth-copy-frame-map.md)
inline constexpr Offset bank = 0x1f200;          // the command bank, toggled each frame (queued-work-provenance.md)
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
inline constexpr Offset barrier_plan = 0x7af18;       // the executing frame's packets in execution order
inline constexpr Offset barrier_plan_count = 0x7af28; // (sub_14229CEE0; full_post_slot.inl)
} // namespace renderer

// cD3DRender, the D3D12 backend (image::d3d_render, renderer::backend).
namespace d3d_render {
inline constexpr Offset device = 0x18;        // ID3D12Device*
inline constexpr Offset compute_root = 0x2a0; // the compute root signature recordings cache (dlaa-output-handoff.md)
inline constexpr Offset latency_waitable = 0x10f0; // the swap chain's frame latency waitable (fg-live-toggle.md)
inline constexpr Offset swap_chain = 0x1470;       // IDXGISwapChain*
inline constexpr Offset sampler_tables = 0x1370;   // the resident sampler table cache (sampler_table_cache)
// The per-frame constant arena (memory.cpp read_arena).
inline constexpr Offset arena_mode = 0xff0; // 2: one resource for the CPU and the GPU
inline constexpr Offset arena_stride = 0x1000;
inline constexpr Offset arena_cpu = 0x1018;  // three CPU pointers
inline constexpr Offset arena_used = 0x1030; // three 4-byte fill levels
inline constexpr Offset arena_index = 0x103c;
inline constexpr Offset arena_cpu_resource = 0x1058;
inline constexpr Offset arena_gpu_resource = 0x1078;
inline constexpr Offset arena_gpu = 0x1080; // three GPU addresses
inline constexpr Offset arena_delta = 0x1098;
inline constexpr Offset game_dlss = 0x42700; // the game's own DLSS 1 NGX context (game-dlss1.md)
} // namespace d3d_render

// The render context (renderer::context): the frame's draw state.
namespace context {
inline constexpr Offset active_targets = 0x10; // the target set last bound (attachments.cpp)
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
inline constexpr Offset vertex_arena = 0x2b50; // the frame's vertex data (full_post's final quad)
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
inline constexpr Offset targets = 0x60; // the render target slots (attachments), 0x40 bytes each
inline constexpr Offset target_stride = 0x40;
inline constexpr Offset scene_target = 0x420; // slot 15: the final quad's tBaseMap (full-size-post-chain.md)
inline constexpr Offset final_target = 0x460; // slot 16: the view's final attachment (dlaa-identity-path.md)
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
inline constexpr Offset default_bias = 0x1a4;       // float: mDefaultBias (ctor 0x22646f0)
inline constexpr Offset sampler_lod_bias = 0x1a8;   // u32: mSamplerLodBias, the sampler bank a material bakes at load
                                                    // (0x2383566; 0 as shipped; texture-mip-bias.md)
inline constexpr Offset jitter_table = 0xe6a0;      // 20 projection candidates, 8 bytes each
inline constexpr Offset capture_texture = 0xe988;   // the result screen's capture: drawn into this texture
inline constexpr Offset capture_pass = 0xe990;      // from this view target slot (default 15)
inline constexpr Offset capture_requested = 0xe998; // at this renderer epoch (full-size-post-chain.md)
inline constexpr Offset taa = 0xe9e4;               // byte: the player's TAA (game-aa-option.md)
inline constexpr Offset fxaa = 0xe9e5;              // byte: the player's FXAA
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
inline constexpr Offset skip = 0x160;         // byte
inline constexpr Offset passthrough = 0x161;  // byte
inline constexpr Offset history = 0x168;      // three owners, by frame % 3
inline constexpr Offset taa_bytes = 0x184;    // read with the inputs (inputs.cpp)
inline constexpr Offset reprojection = 0x19d; // byte, property mReprojection: 0 picks CSTemporalAA2's
                                              // _NO_REPROJECTION technique (sub_142392150; marie-2)
} // namespace taa_filter

// sArea, the loaded area (image::area).
namespace area {
inline constexpr Offset stage = 0xd328; // int: the stage id (the cat helper setter sub_141B34AE0 compares it
                                        // with 108, the Hoarfrost Reach; read in 192 places)
} // namespace area

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
inline constexpr Offset input = 0x10;
inline constexpr Offset output = 0x18;
inline constexpr Offset skip = 0x20; // byte
} // namespace post_entry

// A camera viewport (camera::viewports).
namespace camera_viewport {
inline constexpr Offset scene_texture = 0x18; // null for the scene's viewport
inline constexpr Offset active = 0x20;        // byte
inline constexpr Offset display = 0x25;       // byte: zero for the scene's viewport
} // namespace camera_viewport

// A texture owner: the engine's object for a texture or render target (inputs.cpp owner()).
namespace texture_owner {
inline constexpr Offset kind = 0x58;    // 6: a target the copy executor skips
inline constexpr Offset backing = 0x60; // the backing (below)
inline constexpr Offset flags = 0x70;
inline constexpr Offset srv = 0x78;              // its shader resource view (descriptor_view)
inline constexpr Offset depth_format = 0x74;     // a depth target view's format
inline constexpr Offset dimensions = 0x88;       // a 16-bit width, a 14-bit height, two flag bits
inline constexpr Offset format = 0x90;           // the engine's format
inline constexpr Offset gpu_format = 0x94;       // the DXGI format
inline constexpr Offset backing_override = 0xf0; // used instead of backing when set
} // namespace texture_owner

// The backing of a texture or buffer owner: its D3D12 resource.
namespace backing {
inline constexpr Offset bytes = 0x10;       // a buffer's size
inline constexpr Offset resource = 0x20;    // ID3D12Resource*
inline constexpr Offset gpu_address = 0x40; // a buffer's GPU virtual address
inline constexpr Offset tracker = 0x70;     // its state tracker (below)
inline constexpr Offset width = 0xb0;
inline constexpr Offset height = 0xb4;
inline constexpr Offset format = 0xc4;
} // namespace backing

// A backing's state tracker (backing::tracker).
namespace state_tracker {
inline constexpr Offset epoch = 0x10;       // the frame epoch it was last set in
inline constexpr Offset initial = 0x18;     // the state the frame began in
inline constexpr Offset used = 0x20;        // the epoch it was last used in
inline constexpr Offset initialized = 0x28; // byte
inline constexpr Offset immediate = 0x30;   // byte
} // namespace state_tracker

// A resource state node in the barrier plan (renderer::barrier_plan) and in
// a packet's state list.
namespace state_node {
inline constexpr Offset next = 0x0;
inline constexpr Offset backing = 0x8;
inline constexpr Offset before = 0x18; // uint16: the state before
inline constexpr Offset after = 0x1a;  // uint16: the state after
} // namespace state_node

// A descriptor view: a shader resource or render target view.
namespace descriptor_view {
inline constexpr Offset cpu_handle = 0x10; // D3D12_CPU_DESCRIPTOR_HANDLE
} // namespace descriptor_view

// A view's render target slot (view::targets) or a context's attachment.
namespace attachment {
inline constexpr Offset owner = 0x58; // the texture owner
} // namespace attachment

// A command recording: the state the executors record into a list with.
namespace recording {
inline constexpr Offset command = 0x20;         // ID3D12GraphicsCommandList*
inline constexpr Offset cached_pipeline = 0x98; // the pipeline last set on the list
inline constexpr Offset viewport = 0xe0;        // the viewport and scissor last set
inline constexpr Offset scissor = 0xf0;
inline constexpr Offset scissor_enabled = 0x100; // byte
inline constexpr Offset pipeline = 0x108;        // the current pipeline state
inline constexpr Offset graphics_root = 0x110;   // the current root signatures
inline constexpr Offset compute_root = 0x118;    // (dlaa-output-handoff.md)
inline constexpr Offset cached_depth_resource = 0x140;
inline constexpr Offset packet = 0x158; // the packet being executed
} // namespace recording
// A packet executor's first argument (attachments.cpp DrawSite::executor).
namespace executor {
inline constexpr Offset recording = 0x8;
} // namespace executor

// A queued packet: its first word holds the opcode in its low 5 bits.
namespace packet {
inline constexpr Offset opcode = 0x0;
inline constexpr Offset states = 0x8; // its resource state list (state_node)
} // namespace packet
// The 16-byte entries of context::packets and renderer::barrier_plan.
namespace packet_entry {
inline constexpr Offset key = 0x0; // the sort key
inline constexpr Offset packet = 0x8;
} // namespace packet_entry
// A packet executor's second argument.
namespace executor_device {
inline constexpr Offset backend = 0x8; // cD3DRender*
} // namespace executor_device
// Opcode 0, a draw.
namespace draw_packet {
inline constexpr Offset bindings = 0x10;       // four pointers: the target set (target_set), then:
inline constexpr Offset input_assembly = 0x18; // the vertex and index buffers
inline constexpr Offset graphics = 0x20;       // the graphics binding state (binding_state)
inline constexpr Offset render_state = 0x28;   // the blend, depth and raster owners
inline constexpr Offset pipeline = 0x38;       // the pipeline state it was recorded for
inline constexpr Offset arguments = 0x40;      // the draw's count, then first
inline constexpr Offset first = 0x44;
} // namespace draw_packet
// A draw packet's input assembly (draw_packet::input_assembly).
namespace input_assembly {
inline constexpr Offset vertex_owners = 0x0; // four buffer owners (texture_owner), 8 bytes each
inline constexpr Offset strides = 0x20;      // a byte per stream
inline constexpr Offset offsets = 0x24;      // four bytes per stream
inline constexpr Offset index_owner = 0x38;
inline constexpr Offset index_offset = 0x40;
} // namespace input_assembly
// A draw packet's target set (draw_packet::bindings[0]).
namespace target_set {
inline constexpr Offset colors = 0x0;     // up to eight target views (texture owners), 8 bytes each
inline constexpr Offset depth = 0x40;     // the depth target view
inline constexpr Offset flags = 0x48;     // the color count in bits 0-2
inline constexpr Offset rectangle = 0x4c; // unsigned[4]
} // namespace target_set
// Opcode 6, a dispatch.
namespace dispatch_packet {
inline constexpr Offset state = 0x10;    // its binding state
inline constexpr Offset groups_x = 0x18; // the thread groups: x, y, z
inline constexpr Offset groups_y = 0x1c;
inline constexpr Offset groups_z = 0x20;
inline constexpr Offset mode = 0x24;
inline constexpr Offset indirect = 0x28;
} // namespace dispatch_packet
// Opcode 10, a copy (the executor 0x259df30).
namespace copy_packet {
inline constexpr Offset kind = 0x10;        // 8: a whole copy or a child draw
inline constexpr Offset destination = 0x18; // texture owners
inline constexpr Offset source = 0x20;
inline constexpr Offset rectangle = 0x28;
inline constexpr Offset child = 0x40; // the draw it may execute instead
} // namespace copy_packet
// Opcode 13, a region copy (sub_1423C9250).
namespace region_copy_packet {
inline constexpr Offset destination = 0x10; // texture owners
inline constexpr Offset source = 0x18;
inline constexpr Offset mode = 0x20;
inline constexpr Offset destination_subresource = 0x24;
inline constexpr Offset source_subresource = 0x28;
inline constexpr Offset destination_xy = 0x30;
inline constexpr Offset source_rect = 0x38;
inline constexpr Offset optional = 0x48; // a pointer, null for a plain copy
} // namespace region_copy_packet

// A target view's backing (texture_owner::backing of a target set's view).
namespace target_backing {
inline constexpr Offset handle = 0x10;   // its descriptor handle
inline constexpr Offset resource = 0x18; // ID3D12Resource*
inline constexpr Offset format = 0x20;
inline constexpr Offset rotation = 0x28; // byte: a rotating set of handles (not read further)
inline constexpr Offset rotation_index = 0x2c;
} // namespace target_backing

// sShader (image::shader).
namespace shader {
inline constexpr Offset depth_owner = 0x2480;   // points to the default depth owner
inline constexpr Offset sampler_banks = 0x2488; // 15 pointers to 63 sampler states each, built once at
                                                // shader package load (0x23d6690, texture-mip-bias.md)
inline constexpr unsigned sampler_bank_count = 15, sampler_bank_states = 63;
} // namespace shader

// nDraw::SamplerState (vtable 0x357de88, ctor 0x263d890; texture-mip-bias.md).
namespace sampler_state {
inline constexpr Offset sampler_template = 0x58; // 36 bytes: packed filter/address/anisotropy/comparison word,
inline constexpr Offset mip_bias = 0x5c;         // then float mip bias, MinLOD, MaxLOD, border[4], u32 id
inline constexpr Offset max_lod = 0x64;
inline constexpr Offset id = 0x78;     // the template's index in its bank
inline constexpr Offset native = 0x80; // the native sampler at anisotropy 1, then at 2/4/8/16 (0x263daf0)
} // namespace sampler_state

// The resident sampler table cache (d3d_render::sampler_tables, 0x2624870): it
// never evicts and never checks its bound.
namespace sampler_table_cache {
inline constexpr Offset count = 0x8;     // u32: tables made
inline constexpr Offset next = 0xc;      // u32: descriptor slots used in mResidentDescHeapSampler
inline constexpr Offset capacity = 0x10; // u32: 2048 entries (the heap also holds 2048)
} // namespace sampler_table_cache

// The graphics or compute binding state (draw_packet::graphics, context::state).
namespace binding_state {
inline constexpr Offset flags = 0x0;
inline constexpr Offset shader = 0x8; // points to the shader set (shader_set)
inline constexpr Offset modes = 0x10; // a byte per bound owner
inline constexpr Offset technique = 0x18;
inline constexpr Offset input_layout = 0x20;
inline constexpr Offset objects = 0x30; // the bound owners, 8 bytes each
} // namespace binding_state
namespace input_layout {
inline constexpr Offset elements = 0x10;
inline constexpr Offset count = 0x18;
} // namespace input_layout

// A shader set (binding_state::shader).
namespace shader_set {
inline constexpr Offset words = 0x0; // the binding words
inline constexpr Offset range = 0x14;
inline constexpr Offset binding_count = 0x44; // uint16
inline constexpr Offset binding_start = 0x46; // uint16
inline constexpr Offset stages = 0x50;        // the stage entries, 8 bytes each: vertex,
inline constexpr Offset pixel_stage = 0x58;   // pixel,
inline constexpr Offset extra_stages = 0x60;  // three more,
inline constexpr Offset compute_stage = 0x78; // compute
inline constexpr Offset ids = 0x80;           // the binding ids of the owners
inline constexpr Offset owner_count = 0x88;
} // namespace shader_set
namespace shader_stage {
inline constexpr Offset code = 0x10; // the bytecode descriptor (shader_code)
} // namespace shader_stage
namespace shader_code {
inline constexpr Offset size = 0x0;
inline constexpr Offset bytecode = 0x8;
} // namespace shader_code

// A draw packet's render state (draw_packet::render_state).
namespace render_state {
inline constexpr Offset variant = 0x4; // byte: bit 0 selects the raster state
inline constexpr Offset blend_owner = 0x8;
inline constexpr Offset depth_owner = 0x10;
inline constexpr Offset raster_owner = 0x18;
} // namespace render_state
namespace blend_owner {
inline constexpr Offset state = 0x98;
} // namespace blend_owner
namespace depth_owner {
inline constexpr Offset state = 0x60;
} // namespace depth_owner
namespace raster_owner {
inline constexpr Offset states = 0x68; // two, by render_state::variant
} // namespace raster_owner
namespace pipeline_state {
inline constexpr Offset desc = 0x10; // the D3D12 description
} // namespace pipeline_state

// uToneMapFilter, the grading unit (image::tone_map_vtable).
namespace tone_map_filter {
inline constexpr Offset output = 0x250; // the grade output (dlaa-identity-path.md)
} // namespace tone_map_filter
} // namespace mhw::layout
