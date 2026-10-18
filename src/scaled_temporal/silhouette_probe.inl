// Part of stream_route.cpp. The late packets a plan replays
// (silhouette_candidates) and the sampled indexed draws for the log. Serialized
// by the stream route; every native draw forwards unchanged, and nothing here
// edits a resource, pipeline, packet, descriptor or GPU command (the replay is
// silhouette_route.inl).
struct IndexedScope {
    unsigned sample = UINT_MAX, slot = UINT_MAX, position = UINT_MAX, calls{};
    std::uintptr_t state{};
    pb::DrawSite draw;
    bool replaced{};
};
thread_local IndexedScope indexed_scope;
struct IndexedSample {
    std::uint64_t frame{}, epoch{}, tick{};
    unsigned key{}, technique{}, rejection{}, calls{};
    bool returned{}, stable{}, api_seen{}, root_ready{}, raster_ready{};
    std::array<unsigned, 3> packet_arguments{};
    std::array<std::uintptr_t, 4> packet_bindings{};
    std::array<std::int64_t, 5> arguments{};
    std::uintptr_t caller{}, state{};
    std::string pixel_hash;
    std::array<std::uintptr_t, 3> extra_stages{};
    pb::AttachmentSnapshot attachments;
    obs::RasterBindings raster;
    temporal_present::Bindings roots;
    // color, depth, mask, four VBs, IB: candidate,valid,dimension,width,height,
    // array,mips,format,samples,quality,layout,flags,alignment,buffer GPU address.
    std::array<std::array<std::uint64_t, 14>, 8> descriptions{};
    std::array<std::uint64_t, 12> binding_owners{}; // b1,b5,t0,t18: ID/mode/object
    std::array<unsigned, 28> screen{};
    std::array<unsigned, 5> material{};
    std::array<std::uint64_t, 2> color_state{};
};
std::array<IndexedSample, 16> indexed_samples;
unsigned indexed_sample_count{}, indexed_visits{}, indexed_errors{}, indexed_candidates_rejected{};
std::array<std::array<std::uintptr_t, 2>, 32> indexed_seen_sets{};
unsigned indexed_seen_count{};
std::array<std::uint64_t, 14> indexed_description(std::uintptr_t candidate) noexcept {
    std::array<std::uint64_t, 14> result{};
    result[0] = candidate;
    if (!candidate) return result;
    std::uintptr_t method{};
    for (unsigned i : {0U, 2U, 7U, 10U, 11U})
        if (!pb::system_method(candidate, i, method)) return result;
    ID3D12Resource *held{};
    ID3D12Device *device{};
    __try {
        auto *r = reinterpret_cast<ID3D12Resource *>(candidate);
        if (SUCCEEDED(r->QueryInterface(IID_PPV_ARGS(&held))) && held == r &&
            SUCCEEDED(held->GetDevice(IID_PPV_ARGS(&device))) &&
            reinterpret_cast<std::uintptr_t>(device) == cfg.device) {
            const auto d = held->GetDesc();
            result = {candidate,
                      1,
                      unsigned(d.Dimension),
                      d.Width,
                      d.Height,
                      d.DepthOrArraySize,
                      d.MipLevels,
                      unsigned(d.Format),
                      d.SampleDesc.Count,
                      d.SampleDesc.Quality,
                      unsigned(d.Layout),
                      unsigned(d.Flags),
                      d.Alignment,
                      d.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER ? held->GetGPUVirtualAddress() : 0};
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        result[1] = 0;
    }
    if (device) device->Release();
    if (held) held->Release();
    return result;
}
// Why a built plan's late indexed packets were not frozen (the frame then stays
// native): 0 queue read, 1 packet read, 2 over capacity, 3 duplicate packet,
// 4 packet fields, 5 queue changed while scanning. And the most late indexed
// packets seen in one frame, including frames over capacity.
std::array<std::uint64_t, 6> indexed_candidate_reasons{};
unsigned indexed_candidates_max{};
bool silhouette_candidates(Plan &p) {
    struct Packet {
        unsigned key, padding;
        std::uintptr_t object;
    };
    unsigned count{};
    std::uintptr_t array{};
    std::array<Packet, 128> block{};
    const auto reject = [&](unsigned why) {
        p.late_count = 0;
        ++indexed_candidates_rejected;
        ++indexed_candidate_reasons[why];
        return false;
    };
    if (!field(p.stamp.context, layout::context::packet_count, count) || count > 65535 ||
        !field(p.stamp.context, layout::context::packets, array) || !array || array > UINTPTR_MAX - 16ULL * count)
        return reject(0);
    std::array<Packet, Plan::late_capacity> selected{};
    unsigned size{}, seen{};
    for (unsigned base = 0; base < count; base += 128) {
        const auto n = (std::min)(128U, count - base);
        if (!pb::copy_memory(array + 16ULL * base, block.data(), n * sizeof(Packet))) return reject(0);
        for (unsigned j = 0; j < n; ++j) {
            const auto &q = block[j];
            if (q.key <= 0x7ffffff1 || q.key >= 0x80000000) continue;
            unsigned op{};
            if (!field(q.object, 0, op)) return reject(1);
            if ((op & 31) != 1) continue;
            if (seen++ < selected.size()) selected[size++] = q;
        }
    }
    indexed_candidates_max = (std::max)(indexed_candidates_max, seen);
    if (seen > selected.size()) return reject(2);
    // The native merge sort is stable for equal keys. Preserve insertion order.
    std::stable_sort(selected.begin(), selected.begin() + size,
                     [](const auto &a, const auto &b) { return a.key < b.key; });
    for (unsigned i = 0; i < size; ++i) {
        const auto &q = selected[i];
        auto &r = p.late[i];
        for (unsigned j = 0; j < i; ++j)
            if (selected[j].object == q.object) return reject(3);
        if (!field(q.object, layout::draw_packet::bindings, r.bindings) ||
            !field(q.object, layout::draw_packet::arguments, r.arguments))
            return reject(4);
        p.late_packets[i] = q.object;
        p.late_keys[i] = q.key;
        r.silent = false;
        r.recipe = silhouette_cpu(q.object, r.silent);
        if (r.recipe && !pb::snapshot_native_pipeline(cfg.image, q.object, r.native)) r.recipe.reset();
    }
    unsigned again{};
    std::uintptr_t pointer{};
    if (!field(p.stamp.context, layout::context::packet_count, again) || again != count ||
        !field(p.stamp.context, layout::context::packets, pointer) || pointer != array || !same_stamp(p.stamp))
        return reject(5);
    p.late_count = size;
    return true;
}
IndexedScope indexed_diagnostic_before(const pb::Frame &f) noexcept {
    IndexedScope scope;
    try {
        Lock lock;
        if (!accepting.load() || fault.load() || abandoned || indexed_sample_count == indexed_samples.size() ||
            indexed_visits >= 4096)
            return scope;
        unsigned plan = UINT_MAX, position{};
        for (unsigned i = 0; i < plans.size(); ++i) {
            const auto &p = plans[i];
            if (!p.built) continue;
            for (unsigned j = 0; j < p.late_count; ++j)
                if (!(p.late_executed & (1ULL << j)) && p.late_packets[j] == f.gpr[3]) {
                    if (plan != UINT_MAX) {
                        ++indexed_errors;
                        return scope;
                    }
                    plan = i;
                    position = j;
                }
        }
        if (plan == UINT_MAX) return scope;
        ++indexed_visits;
        auto &p = plans[plan];
        auto &s = indexed_samples[indexed_sample_count];
        std::destroy_at(&s);
        std::construct_at(&s);
        std::uintptr_t handle{}, set{}, entry{}, descriptor{}, data{};
        unsigned size{};
        if (!field(f.gpr[3], layout::draw_packet::bindings, s.packet_bindings) ||
            !field(f.gpr[3], layout::draw_packet::arguments, s.packet_arguments) ||
            !field(s.packet_bindings[2], layout::binding_state::shader, handle) || !field(handle, 0, set) ||
            !field(set, layout::shader_set::pixel_stage, entry) ||
            !field(entry, layout::shader_stage::code, descriptor) ||
            !field(descriptor, layout::shader_code::size, size) || size < 52 || size > 131072 ||
            !field(descriptor, layout::shader_code::bytecode, data))
            return scope;
        unsigned seen = UINT_MAX;
        for (unsigned i = 0; i < indexed_seen_count; ++i)
            if (indexed_seen_sets[i][0] == set) seen = i;
        if (seen != UINT_MAX && indexed_seen_sets[seen][1] >= 2) return scope;
        std::vector<std::byte> bytes(size);
        if (!pb::copy_memory(data, bytes.data(), size) || !known_silhouette_shader(bytes)) return scope;
        if (seen == UINT_MAX) {
            if (indexed_seen_count == indexed_seen_sets.size()) return scope;
            seen = indexed_seen_count++;
            indexed_seen_sets[seen] = {set, 0};
        }
        ++indexed_seen_sets[seen][1];
        s.pixel_hash = pb::bytes_sha256(bytes.data(), size);
        s.frame = p.stamp.frame;
        s.epoch = p.stamp.epoch;
        s.tick = GetTickCount64();
        s.key = p.late_keys[position];
        unsigned short technique{};
        field(s.packet_bindings[2], layout::binding_state::technique, technique);
        s.technique = technique;
        for (unsigned i = 0; i < 3; ++i) field(set, layout::shader_set::extra_stages + 8 * i, s.extra_stages[i]);
        scope.sample = indexed_sample_count++;
        scope.draw.packet = f.gpr[3];
        scope.draw.executor = f.gpr[1];
        scope.draw.context = f.gpr[2];
        if (!field(f.gpr[1], layout::executor::recording, scope.state) ||
            !field(scope.state, layout::recording::command, scope.draw.command)) {
            s.rejection = 1;
            scope.sample = UINT_MAX;
            return scope;
        }
        s.state = scope.state;
        std::uintptr_t words{}, ids{}, modes{};
        unsigned owners{};
        std::array<unsigned short, 2> range{};
        if (field(set, layout::shader_set::words, words) && field(set, layout::shader_set::ids, ids) &&
            field(set, layout::shader_set::owner_count, owners) && owners <= 64 &&
            field(s.packet_bindings[2], layout::binding_state::modes, modes) &&
            field(set, layout::shader_set::range, range) && range[0] <= 128 && unsigned(range[0]) + range[1] <= 4096) {
            std::array<unsigned, 128> binding{};
            if (pb::copy_memory(words + 4ULL * range[1], binding.data(), 4 * range[0]))
                for (unsigned i = 0; i < range[0]; ++i) {
                    const auto w = binding[i], kind = w & 15, index = (w >> 4) & 255, reg = w >> 24;
                    if (index >= owners) continue;
                    unsigned id{};
                    unsigned char mode{};
                    std::uintptr_t object{};
                    if (!field(ids, index * 4, id) || !field(modes, index, mode) ||
                        !field(s.packet_bindings[2], layout::binding_state::objects + 8 * index, object))
                        continue;
                    if (kind == 0 && (reg == 1 || reg == 5)) {
                        const auto offset = reg == 1 ? 0 : 3;
                        s.binding_owners[offset] = id;
                        s.binding_owners[offset + 1] = mode;
                        s.binding_owners[offset + 2] = object;
                        if (mode == 0) {
                            if (reg == 1)
                                pb::copy_memory(object, s.screen.data(), sizeof(s.screen));
                            else
                                pb::copy_memory(object, s.material.data(), sizeof(s.material));
                        }
                    }
                    if (kind == 2 && (reg == 0 || reg == 18)) {
                        const auto offset = reg == 0 ? 6 : 9;
                        s.binding_owners[offset] = id;
                        s.binding_owners[offset + 1] = mode;
                        s.binding_owners[offset + 2] = object;
                        tb::Owner texture;
                        if (tb::read_owner(reader, object, texture))
                            s.descriptions[reg == 0 ? 1 : 2][0] = texture.resource;
                    }
                }
        }
    } catch (...) {
        Lock lock;
        ++indexed_errors;
        scope.sample = UINT_MAX;
    }
    return scope;
}
void indexed_sample(ID3D12GraphicsCommandList *list, unsigned n, unsigned instances, unsigned first, int base,
                    unsigned instance, std::uintptr_t caller) {
    auto &s = indexed_samples[indexed_scope.sample];
    ++s.calls;
    s.api_seen = true;
    s.arguments = {n, instances, first, base, instance};
    s.caller = caller;
    indexed_scope.draw.command = reinterpret_cast<std::uintptr_t>(list);
    s.root_ready = obs::inspect(list, s.roots) && s.roots.known && s.roots.valid;
    s.raster_ready = obs::raster_current(list, s.raster);
    if (caller != cfg.image + 0x259d806 || s.calls != 1 ||
        s.packet_arguments != std::array<unsigned, 3>{n, first, std::bit_cast<unsigned>(base)} || instances != 1 ||
        instance) {
        s.rejection = 2;
        return;
    }
    if (!pb::snapshot_attachments(cfg.image, indexed_scope.draw, s.attachments) ||
        !pb::attachment_cache_matches(s.attachments)) {
        s.rejection = 3;
        return;
    }
    s.descriptions[0] = indexed_description(s.attachments.views[0].resource);
    s.descriptions[1] = indexed_description(s.descriptions[1][0]);
    s.descriptions[2] = indexed_description(s.descriptions[2][0]);
    const auto color = recording_states.get(reinterpret_cast<std::uintptr_t>(list), s.roots.generation,
                                            s.attachments.views[0].resource);
    s.color_state = {color.known, color.value};
    const auto ia = s.packet_bindings[1];
    std::uintptr_t owner{}, backend{}, resource{};
    for (unsigned i = 0; i < 5; ++i) {
        const auto offset = i == 4 ? 0x38 : 8 * i;
        if (field(ia, offset, owner) && field(owner, layout::texture_owner::backing, backend) &&
            field(backend, layout::backing::resource, resource))
            s.descriptions[3 + i] = indexed_description(resource);
    }
}
void indexed_after(bool returned) noexcept {
    if (indexed_scope.sample == UINT_MAX) return;
    Lock lock;
    auto &s = indexed_samples[indexed_scope.sample];
    s.returned = returned;
    pb::AttachmentSnapshot now;
    s.stable = returned && s.api_seen && s.calls == 1 && pb::snapshot_attachments(cfg.image, indexed_scope.draw, now) &&
               pb::attachment_inputs_equal(s.attachments, now) && pb::attachment_cache_matches(now);
}
template <class T> void indexed_array(std::ostream &o, const T &v) {
    o << '[';
    bool comma = false;
    for (auto x : v) {
        if (comma) o << ',';
        o << +x;
        comma = true;
    }
    o << ']';
}
void indexed_write(std::ostream &o) {
    o << "{\"schema\":1,\"mode\":\"read-only-indexed-contract\",\"visits\":" << indexed_visits
      << ",\"errors\":" << indexed_errors << ",\"candidate_rejections\":" << indexed_candidates_rejected
      << ",\"sample_count\":" << indexed_sample_count << ",\"samples\":[";
    for (unsigned i = 0; i < indexed_sample_count; ++i) {
        const auto &s = indexed_samples[i];
        if (i) o << ',';
        o << "{\"frame\":" << s.frame << ",\"epoch\":" << s.epoch << ",\"tick\":" << s.tick << ",\"key\":" << s.key
          << ",\"technique\":" << s.technique << ",\"pixel_sha256\":" << std::quoted(s.pixel_hash)
          << ",\"rejection\":" << s.rejection << ",\"calls\":" << s.calls << ",\"returned\":" << s.returned
          << ",\"stable\":" << s.stable << ",\"api_seen\":" << s.api_seen << ",\"root_ready\":" << s.root_ready
          << ",\"raster_ready\":" << s.raster_ready << ",\"caller\":" << s.caller << ",\"state\":" << s.state;
        o << ",\"packet_arguments\":";
        indexed_array(o, s.packet_arguments);
        o << ",\"packet_bindings\":";
        indexed_array(o, s.packet_bindings);
        o << ",\"arguments\":";
        indexed_array(o, s.arguments);
        o << ",\"extra_stages\":";
        indexed_array(o, s.extra_stages);
        o << ",\"descriptions\":[";
        for (unsigned j = 0; j < s.descriptions.size(); ++j) {
            if (j) o << ',';
            indexed_array(o, s.descriptions[j]);
        }
        o << "],\"binding_owners\":";
        indexed_array(o, s.binding_owners);
        o << ",\"screen_words\":";
        indexed_array(o, s.screen);
        o << ",\"material_words\":";
        indexed_array(o, s.material);
        o << ",\"color_state\":";
        indexed_array(o, s.color_state);
        o << ",\"native_viewport\":";
        indexed_array(o, s.attachments.viewport);
        o << ",\"native_scissor\":";
        indexed_array(o, s.attachments.scissor);
        o << ",\"native_blend\":";
        indexed_array(o, s.attachments.pipeline.blend);
        o << ",\"native_depth\":";
        indexed_array(o, s.attachments.pipeline.depth);
        o << ",\"native_raster\":";
        indexed_array(o, s.attachments.pipeline.raster);
        const auto &r = s.raster;
        o << ",\"raster\":{\"generation\":" << r.generation << ",\"vertex_mask\":" << r.vertex_mask
          << ",\"index_known\":" << r.index_known << ",\"topology\":" << r.topology
          << ",\"output_known\":" << r.output_known << ",\"render_target\":" << r.render_target << ",\"viewport\": ["
          << r.viewport.TopLeftX << ',' << r.viewport.TopLeftY << ',' << r.viewport.Width << ',' << r.viewport.Height
          << ',' << r.viewport.MinDepth << ',' << r.viewport.MaxDepth << "],\"scissor\": [" << r.scissor.left << ','
          << r.scissor.top << ',' << r.scissor.right << ',' << r.scissor.bottom << "],\"index\": ["
          << r.index_buffer.BufferLocation << ',' << r.index_buffer.SizeInBytes << ',' << r.index_buffer.Format
          << "],\"vertices\":[";
        for (unsigned j = 0; j < 4; ++j) {
            if (j) o << ',';
            o << '[' << r.vertex_buffers[j].BufferLocation << ',' << r.vertex_buffers[j].SizeInBytes << ','
              << r.vertex_buffers[j].StrideInBytes << ']';
        }
        o << "]}";
        const auto &b = s.roots;
        o << ",\"roots\":{\"generation\":" << b.generation << ",\"pipeline\":" << b.pipeline
          << ",\"signature\":" << b.graphics.signature << ",\"heaps\":";
        indexed_array(o, b.heaps);
        o << ",\"arguments\":[";
        bool comma = false;
        for (unsigned j = 0; j < b.graphics.arguments.size(); ++j) {
            const auto &a = b.graphics.arguments[j];
            if (a.kind == temporal_present::ArgumentKind::unset) continue;
            if (comma) o << ',';
            comma = true;
            indexed_array(o, std::array<std::uint64_t, 4>{j, unsigned(a.kind), a.address, a.mask});
        }
        o << "]}}";
    }
    o << "]}";
}
