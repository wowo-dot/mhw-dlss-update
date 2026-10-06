import sys
import unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from aa_scene import capture, decode_engine_dimensions


class Memory:
    base = 0x140000000

    def __init__(self):
        self.values = {}
        self.reads = 0
        for rva, pointer in ((0x51238C8, 0x10000), (0x51C4480, 0x20000), (0x51C4368, 0x60000)):
            self.values[self.base+rva] = pointer
        self.values.update({self.base+0x51CC080: 1, 0x10000+0x15D0: 0x90000,
                            0x20000+0x1F450: 0x50000, 0x20000+0x1F210: 10, 0x20000+0x1F208: 10,
                            0x20000+0x78: 0xB0000, 0x90000: self.base+0x351BDA8,
                            0x90000+0x30: 0xA0000, 0xA0000: self.base+0x351C1A8,
                            0x90000+0xC: 8, 0x90000+0x14: 0x11, 0xA0000+0x14: 0x11})

    def value(self, address, fmt):
        self.reads += 1
        return self.values.get(address, 0)


class Cases(unittest.TestCase):
    def test_flagged_color_dimensions(self):
        self.assertEqual(decode_engine_dimensions(0x85A00A00),
                         dict(dimensions_word=0x85A00A00, width=2560,
                              height=1440, dimension_flags=2))

    def test_dimension_flags_are_independent_of_extent(self):
        for flags in range(4):
            for width, height in ((0, 0), (2560, 1440), (0xFFFF, 0x3FFF)):
                word = (flags << 30) | (height << 16) | width
                decoded = decode_engine_dimensions(word)
                self.assertEqual((decoded["width"], decoded["height"]), (width, height))
                self.assertEqual(decoded["dimension_flags"], flags)
                self.assertEqual(decoded["dimensions_word"], word)

    def test_aa_off_missing_resources_is_observation(self):
        reader = Memory()
        result = capture(reader)
        self.assertEqual([x["general_enable"] for x in result["layers"]], [False, False])
        self.assertEqual(result["scene"], dict(color=None, motion=None, depth=None))
        self.assertTrue(result["anchors_stable"] and result["links_stable"] and result["frame_unchanged"])
        self.assertFalse(result["gpu_execution_verified"] or result["ownership_proven"])
        self.assertLess(reader.reads, 150)

    def test_each_native_choice(self):
        for taa, fxaa in ((0, 0), (1, 0), (0, 1), (1, 1)):
            reader = Memory()
            reader.values[0x60000+0xE9E4] = taa
            reader.values[0x60000+0xE9E5] = fxaa
            result = capture(reader)
            self.assertEqual((result["table_taa"], result["table_fxaa"]), (taa, fxaa))

    def test_cycle_rejected(self):
        reader = Memory()
        reader.values[0xA0000+0x30] = 0x90000
        with self.assertRaisesRegex(ValueError, "cycle"):
            capture(reader)

    def test_invalid_choice_rejected(self):
        reader = Memory()
        reader.values[0x60000+0xE9E4] = 2
        with self.assertRaisesRegex(ValueError, "table bytes"):
            capture(reader)

    def test_non_dx12_rejected(self):
        reader = Memory()
        reader.values[reader.base+0x51CC080] = 0
        with self.assertRaisesRegex(ValueError, "DX12"):
            capture(reader)

    def test_secondary_backing_and_metadata(self):
        reader = Memory()
        reader.values.update({0x50000+0x620: 0xC0000, 0xC0000+0x78: 0xD0000,
                              0xD0000+0x60: 0xE0000, 0xD0000+0xF0: 0xF0000,
                              0xD0000+0x88: 0x85A00A00, 0xD0000+0x94: 42,
                              0xF0000+0x20: 0x12340000})
        motion = capture(reader)["scene"]["motion"]
        self.assertEqual((motion["width"], motion["height"], motion["gpu_format"]), (2560, 1440, 42))
        self.assertEqual(motion["resource"], 0x12340000)
        self.assertEqual(motion["dimensions_word"], 0x85A00A00)
        self.assertEqual(motion["dimension_flags"], 2)
        self.assertTrue(motion["fields_stable"])

    def test_other_taa_classes_do_not_use_taa2_resource_layout(self):
        for offset, name in ((0x354DD78, "taa1"), (0x354DF48, "taa3")):
            reader = Memory()
            reader.values[0x90000] = reader.base+offset
            layer = capture(reader)["layers"][0]
            self.assertEqual(layer["name"], name)
            self.assertNotIn("resources", layer)

    def test_changing_frame_is_not_same_frame_evidence(self):
        reader = Memory()
        original = reader.value
        def changing(address, fmt):
            value = original(address, fmt)
            if address == 0x20000+0x1F210:
                reader.values[address] += 1
            return value
        reader.value = changing
        self.assertFalse(capture(reader)["frame_unchanged"])


if __name__ == "__main__":
    unittest.main()
