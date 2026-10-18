// All access serialized by the route lock. No native bytecode is modified.
std::array<std::shared_ptr<SilhouetteCpu>, 64> silhouette_cpu_cache;
struct SilhouettePipeline {
    std::shared_ptr<SilhouetteCpu> recipe;
    std::unique_ptr<SilhouetteDraw> draw;
};
std::array<SilhouettePipeline, 64> silhouette_pipelines;
std::uint64_t silhouette_recorded{}, silhouette_native{}, silhouette_corrected{}, silhouette_restored{};
std::array<std::uint64_t, 12> silhouette_rejections{};
unsigned silhouette_cache_count{};
// Pixel shaders outside the allowlist, by engine code identity, so a rejected
// program is copied and hashed once rather than on every draw. A stale entry
// can only keep a draw native.
struct SilhouetteUnknown {
    std::uintptr_t set{}, data{};
    unsigned size{};
    ShaderHash hash{};
    std::uint64_t draws{};
};
std::array<SilhouetteUnknown, 32> silhouette_unknown{};
unsigned silhouette_unknown_count{};
std::uint64_t silhouette_unknown_draws{}, silhouette_cache_full{}, silhouette_recipe_failed{};
ShaderHash silhouette_unknown_hash{};
// Diagnostics: the first distinct draws per rejection reason (recipe and the
// failed check's values), so a rejected class can be admitted deliberately.
struct SilhouetteSample {
    unsigned reason{};
    const SilhouetteCpu *recipe{};
    std::uint64_t draws{};
    std::array<unsigned, 24> detail{};
    unsigned detail_count{};
};
std::array<SilhouetteSample, 24> silhouette_samples{};
unsigned silhouette_sample_count{};
std::uint64_t silhouette_samples_dropped{};
void silhouette_sample(unsigned reason, const SilhouetteCpu *recipe, std::initializer_list<unsigned> detail) {
    const auto n = (std::min<std::size_t>)(detail.size(), 24);
    for (unsigned i = 0; i < silhouette_sample_count; ++i) {
        auto &s = silhouette_samples[i];
        if (s.reason == reason && s.recipe == recipe && s.detail_count == n &&
            std::equal(detail.begin(), detail.begin() + n, s.detail.begin())) {
            ++s.draws;
            return;
        }
    }
    if (silhouette_sample_count == silhouette_samples.size()) {
        ++silhouette_samples_dropped;
        return;
    }
    auto &s = silhouette_samples[silhouette_sample_count++];
    s.reason = reason;
    s.recipe = recipe;
    s.draws = 1;
    s.detail_count = static_cast<unsigned>(n);
    std::copy(detail.begin(), detail.begin() + n, s.detail.begin());
}
bool silhouette_code_stable(const SilhouetteCpu &r) {
    for (unsigned i = 0; i < 2; ++i) {
        const auto &s = r.identity[i];
        std::uintptr_t entry{}, desc{}, data{};
        unsigned size{};
        if (!field(r.set, layout::shader_set::stages + 8 * i, entry) || entry != s.entry ||
            !field(entry, layout::shader_stage::code, desc) || desc != s.descriptor ||
            !field(desc, layout::shader_code::size, size) || size != s.size ||
            !field(desc, layout::shader_code::bytecode, data) || data != s.data)
            return false;
        std::array<std::byte, 1024> block{};
        for (unsigned at = 0; at < size; at += 1024) {
            const auto n = (std::min)(1024U, size - at);
            if (!pb::copy_memory(data + at, block.data(), n) || std::memcmp(block.data(), r.bytes[i].data() + at, n))
                return false;
        }
    }
    return true;
}
bool silhouette_recipe_equal(pb::NativePipeline a, pb::NativePipeline b) {
    a.render = b.render = 0;
    a.graphics = b.graphics = 0;
    return a == b;
}
// A late draw whose pipeline writes no colour, depth or stencil leaves nothing
// at the render viewport, so it stays native with nothing to correct
// (rejection 11; v0.24.1). These are the eye draws (game-58: write mask 0,
// depth and stencil off) and the two Simple_Mt_AlphaTest RenderTransparent
// draws (PS 9c5c0c85...; RenderDoc, vanilla 2560x1440 frame: write mask 0, no
// depth target, no pixel changed; the program declares no UAV). Checked from
// the engine's pipeline state before any shader lookup, whatever the program.
bool silhouette_silent(const pb::NativePipeline &n) {
    D3D12_BLEND_DESC blend{};
    D3D12_DEPTH_STENCIL_DESC depth{};
    static_assert(sizeof(n.blend) >= sizeof(blend) && sizeof(n.depth) >= sizeof(depth));
    std::memcpy(&blend, n.blend.data(), sizeof(blend));
    std::memcpy(&depth, n.depth.data(), sizeof(depth));
    for (unsigned i = 0; i < (blend.IndependentBlendEnable ? 8U : 1U); ++i)
        if (blend.RenderTarget[i].RenderTargetWriteMask) return false;
    return (!depth.DepthEnable || depth.DepthWriteMask == D3D12_DEPTH_WRITE_MASK_ZERO) &&
           (!depth.StencilEnable || !depth.StencilWriteMask);
}
std::shared_ptr<SilhouetteCpu> silhouette_cpu_lookup(std::uintptr_t packet, bool &silent) {
    pb::NativePipeline native;
    std::uintptr_t set{};
    if (!pb::snapshot_native_pipeline(cfg.image, packet, native)) return {};
    if (silhouette_silent(native)) {
        silent = true;
        return {};
    }
    if (!field(native.shader_handle, 0, set) || !set || native.input_count > 32) return {};
    for (const auto &cached : silhouette_cpu_cache)
        if (cached && cached->set == set && silhouette_recipe_equal(cached->native, native)) {
            if (!layout(cached->layout, true) || !silhouette_code_stable(*cached)) return {};
            return cached;
        }
    {
        std::uintptr_t entry{}, descriptor{}, data{};
        unsigned size{};
        if (field(set, layout::shader_set::pixel_stage, entry) &&
            field(entry, layout::shader_stage::code, descriptor) &&
            field(descriptor, layout::shader_code::size, size) &&
            field(descriptor, layout::shader_code::bytecode, data))
            for (unsigned i = 0; i < silhouette_unknown_count; ++i) {
                auto &u = silhouette_unknown[i];
                if (u.set == set && u.data == data && u.size == size) {
                    ++silhouette_unknown_draws;
                    ++u.draws;
                    return {};
                }
            }
    }
    unsigned empty = UINT_MAX;
    for (unsigned i = 0; i < silhouette_cpu_cache.size(); ++i)
        if (!silhouette_cpu_cache[i]) {
            empty = i;
            break;
        }
    if (empty == UINT_MAX) {
        ++silhouette_cache_full;
        return {};
    }
    auto r = std::make_shared<SilhouetteCpu>();
    r->native = native;
    r->set = set;
    r->layout.pipeline = native;
    if (!layout(r->layout, false)) return {};
    for (unsigned i = 0; i < 2; ++i) {
        auto &s = r->identity[i];
        if (!field(set, layout::shader_set::stages + 8 * i, s.entry) ||
            !field(s.entry, layout::shader_stage::code, s.descriptor) ||
            !field(s.descriptor, layout::shader_code::size, s.size) || s.size < 52 || s.size > 131072 ||
            !field(s.descriptor, layout::shader_code::bytecode, s.data))
            return {};
        r->bytes[i].resize(s.size);
        if (!pb::copy_memory(s.data, r->bytes[i].data(), s.size) || !pb::dxbc_valid(r->bytes[i].data(), s.size))
            return {};
    }
    if (!known_silhouette_shader(r->bytes[1])) {
        ++silhouette_unknown_draws;
        shader_sha256(r->bytes[1], silhouette_unknown_hash);
        if (silhouette_unknown_count < silhouette_unknown.size())
            silhouette_unknown[silhouette_unknown_count++] = {set, r->identity[1].data, r->identity[1].size,
                                                              silhouette_unknown_hash, 1};
        return {};
    }
    shader_sha256(r->bytes[0], r->vs_hash);
    shader_sha256(r->bytes[1], r->ps_hash);
    silhouette_cpu_cache[empty] = r;
    return r;
}
// Late packets resolve their recipe here; a null recipe keeps the draw native
// (rejection 0, or 11 for a draw that writes nothing). Counts lookups that
// failed for a reason other than an unknown pixel shader, a full cache or a
// draw that writes nothing.
std::shared_ptr<SilhouetteCpu> silhouette_cpu(std::uintptr_t packet, bool &silent) {
    const auto unknown = silhouette_unknown_draws, full = silhouette_cache_full;
    auto r = silhouette_cpu_lookup(packet, silent);
    if (!r && !silent && unknown == silhouette_unknown_draws && full == silhouette_cache_full)
        ++silhouette_recipe_failed;
    return r;
}
IndexedScope indexed_before(const pb::Frame &f) noexcept {
    auto scope = indexed_diagnostic_before(f);
    try {
        Lock lock;
        if (fault.load() || abandoned) return scope;
        unsigned slot = UINT_MAX, position{};
        for (unsigned i = 0; i < plans.size(); ++i) {
            const auto &p = plans[i];
            if (!p.built) continue;
            for (unsigned j = 0; j < p.late_count; ++j)
                if (!(p.late_executed & (1ULL << j)) && p.late_packets[j] == f.gpr[3]) {
                    if (slot != UINT_MAX) {
                        fail(14);
                        return scope;
                    }
                    slot = i;
                    position = j;
                }
        }
        if (slot == UINT_MAX) return scope;
        auto &p = plans[slot];
        p.late_executed |= 1ULL << position;
        if (!p.prepared) return scope;
        scope.slot = slot;
        scope.position = position;
        scope.calls = 0;
        scope.replaced = false;
        scope.draw.packet = f.gpr[3];
        scope.draw.executor = f.gpr[1];
        scope.draw.context = f.gpr[2];
        if (!field(f.gpr[1], layout::executor::recording, scope.state) ||
            !field(scope.state, layout::recording::command, scope.draw.command))
            fail(14);
    } catch (...) {
        Lock lock;
        fail(14);
    }
    return scope;
}
// CBScreen (32-bit words): [4..5] fScreenSize (output), [6..7] fScreenInverseSize,
// [10..11] iViewSize (render), [12..13] fViewOffset, [14..15] fViewSize,
// [16..17] fViewInverseSize = 1 / render. At 2:1, 1/render equals 2/output,
// which the first checks assumed (game-61: every draw rejected at Quality).
// Engine global binding IDs (shader census): CBScreen 12, CBMhMaterialGlobal
// 433, tVolumeBlendDepthMap 159, tGBuffer1 41. The hidden-surface variants bind
// them in different registers (mask t1..t20, material b5 or b6); only some
// sample depth. Screen, material and mask are required.
bool silhouette_bindings(const LatePacket &r, std::array<unsigned, 28> &screen, std::array<unsigned, 5> &material,
                         std::array<ComPtr<ID3D12Resource>, 2> &textures) {
    const auto set = r.recipe->set, graphics = r.bindings[2];
    std::uintptr_t words{}, ids{}, modes{};
    unsigned count{};
    std::array<unsigned short, 2> range{};
    if (!field(set, layout::shader_set::words, words) || !field(set, layout::shader_set::ids, ids) ||
        !field(set, layout::shader_set::owner_count, count) || count > 64 ||
        !field(graphics, layout::binding_state::modes, modes) || !field(set, layout::shader_set::range, range) ||
        range[0] > 128 || unsigned(range[0]) + range[1] > 4096)
        return false;
    std::array<unsigned, 128> binding{};
    unsigned found{};
    if (!pb::copy_memory(words + 4ULL * range[1], binding.data(), 4 * range[0])) return false;
    for (unsigned i = 0; i < range[0]; ++i) {
        const auto w = binding[i], kind = w & 15, index = (w >> 4) & 255;
        if ((kind != 0 && kind != 2) || index >= count) continue;
        unsigned id{};
        if (!field(ids, index * 4, id)) return false;
        const unsigned wanted = kind == 0 && id == 12    ? 0
                                : kind == 0 && id == 433 ? 1
                                : kind == 2 && id == 159 ? 2
                                : kind == 2 && id == 41  ? 3
                                                         : UINT_MAX;
        if (wanted == UINT_MAX) continue;
        unsigned char mode{};
        std::uintptr_t object{};
        if ((found & (1U << wanted)) || !field(modes, index, mode) || mode ||
            !field(graphics, layout::binding_state::objects + 8 * index, object) || !object)
            return false;
        found |= 1U << wanted;
        if (wanted < 2) {
            if (wanted == 0 ? !pb::copy_memory(object, screen.data(), sizeof(screen))
                            : !pb::copy_memory(object, material.data(), sizeof(material)))
                return false;
        } else {
            tb::Owner owner;
            if (!tb::read_owner(reader, object, owner) || !indexed_description(owner.resource)[1]) return false;
            textures[wanted - 2] = reinterpret_cast<ID3D12Resource *>(owner.resource);
        }
    }
    return (found & 11U) == 11U;
}
// 0 when the vertex and index buffers qualify, else the first failed check
// (1-5 raster state; vertex stream 0 6-12, streams 1-3 20/30/40 + 0-7; index
// buffer 14-18). `seen` keeps the failed buffer's resource facts for
// diagnostics: valid, dimension, width, engine address - resource base, engine
// byte count.
//
// The engine's input-assembly object (sub_14259E470, clean image) holds up to
// four vertex streams: owners at +0x00/+0x08/+0x10/+0x18, strides as bytes at
// +0x20..+0x23, offsets at +0x24/+0x28/+0x2c/+0x30; the index owner at +0x38
// and its offset at +0x40. It binds streams 0..last, a zero view for a null
// owner in between. The hunter's AlphaDither silhouette meshes bind four
// (game-100: 75,740 draws stayed native before v0.24.3); the others one.
// Slots above the engine's streams keep what an earlier draw bound (game-101:
// 14,027 draws with three streams after a four-stream one, v0.24.4): they
// are allowed when the input layout reads only the engine's streams.
//
// The engine's buffer may be a range inside a larger D3D12 buffer: the
// resource must contain [address, address + bytes), and the bound views are
// then checked against that range exactly.
//
// The shield's index range (MeshSkin1Weight Uber_Mt_General and
// PL_Mt_AlphaTest, game-59) is held by an engine object that is not an
// ID3D12Resource. That index buffer alone is accepted without a resource when
// the bound view matches the engine range exactly, and it is not pinned
// (buffers[4] stays null). The replacement is recorded right after the native
// draw in the same list, reads the same bound views, and the predicate
// executes exactly one of them. So the game's own lifetime guarantee for that
// index data covers the replacement.
unsigned silhouette_buffers(const LatePacket &r, const obs::RasterBindings &b,
                            std::array<ComPtr<ID3D12Resource>, 5> &buffers, std::array<std::uint64_t, 5> &seen) {
    if (!b.ready()) return 1;
    if (!b.index_known) return 3;
    if (b.topology != D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST) return 4;
    if (b.index_buffer.Format != DXGI_FORMAT_R16_UINT) return 5;
    const auto ia = r.bindings[1];
    std::array<std::uintptr_t, 4> owners{};
    unsigned streams{};
    for (unsigned k = 0; k < 4; ++k) {
        if (!field(ia, layout::input_assembly::vertex_owners + 8 * k, owners[k])) return 6;
        if (owners[k]) streams = k + 1;
    }
    const auto needed = (1U << streams) - 1;
    if (!owners[0] || (b.vertex_mask & needed) != needed || !r.recipe ||
        r.recipe->native.input_count > r.recipe->layout.elements.size())
        return 2;
    for (unsigned i = 0; i < r.recipe->native.input_count; ++i)
        if (r.recipe->layout.elements[i].InputSlot >= streams) return 2;
    const auto code = [](unsigned k, unsigned c) { return k ? 10 + 10 * k + c : 6 + c; };
    for (unsigned k = 0; k < streams; ++k) {
        const auto &view = b.vertex_buffers[k];
        if (!owners[k]) {
            if (view.BufferLocation || view.SizeInBytes) return code(k, 7);
            continue;
        }
        std::uintptr_t backend{}, resource{}, address{};
        unsigned bytes{}, offset{};
        unsigned char stride{};
        if (!field(owners[k], layout::texture_owner::backing, backend) ||
            !field(backend, layout::backing::resource, resource) ||
            !field(backend, layout::backing::gpu_address, address) || !field(backend, layout::backing::bytes, bytes) ||
            !field(ia, layout::input_assembly::offsets + 4 * k, offset) || offset > bytes ||
            address > UINTPTR_MAX - offset)
            return code(k, 0);
        const auto description = indexed_description(resource);
        seen = {description[1], description[2], description[3], address - description[13], bytes};
        if (!description[1] || description[2] != D3D12_RESOURCE_DIMENSION_BUFFER || address < description[13] ||
            address - description[13] > description[3] || bytes > description[3] - (address - description[13]))
            return code(k, 1);
        if (!field(ia, layout::input_assembly::strides + k, stride)) return code(k, 2);
        if (stride > 128 || stride % 4 || stride < (k ? 4 : 12)) return code(k, 3);
        if (view.BufferLocation != address + offset) return code(k, 4);
        if (view.SizeInBytes != bytes - offset) return code(k, 5);
        if (view.StrideInBytes != stride) return code(k, 6);
        buffers[k] = reinterpret_cast<ID3D12Resource *>(resource);
    }
    std::uintptr_t owner{}, backend{}, resource{}, address{};
    unsigned bytes{}, offset{};
    if (!field(ia, layout::input_assembly::index_owner, owner) ||
        !field(owner, layout::texture_owner::backing, backend) ||
        !field(backend, layout::backing::resource, resource) ||
        !field(backend, layout::backing::gpu_address, address) || !field(backend, layout::backing::bytes, bytes) ||
        !field(ia, layout::input_assembly::index_offset, offset) || offset > bytes || address > UINTPTR_MAX - offset)
        return 14;
    const auto description = indexed_description(resource);
    seen = {description[1], description[2], description[3], address - description[13], bytes};
    const bool unpinned = !description[1];
    if (!unpinned &&
        (description[2] != D3D12_RESOURCE_DIMENSION_BUFFER || address < description[13] ||
         address - description[13] > description[3] || bytes > description[3] - (address - description[13])))
        return 15;
    if (b.index_buffer.BufferLocation != address + offset) return 16;
    if (b.index_buffer.SizeInBytes != bytes - offset) return 17;
    if ((UINT64(r.arguments[0]) + r.arguments[1]) * 2 > b.index_buffer.SizeInBytes) return 18;
    if (!unpinned) buffers[4] = reinterpret_cast<ID3D12Resource *>(resource);
    return 0;
}
SilhouettePipeline *silhouette_pipeline(const std::shared_ptr<SilhouetteCpu> &r, ID3D12RootSignature *root) {
    for (auto &p : silhouette_pipelines)
        if (p.draw && p.recipe == r && p.draw->root() == root) return &p;
    SilhouettePipeline *slot{};
    for (auto &p : silhouette_pipelines)
        if (!p.draw) {
            slot = &p;
            break;
        }
    if (!slot) return nullptr;
    auto &layout_record = r->layout;
    auto input = layout_record.elements;
    for (unsigned i = 0; i < r->native.input_count; ++i) input[i].SemanticName = layout_record.semantics[i].data();
    SilhouetteRecipe recipe;
    recipe.geometry = {render_w, render_h, width, height};
    auto &d = recipe.native;
    d.pRootSignature = root;
    d.VS = {r->bytes[0].data(), r->bytes[0].size()};
    d.PS = {r->bytes[1].data(), r->bytes[1].size()};
    std::memcpy(&d.BlendState, r->native.blend.data(), sizeof(d.BlendState));
    std::memcpy(&d.RasterizerState, r->native.raster.data(), sizeof(d.RasterizerState));
    std::memcpy(&d.DepthStencilState, r->native.depth.data(), sizeof(d.DepthStencilState));
    d.InputLayout = {input.data(), r->native.input_count};
    d.SampleMask = UINT_MAX;
    d.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    d.NumRenderTargets = 1;
    d.RTVFormats[0] = DXGI_FORMAT_R11G11B10_FLOAT;
    d.SampleDesc.Count = 1;
    slot->draw = std::make_unique<SilhouetteDraw>(reinterpret_cast<ID3D12Device *>(cfg.device), recipe);
    slot->recipe = r;
    ++silhouette_cache_count;
    return slot;
}
bool record_indexed(ID3D12GraphicsCommandList *list, unsigned n, unsigned instances, unsigned first, int base,
                    unsigned instance, std::uintptr_t caller) {
    auto &scope = indexed_scope;
    auto &p = plans[scope.slot];
    auto &r = p.late[scope.position];
    std::array<std::uintptr_t, 4> bindings{};
    std::array<unsigned, 3> arguments{};
    temporal_present::Bindings roots;
    std::uintptr_t packet{}, root{}, pipeline{}, command{};
    if (!p.prepared || ++scope.calls != 1 || caller != cfg.image + 0x259d806 || instances != 1 || instance ||
        reinterpret_cast<std::uintptr_t>(list) != scope.draw.command ||
        !field(p.late_packets[scope.position], layout::draw_packet::bindings, bindings) || bindings != r.bindings ||
        !field(p.late_packets[scope.position], layout::draw_packet::arguments, arguments) || arguments != r.arguments ||
        arguments != std::array<unsigned, 3>{n, first, std::bit_cast<unsigned>(base)} || !obs::inspect(list, roots) ||
        !roots.known || !roots.valid || !field(scope.state, layout::recording::packet, packet) ||
        packet != scope.draw.packet || !field(scope.state, layout::recording::command, command) ||
        command != scope.draw.command || !field(scope.state, layout::recording::graphics_root, root) ||
        root != roots.graphics.signature || !field(scope.state, layout::recording::pipeline, pipeline) ||
        pipeline != roots.pipeline)
        throw std::runtime_error("silhouette indexed packet identity");
    // Even unadapted candidates participate in GPU order and selector lifetime.
    if (!track_recording(p, list, roots.generation, true) || !retain_recording(p, list, roots.generation) ||
        p.handoff.record(handoff_indexed(p, scope.position), scope.draw.packet,
                         {reinterpret_cast<std::uintptr_t>(list), roots.generation}) != FrameHandoff::Result::ok)
        throw std::runtime_error("silhouette recording ownership");
    ++silhouette_recorded;
    ++p.late_recorded;
    // full_post: the depth's render region back before the first late draw.
    if (p.identity && !scope.position) full_restore(p, list, scope.draw.packet);
    const auto native = [&](unsigned reason, std::initializer_list<unsigned> detail = {}) {
        ++silhouette_native;
        ++silhouette_rejections[reason];
        silhouette_sample(reason, r.recipe.get(), detail);
        completed(p);
        return false;
    };
    const auto u = [](auto v) { return static_cast<unsigned>(v); };
    if (!r.recipe || !n) return native(r.silent && n ? 11 : 0);
    pb::AttachmentSnapshot now;
    obs::RasterBindings raster;
    if (!pb::snapshot_attachments(cfg.image, scope.draw, now) || !pb::attachment_cache_matches(now) ||
        now.pipeline != r.native || now.color_count != 1 || now.views[8].owner ||
        now.views[0].resource != reinterpret_cast<std::uintptr_t>(p.resources[2].Get()) ||
        now.views[0].format != DXGI_FORMAT_R11G11B10_FLOAT || !obs::raster_current(list, raster) ||
        !raster.output_known || raster.render_target != now.views[0].handle || raster.generation != roots.generation) {
        const bool snapped = pb::snapshot_attachments(cfg.image, scope.draw, now),
                   rastered = obs::raster_current(list, raster);
        return native(1, {u(snapped), u(snapped && pb::attachment_cache_matches(now)), u(now.pipeline == r.native),
                          now.color_count, u(now.views[8].owner != 0),
                          u(now.views[0].resource == reinterpret_cast<std::uintptr_t>(p.resources[2].Get())),
                          u(now.views[0].format), u(rastered), u(raster.output_known),
                          u(raster.render_target == now.views[0].handle), u(raster.generation == roots.generation)});
    }
    D3D12_BLEND_DESC blend{};
    D3D12_DEPTH_STENCIL_DESC depth{};
    D3D12_RASTERIZER_DESC rs{};
    std::memcpy(&blend, now.pipeline.blend.data(), sizeof(blend));
    std::memcpy(&depth, now.pipeline.depth.data(), sizeof(depth));
    std::memcpy(&rs, now.pipeline.raster.data(), sizeof(rs));
    const auto &b = blend.RenderTarget[0];
    if (b.RenderTargetWriteMask != 15 || !b.BlendEnable || b.LogicOpEnable || b.SrcBlend != D3D12_BLEND_SRC_ALPHA ||
        b.DestBlend != D3D12_BLEND_INV_SRC_ALPHA || b.BlendOp != D3D12_BLEND_OP_ADD ||
        b.SrcBlendAlpha != D3D12_BLEND_ONE || b.DestBlendAlpha != D3D12_BLEND_ZERO ||
        b.BlendOpAlpha != D3D12_BLEND_OP_ADD || blend.AlphaToCoverageEnable || depth.DepthEnable ||
        depth.StencilEnable || rs.FillMode != D3D12_FILL_MODE_SOLID || !rs.DepthClipEnable || rs.DepthBias ||
        rs.DepthBiasClamp || rs.SlopeScaledDepthBias || rs.MultisampleEnable || rs.AntialiasedLineEnable ||
        rs.ForcedSampleCount || rs.ConservativeRaster != D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF)
        return native(2, {u(b.BlendEnable),
                          u(b.SrcBlend),
                          u(b.DestBlend),
                          u(b.BlendOp),
                          u(b.SrcBlendAlpha),
                          u(b.DestBlendAlpha),
                          u(b.BlendOpAlpha),
                          u(b.RenderTargetWriteMask),
                          u(b.LogicOpEnable),
                          u(blend.AlphaToCoverageEnable),
                          u(blend.IndependentBlendEnable),
                          u(depth.DepthEnable),
                          u(depth.DepthWriteMask),
                          u(depth.DepthFunc),
                          u(depth.StencilEnable),
                          u(rs.FillMode),
                          u(rs.CullMode),
                          u(rs.FrontCounterClockwise),
                          u(rs.DepthClipEnable),
                          u(rs.DepthBias),
                          u(rs.MultisampleEnable),
                          u(rs.AntialiasedLineEnable),
                          u(rs.ForcedSampleCount),
                          u(rs.ConservativeRaster)});
    for (unsigned i = 0; i < 3; ++i) {
        std::uintptr_t extra{};
        if (!field(r.recipe->set, layout::shader_set::extra_stages + 8 * i, extra) || extra) return native(3, {i});
    }
    const auto &v = raster.viewport;
    const auto &sc = raster.scissor;
    if (v.TopLeftX || v.TopLeftY || v.Width != render_w || v.Height != render_h || v.MinDepth || v.MaxDepth != 1 ||
        sc.left || sc.top || sc.right != LONG(render_w) || sc.bottom != LONG(render_h))
        return native(4, {u(v.TopLeftX), u(v.TopLeftY), u(v.Width), u(v.Height), u(v.MinDepth * 1000),
                          u(v.MaxDepth * 1000), u(sc.left), u(sc.top), u(sc.right), u(sc.bottom)});
    const auto state =
        recording_states.get(reinterpret_cast<std::uintptr_t>(list), roots.generation, now.views[0].resource);
    // The replacement and the native draw are predicated against each other on
    // the same render-target view, so they need the same state: the native draw's
    // own requirement. An unknown state (no barrier seen in this list; game-58,
    // about 1% of draws) is accepted; a known state other than RENDER_TARGET is not.
    if (state.known && state.value != D3D12_RESOURCE_STATE_RENDER_TARGET)
        return native(5, {u(state.known), u(state.value)});
    std::array<ComPtr<ID3D12Resource>, 2> textures;
    std::array<ComPtr<ID3D12Resource>, 5> buffers;
    std::array<unsigned, 28> screen{};
    std::array<unsigned, 5> material{};
    const bool bound = silhouette_bindings(r, screen, material, textures);
    if (!bound || !silhouette_material(material) || screen[4] != std::bit_cast<unsigned>(float(width)) ||
        screen[5] != std::bit_cast<unsigned>(float(height)) || screen[6] != std::bit_cast<unsigned>(1.F / width) ||
        screen[7] != std::bit_cast<unsigned>(1.F / height) || screen[10] != render_w || screen[11] != render_h ||
        screen[12] || screen[13] || screen[14] != std::bit_cast<unsigned>(float(render_w)) ||
        screen[15] != std::bit_cast<unsigned>(float(render_h)) ||
        screen[16] != std::bit_cast<unsigned>(1.F / float(render_w)) ||
        screen[17] != std::bit_cast<unsigned>(1.F / float(render_h)))
        return native(6,
                      {u(bound), material[2], material[3], material[4], screen[4], screen[5], screen[6], screen[7],
                       screen[10], screen[11], screen[12], screen[13], screen[14], screen[15], screen[16], screen[17]});
    for (unsigned i = 0; i < 2; ++i) {
        if (!textures[i]) continue;
        const auto d = textures[i]->GetDesc();
        if (d.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D || d.Width != width || d.Height != height ||
            d.DepthOrArraySize != 1 || d.MipLevels != 1 || d.SampleDesc.Count != 1 || d.SampleDesc.Quality ||
            d.Format != (i ? DXGI_FORMAT_R8G8B8A8_TYPELESS : DXGI_FORMAT_R32_FLOAT))
            return native(7, {i, u(d.Dimension), u(d.Width), u(d.Height), u(d.DepthOrArraySize), u(d.MipLevels),
                              u(d.SampleDesc.Count), u(d.Format)});
    }
    std::array<std::uint64_t, 5> seen{};
    if (const auto why = silhouette_buffers(r, raster, buffers, seen))
        return native(8,
                      {why, raster.vertex_mask, u(raster.index_buffer.Format), u(raster.topology),
                       raster.vertex_buffer.StrideInBytes, u(seen[0]), u(seen[1]), u(seen[2]), u(seen[3]), u(seen[4])});
    if (!layout(r.recipe->layout, true)) return native(8, {100});
    if (!silhouette_code_stable(*r.recipe)) return native(8, {101});
    for (auto ptr : {root, pipeline}) {
        std::uintptr_t method{};
        for (unsigned slot : {0U, 2U, 7U})
            if (!pb::system_method(ptr, slot, method)) return native(9);
    }
    auto *cache = silhouette_pipeline(r.recipe, reinterpret_cast<ID3D12RootSignature *>(root));
    if (!cache) return native(10);
    std::array<IUnknown *, 12> pins{reinterpret_cast<IUnknown *>(pipeline), reinterpret_cast<IUnknown *>(root),
                                    textures[1].Get(), cache->draw->pipeline()};
    unsigned size = 4;
    for (const auto &buffer : buffers)
        if (buffer) pins[size++] = buffer.Get();
    if (textures[0]) pins[size++] = textures[0].Get();
    for (auto heap : roots.heaps)
        if (heap) pins[size++] = reinterpret_cast<IUnknown *>(heap);
    if (!retain_recording(p, list, roots.generation, std::span(pins).first(size)))
        throw std::runtime_error("silhouette draw pins");
    injecting = true;
    {
        PredicateScope predicate(list);
        predicate.native(p.selector.Get());
        list->DrawIndexedInstanced(n, instances, first, base, instance);
        predicate.full(p.selector.Get());
        scope.replaced =
            cache->draw->record(list, reinterpret_cast<ID3D12PipelineState *>(pipeline), v, sc, n, first, base);
    }
    injecting = false;
    obs::RasterBindings after;
    if (!scope.replaced || !obs::matches_current(list, roots) || !obs::raster_current(list, after) ||
        after.vertex_mask != raster.vertex_mask || after.index_known != raster.index_known ||
        std::memcmp(after.vertex_buffers.data(), raster.vertex_buffers.data(), sizeof(raster.vertex_buffers)) ||
        std::memcmp(&after.index_buffer, &raster.index_buffer, sizeof(raster.index_buffer)) ||
        std::memcmp(&after.viewport, &v, sizeof(v)) || std::memcmp(&after.scissor, &sc, sizeof(sc)) ||
        after.render_target != raster.render_target || after.topology != raster.topology)
        throw std::runtime_error("silhouette state restore");
    ++silhouette_corrected;
    ++silhouette_restored;
    completed(p);
    return true;
}
void silhouette_after(bool returned) noexcept {
    if (indexed_scope.slot == UINT_MAX) return;
    Lock lock;
    if (!returned || indexed_scope.calls != 1) fail(14);
}
void silhouette_summary(std::ostream &o) {
    o << "{\"recorded\":" << silhouette_recorded << ",\"native\":" << silhouette_native
      << ",\"corrected\":" << silhouette_corrected << ",\"pipelines\":" << silhouette_cache_count
      << ",\"allowlist\":" << known_silhouette_shader_count() << ",\"rejections\":";
    indexed_array(o, silhouette_rejections);
    o << ",\"unknown_draws\":" << silhouette_unknown_draws << ",\"unknown_programs\":" << silhouette_unknown_count
      << ",\"cache_full\":" << silhouette_cache_full << ",\"unknown_ps_sha256\":";
    if (silhouette_unknown_count) {
        static const char *d = "0123456789abcdef";
        o << '"';
        for (auto b : silhouette_unknown_hash) o << d[b >> 4] << d[b & 15];
        o << '"';
    } else
        o << "null";
    const auto hex = [&](const ShaderHash &h) {
        static const char *d = "0123456789abcdef";
        o << '"';
        for (auto b : h) o << d[b >> 4] << d[b & 15];
        o << '"';
    };
    o << ",\"recipe_failed\":" << silhouette_recipe_failed << ",\"unknown\":[";
    for (unsigned i = 0; i < silhouette_unknown_count; ++i) {
        const auto &x = silhouette_unknown[i];
        if (i) o << ',';
        o << "{\"ps\":";
        hex(x.hash);
        o << ",\"draws\":" << x.draws << '}';
    }
    o << "],\"samples\":[";
    for (unsigned i = 0; i < silhouette_sample_count; ++i) {
        const auto &s = silhouette_samples[i];
        if (i) o << ',';
        o << "{\"reason\":" << s.reason << ",\"draws\":" << s.draws << ",\"vs\":";
        if (s.recipe)
            hex(s.recipe->vs_hash);
        else
            o << "null";
        o << ",\"ps\":";
        if (s.recipe)
            hex(s.recipe->ps_hash);
        else
            o << "null";
        o << ",\"detail\":[";
        for (unsigned j = 0; j < s.detail_count; ++j) o << (j ? "," : "") << s.detail[j];
        o << "]}";
    }
    o << "],\"samples_dropped\":" << silhouette_samples_dropped << '}';
}
void silhouette_write(std::ostream &o) {
    o << "{\"schema\":1,\"policy\":\"indexed-native-coordinate-adapter-v1\",\"planned\":" << silhouette_planned
      << ",\"recorded\":" << silhouette_recorded << ",\"native\":" << silhouette_native
      << ",\"corrected\":" << silhouette_corrected << ",\"restored\":" << silhouette_restored
      << ",\"pipelines\":" << silhouette_cache_count << ",\"rejections\":";
    indexed_array(o, silhouette_rejections);
    o << '}';
}
