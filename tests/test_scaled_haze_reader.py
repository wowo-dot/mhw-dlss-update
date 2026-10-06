"""CPU reader bounds and asynchronous-mutation rejection, without a game."""
import struct
import sys
import unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
import read_scaled_haze as h


def dxbc(kind):
    data = bytearray(52)
    data[:4] = b'DXBC'
    struct.pack_into('<4I', data, 20, 1, 52, 1, 36)
    data[36:40] = b'SHEX'
    struct.pack_into('<3I', data, 40, 8, kind << 16 | 0x50, 2)
    return bytes(data)


class Fake(h.Memory):
    def __init__(self):
        self.base, self.next = 0x140000000, 0x20000000
        self.data, self.changed, self.allocated = {}, False, []
        self.ranges = [(self.base, self.base+0x55d3000), (self.next, self.next+0x100000)]

    def alloc(self, size=0x100):
        address = self.next
        self.next += (size+15)&~15
        self.allocated.append(address)
        return address

    def write(self, address, data):
        self.data.update({address+i: v for i, v in enumerate(data)})

    def put(self, address, value, fmt='Q'):
        self.write(address, struct.pack('<'+fmt, value))

    def read(self, address, size):
        if not 0 < size <= h.MAX_SHADER or not any(start <= address and address+size <= end for start, end in self.ranges):
            raise ValueError('fake unmapped read')
        return bytes(self.data.get(address+i, 0) for i in range(size))


def fixture():
    m = Fake()
    b = m.base
    renderer, view = m.alloc(0x21000), m.alloc(0x1000)
    context = renderer+0x710
    m.put(b+0x51c4480, renderer)
    m.put(b+0x51c73d0, renderer)
    m.put(b+0x51cc080, 1, 'B')
    m.put(renderer+0x1f450, view)
    m.put(context+0x1d0, view)
    m.put(renderer+0x1f210, 111)
    m.put(renderer+0x1f208, 111)
    m.put(renderer+0x93, 1, 'B')
    unit, manager = m.alloc(0x200), m.alloc()
    m.put(unit, b+h.HAZE_TYPE)
    m.put(b+0x51cc238, manager)
    m.put(manager+0x3c, 1, 'I')
    m.put(manager+0x40, unit+0x140)
    owner_values = []
    for i in range(2):
        owner, backend, resource = m.alloc(), m.alloc(), m.alloc()
        m.put(owner, b+0x3537870)
        m.put(owner+0x60, backend)
        m.put(owner+0x88, 2560+(1440<<16), 'I')
        m.put(owner+0x90, 34, 'I')
        m.put(backend+0x20, resource)
        owner_values.append((owner, resource))
    m.write(unit+0x140, struct.pack('<IIQQQ', 15, 20000, unit, owner_values[0][0], owner_values[1][0]))
    queue, packet = m.alloc(), m.alloc()
    m.put(context+0x2b78, queue)
    m.put(context+0x2b80, 1, 'I')
    m.write(queue, struct.pack('<IIQ', h.HAZE_KEY, 0, packet))
    attachment, ia, graphics, render = (m.alloc(0x250) for _ in range(4))
    m.write(packet+0x10, struct.pack('<4Q', attachment, ia, graphics, render))
    m.write(packet+0x40, struct.pack('<2I', 3, 0))
    output_view, backend = m.alloc(), m.alloc()
    m.put(attachment, output_view)
    m.put(output_view+0x60, backend)
    m.put(backend+0x18, owner_values[1][1])
    m.put(attachment+0x48, 1, 'I')
    m.write(attachment+0x4c, struct.pack('<4i', 0, 0, 1280, 720))
    m.put(ia+0x20, 16, 'B')
    vertices = m.alloc()
    m.put(context+0x2b50, vertices)
    m.write(vertices, struct.pack('<12f', -1, 1, 0, 0, -1, -3, 0, 1, 3, 1, 1, 0))
    handle, shaders = m.alloc(), m.alloc()
    m.put(graphics+8, handle)
    m.put(handle, shaders)
    for off, kind in ((0x50, 1), (0x58, 0)):
        entry, desc, bytecode = m.alloc(), m.alloc(), m.alloc()
        m.put(shaders+off, entry)
        m.put(entry+0x10, desc)
        m.put(desc, 52, 'I')
        m.put(desc+8, bytecode)
        m.write(bytecode, dxbc(kind))
    layout, elements, text = m.alloc(), m.alloc(), m.alloc()
    m.put(graphics+0x20, layout)
    m.put(layout+0x10, elements)
    m.put(layout+0x18, 1, 'I')
    m.put(elements, text)
    m.write(text, b'POSITION\0')
    ids, modes, words = m.alloc(), m.alloc(), m.alloc()
    m.put(shaders+0x80, ids)
    m.put(shaders+0x88, 1, 'I')
    m.put(graphics+0x10, modes)
    m.put(graphics+0x30, owner_values[0][0])
    m.put(shaders, words)
    m.write(shaders+0x14, struct.pack('<2H', 1, 2))
    m.put(words+8, 2, 'I')
    srv = m.alloc()
    m.put(owner_values[0][0]+0x78, srv)
    m.put(srv+0x10, 0x12345000)
    depth_pointer, depth, blend, raster = (m.alloc(0x300) for _ in range(4))
    m.put(renderer+0x2480, depth_pointer)
    m.put(depth_pointer, depth)
    m.put(render+8, blend)
    m.put(render+0x18, raster)
    for address, offset in ((depth, 0x60), (blend, 0x98), (raster, 0x68)):
        state = m.alloc(0x200)
        m.put(address+offset, state)
    m.locations = dict(renderer=renderer, context=context, unit=unit, manager=manager,
                       queue=queue, packet=packet, backend=backend, shaders=shaders,
                       graphics=graphics, layout=layout, ia=ia, attachment=attachment)
    return m


