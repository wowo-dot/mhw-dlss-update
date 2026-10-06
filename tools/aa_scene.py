"""Bounded CPU metadata for independent-AA research. No COM calls or writes.

The supplied reader must validate pages, process identity and live code first.
These observations establish neither GPU contents nor resource lifetime.
"""
VTABLES = {0x351BDA8: "taa2", 0x351C1A8: "fxaa", 0x354DD78: "taa1", 0x354DF48: "taa3"}


def decode_engine_dimensions(word):
    """Decode owner+0x88 metadata; high two bits are flags, not height.

    Packing verified against host RVAs 0x239d650 and 0x239da50. This is
    engine metadata, not an ID3D12Resource::GetDesc result.
    """
    return dict(dimensions_word=word, width=word & 0xFFFF,
                height=(word >> 16) & 0x3FFF, dimension_flags=word >> 30)


def capture(reader):
    v, base = reader.value, reader.base

    def owner(address):
        if not address:
            return None
        fields = ((0, "Q"), (0x60, "Q"), (0xF0, "Q"), (0x88, "I"),
                  (0x90, "I"), (0x94, "I"))
        values = [v(address + offset, fmt) for offset, fmt in fields]
        backing = values[2] or values[1]
        resource = v(backing + 0x20, "Q") if backing else 0
        again = [v(address + offset, fmt) for offset, fmt in fields]
        stable = values == again and (not backing or v(backing + 0x20, "Q") == resource)
        return dict(object=address, type=values[0], backing=backing, resource=resource,
                    **decode_engine_dimensions(values[3]),
                    engine_format=values[4], gpu_format=values[5], fields_stable=stable)

    def binding(slot):
        pointer = v(slot, "Q")
        result = owner(pointer)
        if v(slot, "Q") != pointer:
            raise ValueError("resource binding changed during observation")
        return result

    registry, renderer, table = [v(base + rva, "Q") for rva in (0x51238C8, 0x51C4480, 0x51C4368)]
    if not all((registry, renderer, table)):
        raise ValueError("renderer registry/table unavailable")
    head = v(registry + 0x15D0, "Q")
    view = v(renderer + 0x1F450, "Q")
    if not view:
        raise ValueError("primary view unavailable")
    frame, epoch = v(renderer + 0x1F210, "Q"), v(renderer + 0x1F208, "Q")
    choices = [v(table + offset, "B") for offset in (0xE9E4, 0xE9E5)]
    if any(x not in (0, 1) for x in choices):
        raise ValueError("invalid native AA table bytes")
    backend = v(renderer + 0x78, "Q")
    api = v(base + 0x51CC080, "B")
    if api != 1 or not backend:
        raise ValueError("DX12 backend unavailable")
    nodes, layers, seen = [], [], set()
    node = head
    while node:
        if len(nodes) >= 512 or node in seen:
            raise ValueError("layer list bound/cycle")
        seen.add(node)
        vt, following = v(node, "Q"), v(node + 0x30, "Q")
        nodes.append((node, vt, following))
        if vt - base in VTABLES:
            if len(layers) >= 16:
                raise ValueError("AA layer match budget")
            kind, flags = v(node + 0xC, "I"), v(node + 0x14, "I")
            layer = dict(name=VTABLES[vt-base], object=node, vtable_rva=vt-base,
                         kind=kind, flags=flags, general_enable=bool(flags & 2))
            if layer["name"] == "taa2":
                layer["taa_bytes"] = v(node + 0x184, "I")
                layer["resources"] = {name: binding(node + offset) for name, offset in
                    (("input", 0x150), ("destination", 0x158), ("ring0", 0x168), ("ring1", 0x170), ("ring2", 0x178))}
            layer["flags_stable"] = (kind, flags) == (v(node + 0xC, "I"), v(node + 0x14, "I"))
            layers.append(layer)
        node = following
    attachment, gbuffer, depth = [v(view + offset, "Q") for offset in (0x420, 0x620, 0x628)]
    scene = dict(color=binding(attachment + 0x58) if attachment else None,
                 motion=binding(gbuffer + 0x78) if gbuffer else None,
                 depth=binding(depth + 0xA0) if depth else None)
    frame_after, epoch_after = v(renderer + 0x1F210, "Q"), v(renderer + 0x1F208, "Q")
    links_stable = all(v(p, "Q") == vt and v(p + 0x30, "Q") == nxt for p, vt, nxt in nodes)
    anchors_stable = (
        [v(base + rva, "Q") for rva in (0x51238C8, 0x51C4480, 0x51C4368)] == [registry, renderer, table]
        and v(registry + 0x15D0, "Q") == head and v(renderer + 0x1F450, "Q") == view
        and [v(view + offset, "Q") for offset in (0x420, 0x620, 0x628)] == [attachment, gbuffer, depth]
        and [v(table + offset, "B") for offset in (0xE9E4, 0xE9E5)] == choices)
    return dict(schema=2, graphics_api="D3D12", renderer=renderer, primary_view=view,
                device=v(backend + 0x18, "Q"), swapchain=v(backend + 0x1470, "Q"),
                adapter=None, swapchain_format=None, com_metadata="not queried by external scalar reader",
                frame=frame, epoch=epoch, frame_after=frame_after, epoch_after=epoch_after,
                frame_unchanged=(frame, epoch) == (frame_after, epoch_after),
                table_taa=choices[0], table_fxaa=choices[1], list_nodes=len(nodes),
                links_stable=links_stable, anchors_stable=anchors_stable,
                layers=layers, scene=scene, gpu_execution_verified=False, ownership_proven=False)
