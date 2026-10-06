"""Bounded read-only native haze recipe capture for the verified MHW executable.

Double snapshots reject observed changes. They are CPU observations, not an
atomic process snapshot, GPU descriptor/state proof, or permission to render.
No remote allocation, code execution, suspension, or process-memory writes.
"""
import argparse
import ctypes as c
from ctypes import wintypes as w
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import struct
import time

from arm_continuous_dlaa import HOST, parse, require

HAZE_TYPE = 0x35372b0
HAZE_KEY = 0x78004e20
MAX_QUEUE = 16384
MAX_SHADER = 262144
GATES = ((0x24c7760, 2256, 'd1e1c91d7cd6ff65cf5fee677a998692fc4e9ead2bedcb7c0a79f88fbe752701'),
         (0x259e670, 2554, '088e67c77b7e96af64c83c8297f6fc9f6d355dbc170530dda125b41f4cb53727'),
         (0x259f330, 1664, 'ec12eca9df5bc5598eed515d9e882fad9d0afcf54c0488f9eaf98127ab046c32'))
NAMED_IDS = {'tFilterTempMap1-custom': 0x4beae84, 'tFilterTempMap1-default': 0x4beae88,
             'tVolumeMap': 0x4beae8c, 'tDepthMap': 0x4beae90, 'tBaseMap': 0x4beae94,
             'CBHazeFilter': 0x4beae98, 'CBScreen': 0x4beaeb8}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def program(data, shader_type):
    require(52 <= len(data) <= MAX_SHADER and data[:4] == b'DXBC', 'shader container')
    word = lambda at: struct.unpack_from('<I', data, at)[0]
    require(word(20) == 1 and word(24) == len(data), 'shader length')
    count = word(28)
    require(0 < count <= 64 and 32 + count * 4 <= len(data), 'shader chunks')
    spans, programs = [], 0
    for i in range(count):
        at = word(32 + 4*i)
        require(32 + 4*count <= at <= len(data)-8, 'chunk offset')
        size = word(at+4)
        require(size <= len(data)-at-8, 'chunk size')
        end = at+8+size
        require(all(end <= begin or finish <= at for begin, finish in spans), 'chunk overlap')
        spans.append((at, end))
        if data[at:at+4] in (b'SHEX', b'SHDR'):
            require(size >= 8 and size % 4 == 0 and word(at+8) == (shader_type << 16 | 0x50)
                    and word(at+12) == size//4, 'shader program')
            programs += 1
    require(programs == 1, 'one shader program')


class Memory:
    def scalar(self, address, fmt='Q'):
        return struct.unpack('<'+fmt, self.read(address, struct.calcsize('<'+fmt)))[0]

    def values(self, address, fmt):
        return list(struct.unpack('<'+fmt, self.read(address, struct.calcsize('<'+fmt))))

    def string(self, address):
        data = bytearray()
        for i in range(64):
            value = self.scalar(address+i, 'B')
            if value == 0:
                return data.decode('ascii')
            require(32 <= value < 127, 'semantic text')
            data.append(value)
        raise ValueError('semantic bound')


class MBI(c.Structure):
    _fields_ = [('BaseAddress', c.c_void_p), ('AllocationBase', c.c_void_p),
                ('AllocationProtect', w.DWORD), ('PartitionId', w.WORD),
                ('RegionSize', c.c_size_t), ('State', w.DWORD),
                ('Protect', w.DWORD), ('Type', w.DWORD)]


class LiveMemory(Memory):
    def __init__(self, pid):
        self.k = c.WinDLL('kernel32', use_last_error=True)
        self.k.OpenProcess.argtypes = [w.DWORD, w.BOOL, w.DWORD]
        self.k.OpenProcess.restype = w.HANDLE
        self.k.CloseHandle.argtypes = [w.HANDLE]
        self.k.ReadProcessMemory.argtypes = [w.HANDLE, c.c_void_p, c.c_void_p, c.c_size_t, c.POINTER(c.c_size_t)]
        self.k.VirtualQueryEx.argtypes = [w.HANDLE, c.c_void_p, c.POINTER(MBI), c.c_size_t]
        self.k.VirtualQueryEx.restype = c.c_size_t
        self.k.GetProcessTimes.argtypes = [w.HANDLE] + [c.POINTER(w.FILETIME)]*4
        self.k.QueryFullProcessImageNameW.argtypes = [w.HANDLE, w.DWORD, w.LPWSTR, c.POINTER(w.DWORD)]
        self.k.GetTickCount64.restype = c.c_uint64
        self.handle = self.k.OpenProcess(0x410, False, pid)
        require(self.handle, 'OpenProcess query/VM_READ')
        self.reads = self.bytes = 0

    def close(self):
        if self.handle:
            self.k.CloseHandle(self.handle)
            self.handle = None

    def read(self, address, size):
        require(type(address) is int and 0 < size <= MAX_SHADER and 0x10000 <= address <= 0x7fffffffffff-size, 'read bound')
        require(self.bytes+size <= 16*1048576 and self.reads < 8192, 'total read budget')
        self.bytes += size
        self.reads += 1
        parts, at = [], address
        while at < address+size:
            region = MBI()
            require(self.k.VirtualQueryEx(self.handle, at, c.byref(region), c.sizeof(region)) == c.sizeof(region), 'mapped region')
            require(region.State == 0x1000 and not region.Protect & 0x101
                    and region.Protect & 255 in (2, 4, 8, 0x20, 0x40, 0x80), 'readable region')
            n = min(address+size-at, region.BaseAddress+region.RegionSize-at, 65536)
            require(n > 0, 'region span')
            buf, got = c.create_string_buffer(n), c.c_size_t()
            require(self.k.ReadProcessMemory(self.handle, at, buf, n, c.byref(got)) and got.value == n, 'ReadProcessMemory')
            parts.append(buf.raw)
            at += n
        return b''.join(parts)

    def identify(self, header):
        times = [w.FILETIME() for _ in range(4)]
        require(self.k.GetProcessTimes(self.handle, *map(c.byref, times)), 'process times')
        creation = times[0].dwHighDateTime << 32 | times[0].dwLowDateTime
        require(creation == header['creation'] and not (times[1].dwHighDateTime or times[1].dwLowDateTime), 'live process generation')
        name = c.create_unicode_buffer(32768)
        size = w.DWORD(len(name))
        require(self.k.QueryFullProcessImageNameW(self.handle, 0, name, c.byref(size)), 'host path')
        require(Path(name.value).name.lower() == 'monsterhunterworld.exe' and sha(Path(name.value).read_bytes()) == HOST, 'host hash')
        ntdll = c.WinDLL('ntdll')
        ntdll.NtQueryInformationProcess.argtypes = [w.HANDLE, w.ULONG, c.c_void_p, w.ULONG, c.POINTER(w.ULONG)]
        info, returned = c.create_string_buffer(48), w.ULONG()
        require(ntdll.NtQueryInformationProcess(self.handle, 0, info, 48, c.byref(returned)) == 0, 'process basic information')
        base = self.scalar(int.from_bytes(info.raw[8:16], 'little')+0x10)
        require(base == header['base'], 'mapped base')
        nt_offset = self.scalar(base+0x3c, 'I')
        require(nt_offset <= 4096, 'PE header bound')
        nt = base+nt_offset
        require(self.scalar(base, 'H') == 0x5a4d and self.scalar(nt, 'I') == 0x4550
                and self.scalar(nt+8, 'I') == 0x66d13cb6 and self.scalar(nt+80, 'I') == 0x55d3000, 'mapped image identity')
        for rva, length, expected in GATES:
            require(sha(self.read(base+rva, length)) == expected, 'native code gate '+hex(rva))
        return base


def stamp(m, base):
    r = m.scalar(base+0x51c4480)
    context = r+0x710
    view = m.scalar(context+0x1d0)
    require(view and view == m.scalar(r+0x1f450), 'primary view')
    require(m.scalar(base+0x51cc080, 'B') != 0, 'DX12')
    result = dict(renderer=r, context=context, view=view, frame=m.scalar(r+0x1f210),
                  epoch=m.scalar(r+0x1f208), bank=m.scalar(r+0x1f200, 'I'), asynchronous=m.scalar(r+0x93, 'B'))
    require(result['frame'] and result['epoch'] and result['bank'] <= 1 and result['asynchronous'] <= 1, 'frame stamp')
    return result


def owner(m, at):
    primary, secondary = m.scalar(at+0x60), m.scalar(at+0xf0)
    dimensions = m.scalar(at+0x88, 'I')
    result = dict(owner=at, type=m.scalar(at), primary=primary, secondary=secondary,
                  kind=m.scalar(at+0x58, 'I'), flags=m.scalar(at+0x70, 'I'),
                  width=dimensions & 65535, height=(dimensions >> 16) & 16383,
                  format=m.scalar(at+0x90, 'I'), gpu_format=m.scalar(at+0x94, 'I'),
                  resource=m.scalar((secondary or primary)+0x20))
    require(result['width'] and result['height'] and result['resource'], 'texture owner metadata')
    return result


def read_shader(m, shader_set, offset, kind):
    entry = m.scalar(shader_set+offset)
    descriptor = m.scalar(entry+0x10)
    size, data = m.scalar(descriptor, 'I'), m.scalar(descriptor+8)
    require(52 <= size <= MAX_SHADER, 'shader byte bound')
    blob = m.read(data, size)
    program(blob, kind)
    return dict(entry=entry, descriptor=descriptor, data=data, size=size, sha256=sha(blob)), blob


def snapshot(m, base):
    at = stamp(m, base)
    manager = m.scalar(base+0x51cc238)
    count = m.scalar(manager+0x3c, 'I')
    require(0 < count <= 16, 'filter count')
    filters, selected = [], []
    for entry in m.values(manager+0x40, str(count)+'Q'):
        stage, order, unit, src, dst = m.values(entry, 'IIQQQ')
        ty = m.scalar(unit) if unit else 0
        f = dict(entry=entry, stage=stage, order=order, unit=unit, type=ty, source=src, destination=dst)
        filters.append(f)
        if stage == 15 and order == 20000 and ty == base+HAZE_TYPE:
            require(entry == unit+0x140 and src and dst and src != dst, 'haze descriptor')
            selected.append(f)
    require(len(selected) == 1, 'one active haze descriptor')
    haze = selected[0]
    src, dst = owner(m, haze['source']), owner(m, haze['destination'])
    queue = m.scalar(at['context']+0x2b78)
    commands = m.scalar(at['context']+0x2b80, 'I')
    require(0 < commands <= MAX_QUEUE, 'queue bound')
    commands_raw = m.read(queue, commands*16)
    candidates = []
    for index, (key, padding, packet) in enumerate(struct.iter_unpack('<IIQ', commands_raw)):
        if key != HAZE_KEY:
            continue
        require(len(candidates) < 8, 'haze candidate bound')
        if m.scalar(packet, 'I') & 31 == 0:
            candidates.append((index, packet))
    require(len(candidates) == 1, 'one haze draw packet')
    index, packet = candidates[0]
    packet_bytes = m.read(packet, 0x48)
    attachment, ia, graphics, render = struct.unpack_from('<QQQQ', packet_bytes, 0x10)
    vertex_count, first = struct.unpack_from('<II', packet_bytes, 0x40)
    require(vertex_count in (3, 4) and first <= 1048576, 'vertex arguments')
    require(m.scalar(attachment+0x48, 'I') & 7 == 1 and m.scalar(attachment+0x40) == 0, 'one color no depth attachment')
    view = m.scalar(attachment)
    backend = m.scalar(view+0x60)
    require(m.scalar(backend+0x18) == dst['resource'] and m.scalar(backend+0x28, 'B') == 0, 'haze output attachment')
    rectangle = m.values(attachment+0x4c, '4i')
    require(0 <= rectangle[0] < rectangle[2] <= 8192 and 0 <= rectangle[1] < rectangle[3] <= 8192, 'draw rectangle')
    stride, offset = m.scalar(ia+0x20, 'B'), m.scalar(ia+0x24, 'I')
    require(stride == 16 and offset <= 16777216, 'native vertex stride/offset')
    vertex_arena = m.scalar(at['context']+0x2b50)
    vertices = m.values(vertex_arena+offset+16*first, str(vertex_count*4)+'I')
    handle = m.scalar(graphics+8)
    shader_set = m.scalar(handle)
    shaders, blobs = {}, {}
    for name, off, kind in (('vs', 0x50, 1), ('ps', 0x58, 0)):
        shaders[name], blobs[name] = read_shader(m, shader_set, off, kind)
    require(all(m.scalar(shader_set+off) == 0 for off in (0x60, 0x68, 0x70)), 'VS/PS only')
    layout = m.scalar(graphics+0x20)
    elements, n = m.scalar(layout+0x10), m.scalar(layout+0x18, 'I')
    require(0 < n <= 32, 'input layout bound')
    inputs = []
    for i in range(n):
        text_pointer, *fields = m.values(elements+32*i, 'Q6I')
        inputs.append(dict(semantic=m.string(text_pointer), fields=fields))
    # Native graphics and compute recipes share the owner table. Keep packed
    # range words as evidence; do not infer live GPU descriptors from them.
    n = m.scalar(shader_set+0x88, 'I')
    require(0 < n <= 64, 'shader owner count')
    ids, modes = m.scalar(shader_set+0x80), m.scalar(graphics+0x10)
    owners = [dict(id=a, mode=b, object=d) for a, b, d in zip(
        m.values(ids, str(n)+'I'), m.values(modes, str(n)+'B'), m.values(graphics+0x30, str(n)+'Q'))]
    named_ids = {name: m.scalar(base+rva, 'I') for name, rva in NAMED_IDS.items()}
    for item in owners:
        item['names'] = [name for name, value in named_ids.items()
                         if value != 0xffffffff and value & 0xfff == item['id'] & 0xfff]
    words_pointer = m.scalar(shader_set)
    ranges = {}
    for name, start in (('vs', 8), ('ps', 0x14)):
        count, first_word = m.values(shader_set+start, '2H')
        metadata = m.values(shader_set+start+4, '8B')
        require(count <= 128 and (not count or first_word+count <= 4096), 'shader binding range bound')
        words = m.values(words_pointer+4*first_word, str(count)+'I') if count else []
        ranges[name] = [dict(count=count, start=first_word, metadata=metadata, words=words)]
    texture_bindings = []
    # The reviewed per-stage binding loop uses the first count/start pair.
    # Type 2/3 are SRV/UAV; this records their source CPU descriptor handles.
    for stage, parts in ranges.items():
        for word in parts[0]['words']:
            kind, index = word & 15, (word >> 4) & 255
            require(kind <= 3 and (kind == 0 or index < len(owners)), 'binding owner index')
            if kind in (2, 3):
                item = owners[index]
                srv = m.scalar(item['object']+(0x78 if kind == 2 else 0x80))
                descriptor = m.scalar(srv+0x10)
                require(descriptor, 'source CPU descriptor')
                texture_bindings.append(dict(stage=stage, register=word >> 24, kind=kind,
                    owner_index=index, id=item['id'], names=item['names'], view=srv,
                    cpu_descriptor=descriptor, texture=owner(m, item['object'])))
    depth_owner = m.scalar(m.scalar(m.scalar(base+0x51c73d0)+0x2480))
    blend_owner, raster_owner = m.scalar(render+8), m.scalar(render+0x18)
    variant = m.scalar(render+4, 'B') & 1
    states = dict(depth=m.read(m.scalar(depth_owner+0x60)+0x10, 52).hex(),
                  blend=m.read(m.scalar(blend_owner+0x98)+0x10, 328).hex(),
                  raster=m.read(m.scalar(raster_owner+0x68+8*variant)+0x10, 44).hex())
    haze_parameters = dict(strength_bits=m.scalar(haze['unit']+0x1bc, 'I'),
                           flags=m.values(haze['unit']+0x1d0, '4B'))
    require(stamp(m, base) == at and m.scalar(at['context']+0x2b78) == queue
            and m.scalar(at['context']+0x2b80, 'I') == commands, 'snapshot frame/queue changed')
    return dict(stamp=at, manager=manager, filters=filters, haze=haze, source=src, destination=dst,
                queue=queue, commands=commands, queue_sha256=sha(commands_raw), command_index=index,
                packet=packet, packet_bytes=packet_bytes.hex(), rectangle=rectangle, vertices_bits=vertices,
                vertex_count=vertex_count, first_vertex=first, vertex_arena=vertex_arena, vertex_offset=offset,
                attachment=attachment, ia=ia, graphics=graphics, render=render, shader_set=shader_set,
                shaders=shaders, input_layout=inputs, owners=owners, named_ids=named_ids,
                texture_bindings=texture_bindings, binding_ranges=ranges, states=states,
                haze_parameters=haze_parameters), blobs


def stable_snapshot(m, base):
    a, blobs_a = snapshot(m, base)
    b, blobs_b = snapshot(m, base)
    require(a == b and blobs_a == blobs_b, 'double snapshot changed')
    return a, blobs_a


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--log', required=True, type=Path)
    ap.add_argument('--output-dir', required=True, type=Path)
    ap.add_argument('--attempts', type=int, default=16)
    args = ap.parse_args()
    require(1 <= args.attempts <= 32 and not args.output_dir.exists(), 'attempt/output bound')
    require(args.log.stat().st_size <= 16*1048576, 'input log bound')
    header = parse(args.log.read_bytes())[0]
    require(header['version'] == '1.25.7' and header['experiment'] == 'continuous-scaled-native-resolve', 'tested capture version')
    args.output_dir.mkdir(parents=True)
    m = None
    result = dict(schema=2, policy='bounded-haze-cpu-double-snapshot-v2', pid=header['pid'], creation=header['creation'],
                  host_sha256=header['host_sha256'], dll_sha256=header['dll_sha256'],
                  observed_utc=datetime.now(timezone.utc).isoformat(), accepted=False, attempts=[],
                  process_memory_writes=0, process_suspended=False, gpu_state_verified=False, gpu_descriptors_verified=False,
                  native_code_gates=[dict(rva=hex(a), bytes=b, sha256=d) for a, b, d in GATES])
    try:
        m = LiveMemory(header['pid'])
        base = m.identify(header)
        for i in range(args.attempts):
            started = m.k.GetTickCount64()
            try:
                record, blobs = stable_snapshot(m, base)
                for name, blob in blobs.items():
                    with (args.output_dir/(name+'.dxbc')).open('xb') as f:
                        f.write(blob)
                result.update(accepted=True, record=record)
                result['attempts'].append(dict(index=i, begin_tick=started, end_tick=m.k.GetTickCount64(), accepted=True))
                break
            except (ValueError, OSError) as error:
                result['attempts'].append(dict(index=i, begin_tick=started, end_tick=m.k.GetTickCount64(), accepted=False, reason=str(error)))
                time.sleep(.02)
    except (ValueError, OSError) as error:
        result['failure'] = str(error)
    finally:
        if m:
            result.update(read_calls=m.reads, read_bytes=m.bytes)
            m.close()
        with (args.output_dir/'capture.json').open('x') as f:
            json.dump(result, f, indent=2)
    print(json.dumps({k: result[k] for k in ('accepted', 'pid', 'process_memory_writes')} | dict(attempts=len(result['attempts']))))
    return 0 if result['accepted'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
