"""Bounded DXBC and CPU binding-recipe review; no GPU-consumption inference."""
import hashlib
import struct

def require(ok, reason):
    if not ok:
        raise ValueError(reason)

def compute_dxbc(data):
    require(52 <= len(data) <= 262144 and data[:4] == b'DXBC', 'shader container')
    word = lambda at: struct.unpack_from('<I', data, at)[0]
    require(word(20) == 1 and word(24) == len(data), 'shader length')
    count = word(28)
    require(0 < count <= 64 and 32 + count * 4 <= len(data), 'shader chunks')
    spans, programs = [], 0
    for i in range(count):
        at = word(32 + i * 4)
        require(32 + 4 * count <= at <= len(data) - 8, 'chunk offset')
        size = word(at + 4)
        require(size <= len(data) - at - 8, 'chunk length')
        end = at + 8 + size
        require(all(end <= first or last <= at for first, last in spans), 'chunk overlap')
        spans.append((at, end))
        if data[at:at + 4] in (b'SHDR', b'SHEX'):
            programs += 1
            require(size >= 8 and size % 4 == 0 and word(at + 8) == 0x50050 and word(at + 12) == size // 4, 'compute program')
    require(programs == 1, 'one compute program')

def review_shader(capture, shader_path):
    require(capture.get('shader_saved') is True and capture.get('typed_views_verified') is False, 'shader closure')
    require(shader_path is not None and 52 <= shader_path.stat().st_size <= 262144, 'shader artifact')
    data = shader_path.read_bytes()
    require(len(data) == capture['shader_bytes'] and hashlib.sha256(data).hexdigest() == capture['shader_sha256'], 'shader artifact identity')
    compute_dxbc(data)
    mapping = []
    for r in capture['records']:
        s = r['shader']
        require(s['stable'] is True and s['technique'] == r['technique'] and s['bytes'] == len(data), 'shader request stability')
        require(all(type(s.get(k)) is int and 0 < s[k] < 2**64 for k in ('handle','set','entry','descriptor','bytecode','ids','modes','words')), 'shader pointers')
        owners, bindings = s['owners'], s['bindings']
        require(0 < len(owners) <= 64 and 0 < len(bindings) <= 128 and 0 <= s['binding_start'] <= 65535, 'shader recipe bounds')
        named = {v['id'] & 4095:v for v in r['inputs']}
        for owner in owners:
            require(0 <= owner['id'] < 0xffffffff and owner['owner'] > 0 and 0 <= owner['mode'] <= 255, 'shader owner')
            n = named.get(owner['id'] & 4095)
            require(n is None or n['owner'] == owner['owner'], 'named shader owner')
        frame_map = []
        for b in bindings:
            w = b['word']; require(type(w) is int and 0 <= w <= 0xffffffff, 'binding word')
            kind, index, slot = w & 15, (w >> 4) & 255, w >> 24
            require(kind <= 3 and (kind == 0 or index < len(owners)), 'binding instruction')
            if kind in (2,3):
                require(b['view'] > 0 and b['cpu_descriptor'] > 0, 'view metadata')
                owner = owners[index]; n = named.get(owner['id'] & 4095)
                frame_map.append(dict(kind='srv' if kind == 2 else 'uav', table_ordinal=slot, owner_index=index,
                    binding_id=owner['id'], name=n['name'] if n else None, owner=owner['owner']))
            else:
                require(b['view'] == 0 and b['cpu_descriptor'] == 0, 'non-texture view metadata')
        mapping.append(dict(frame=r['frame'], bindings=frame_map))
    return dict(shader_sha256=capture['shader_sha256'], shader_bytes=len(data), shader_model='cs_5_0',
                cpu_binding_recipes=mapping, typed_views_verified=False)