class ReaderTests(unittest.TestCase):
    def test_double_snapshot(self):
        m = fixture()
        record, blobs = h.stable_snapshot(m, m.base)
        self.assertEqual(record['haze']['type']-m.base, h.HAZE_TYPE)
        self.assertEqual(record['rectangle'], [0, 0, 1280, 720])
        self.assertEqual(record['vertex_count'], 3)
        self.assertEqual(record['source']['width'], 2560)
        self.assertEqual(blobs['ps'], dxbc(0))
        self.assertEqual(record['binding_ranges']['ps'][0]['words'], [2])
        self.assertEqual(record['texture_bindings'][0]['cpu_descriptor'], 0x12345000)

    def test_malformed_recipes(self):
        cases = [('manager', 0x3c, 17, 'I'), ('context', 0x2b80, h.MAX_QUEUE+1, 'I'),
                 ('unit', 0, 123, 'Q'), ('packet', 0x40, 99999, 'I'), ('ia', 0x20, 32, 'B'),
                 ('backend', 0x18, 123, 'Q'), ('shaders', 0x88, 65, 'I'),
                 ('shaders', 0x60, 1, 'Q'), ('layout', 0x18, 33, 'I'),
                 ('attachment', 0x48, 2, 'I'), ('shaders', 0x14, 129, 'H'),
                 ('shaders', 0x16, 4096, 'H')]
        for name, offset, value, fmt in cases:
            with self.subTest(name=name, offset=offset):
                m = fixture()
                m.put(m.locations[name]+offset, value, fmt)
                with self.assertRaises(ValueError):
                    h.stable_snapshot(m, m.base)

    def test_stage_metadata_is_not_a_binding_range(self):
        m = fixture()
        shaders = m.locations['shaders']
        vs = bytes([0, 0, 0, 255, 255, 255, 1, 0])
        ps = bytes([4, 0, 4, 0, 255, 0, 0, 0])
        m.write(shaders+12, vs)
        m.write(shaders+0x18, ps)
        m.put(shaders+10, 65535, 'H')  # Empty VS range: no read at this start.
        record, _ = h.stable_snapshot(m, m.base)
        self.assertEqual(record['binding_ranges']['vs'][0]['words'], [])
        self.assertEqual(record['binding_ranges']['vs'][0]['metadata'], list(vs))
        self.assertEqual(record['binding_ranges']['ps'][0]['metadata'], list(ps))
        self.assertEqual(record['binding_ranges']['ps'][0]['words'], [2])

    def test_frame_change(self):
        m = fixture()
        original = m.read
        frame_address = m.locations['renderer']+0x1f210
        hits = 0
        def changing(address, size):
            nonlocal hits
            if address == frame_address:
                hits += 1
                if hits == 2:
                    m.put(frame_address, 112)
            return original(address, size)
        m.read = changing
        with self.assertRaisesRegex(ValueError, 'frame/queue changed'):
            h.stable_snapshot(m, m.base)

    def test_same_frame_packet_mutation(self):
        m = fixture()
        original = m.read
        packet = m.locations['packet']
        hits = 0
        def changing(address, size):
            nonlocal hits
            if address == packet and size == 0x48:
                hits += 1
                if hits == 2:
                    m.put(packet+0x38, 1234)
            return original(address, size)
        m.read = changing
        with self.assertRaisesRegex(ValueError, 'double snapshot changed'):
            h.stable_snapshot(m, m.base)

    def test_duplicate_packet(self):
        m = fixture()
        queue = m.locations['queue']
        m.write(queue+16, m.read(queue, 16))
        m.put(m.locations['context']+0x2b80, 2, 'I')
        with self.assertRaisesRegex(ValueError, 'one haze draw packet'):
            h.stable_snapshot(m, m.base)

    def test_shader_validation(self):
        for kind in (0, 1):
            h.program(dxbc(kind), kind)
            with self.assertRaisesRegex(ValueError, 'shader program'):
                h.program(dxbc(kind), 1-kind)
        for at, value in ((24, 60), (28, 65), (32, 0), (40, 100), (48, 99)):
            data = bytearray(dxbc(0))
            struct.pack_into('<I', data, at, value)
            with self.assertRaises(ValueError):
                h.program(data, 0)


if __name__ == '__main__':
    unittest.main()
