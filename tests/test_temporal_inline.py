import copy
import json
from pathlib import Path
import sys
import unittest
import hashlib
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from temporal_inline import review_contract, pack_reference, review_pixels
from review_temporal_inputs import parse


def fixture():
    p = dict(schema=1, allocation_bytes=100, failure='', creation_completed=True,
             attempted=True, issued=True, restored=True, copied=True, collected=True, released=True,
             shutdown_deferred=True, create_result=1, evaluate_result=1, release_result=1, destroy_result=1,
             device=1000, width=32, height=32, create_flags=75, mode=5, preset=11,
             input_formats=[26, 41, 42], converted_formats=[2, 41, 16], mv_scale=[1, 1], pre_exposure=1,
             mv_jittered=False, reset=True, frame_ms=16.66666603088379, frame=104, epoch=204,
             generation=10, command=800, completion_fence=801, completion_value=1, native_output=900, raw=[901, 902, 903], jitter=[.1875, .21875])
    sample = dict(frame=104, reset=10, command=800, fence=801, inline_ok=True, completed=True, completed_value=1,
                  depth_end=1, dispatch=2, copy_begin=3, copy_end=4, color_begin=5, color_end=6,
                  dispatch_forward=7, inline_begin=8, inline_end=9, dispatch_exit=10, close=11,
                  execute=12, execute_exit=13, retire_exit=14)
    c = dict(inline_dlaa=dict(schema=1, receipt=4, failure='', attempted=True, bindings_equal=True, saved=True, **{'pass': p}),
             device=1000, width=32, height=32, records=[{} for _ in range(9)],
             submission=dict(samples=[dict(inline_begin=0, inline_end=0, inline_ok=False) for _ in range(9)]),
             native_bindings=dict(records=[{} for _ in range(9)]), native_compute=dict(records=[{} for _ in range(9)]),
             projection_stimulus=dict(records=[{} for _ in range(9)]),
             owned_gpu_inputs=dict(sets=[{} for _ in range(9)], sdk=dict(attempted=False, saved=False)))
    c['records'][4] = dict(frame=104, epoch=204, inputs=[{}, {}, {}, {}, dict(resource=900)])
    c['submission']['samples'][4] = sample
    c['native_bindings']['records'][4] = dict(frame=104, generation=10, command=800)
    c['native_compute']['records'][4] = dict(states=[{}, {}, {}, {}, dict(known=True, conflict=False, value=8)])
    c['projection_stimulus']['records'][4] = dict(jitter_bits=[1052770304, 3202351104])  # .375, -.4375
    c['owned_gpu_inputs']['sets'][4] = dict(inputs=[901, 902, 903])
    h = dict(version='1.17.0', inline_receipt=4, inline_same_list=True, presentation_writes=1,
             sdk_evaluation_limit=1, sdk_image_byte_limit=419430400)
    return h, c


class InlineReviewTests(unittest.TestCase):
    def test_complete(self):
        self.assertEqual(review_contract(*fixture()), 4)

    def test_failed_lifetime_or_api(self):
        for key in ('creation_completed', 'issued', 'restored', 'copied', 'collected', 'released', 'shutdown_deferred'):
            h, c = fixture(); c['inline_dlaa']['pass'][key] = False
            with self.subTest(key=key), self.assertRaises(ValueError): review_contract(h, c)
        for key in ('create_result', 'evaluate_result', 'release_result', 'destroy_result'):
            h, c = fixture(); c['inline_dlaa']['pass'][key] = 0xbad00002
            with self.subTest(key=key), self.assertRaises(ValueError): review_contract(h, c)

    def test_frame_and_resource_binding(self):
        for key in ('device', 'width', 'height', 'frame', 'epoch', 'generation', 'command', 'native_output', 'completion_fence', 'completion_value'):
            h, c = fixture(); c['inline_dlaa']['pass'][key] += 1
            with self.subTest(key=key), self.assertRaises(ValueError): review_contract(h, c)
        h, c = fixture(); c['inline_dlaa']['pass']['raw'].reverse()
        with self.assertRaises(ValueError): review_contract(h, c)

    def test_order_and_retirement(self):
        keys = ('depth_end', 'dispatch', 'copy_begin', 'copy_end', 'color_begin', 'color_end',
                'dispatch_forward', 'inline_begin', 'inline_end', 'dispatch_exit', 'close', 'execute')
        for a, b in zip(keys, keys[1:]):
            h, c = fixture(); s = c['submission']['samples'][4]; s[a] = s[b]
            with self.subTest(order=a), self.assertRaises(ValueError): review_contract(h, c)
        for key, value in (('completed', False), ('completed_value', 0), ('retire_exit', 13), ('inline_ok', False)):
            h, c = fixture(); c['submission']['samples'][4][key] = value
            with self.subTest(key=key), self.assertRaises(ValueError): review_contract(h, c)

    def test_extra_frame_and_state(self):
        for change in ('extra', 'bindings', 'state', 'deferred'):
            h, c = fixture()
            if change == 'extra': c['submission']['samples'][5]['inline_ok'] = True
            if change == 'bindings': c['inline_dlaa']['bindings_equal'] = False
            if change == 'state': c['native_compute']['records'][4]['states'][4]['value'] = 64
            if change == 'deferred': c['owned_gpu_inputs']['sdk']['attempted'] = True
            with self.subTest(change=change), self.assertRaises(ValueError): review_contract(h, c)

    def test_sdk_parameters(self):
        for key, value in (('reset', False), ('jitter', [0, 0]), ('jitter', [float('nan'), 0]), ('mv_jittered', True),
                           ('create_flags', 0), ('mode', 2), ('preset', 0), ('mv_scale', [32, 32]),
                           ('frame_ms', 0), ('allocation_bytes', 419430401)):
            h, c = fixture(); c['inline_dlaa']['pass'][key] = value
            with self.subTest(key=key), self.assertRaises(ValueError): review_contract(h, c)

    def test_reference_quantization(self):
        import numpy as np
        rgb = np.array([[0, 0, 0], [1, 1, 1], [1.0078125, 1.0234375, 1.015625], [-1, -1, -1]], dtype=np.float32)
        expected = [0, (15 << 6) | ((15 << 6) << 11) | ((15 << 5) << 22),
                    (15 << 6) | (((15 << 6)+2) << 11) | ((15 << 5) << 22), 0]
        self.assertEqual(pack_reference(rgb).tolist(), expected)

    def test_header(self):
        prior = Path(__file__).resolve().parents[1]/'local/dlaa-bindings-live-20260926/v1.16.0/inputs-36704-134348538801908752.jsonl'
        if not prior.exists(): self.skipTest('local accepted capture unavailable')
        h = json.loads(prior.read_text().splitlines()[0]); h.update(version='1.17.0', presentation_writes=1,
            inline_receipt=4, inline_same_list=True, sdk_evaluation_limit=1, sdk_image_byte_limit=419430400, sdk_output_limit=67108864)
        self.assertEqual(parse(json.dumps(h).encode())[0]['version'], '1.17.0')
        for key in ('presentation_writes', 'inline_receipt', 'sdk_evaluation_limit', 'sdk_image_byte_limit', 'sdk_output_limit'):
            bad = copy.deepcopy(h); bad[key] += 1
            with self.subTest(key=key), self.assertRaises(ValueError): parse(json.dumps(bad).encode())

    def test_pixels_and_corruption(self):
        import numpy as np
        for damage in ('none', 'packed', 'nonfinite', 'sentinel', 'conversion', 'hash', 'size', 'unchanged'):
            with self.subTest(damage=damage), tempfile.TemporaryDirectory() as directory:
                h, c = fixture(); h['motion_prefix'] = 'fixture'; folder = Path(directory)
                color = np.tile(np.array([.5, 1, 1.5, 1], '<f4'), (1024, 1))
                depth = np.full(1024, .5, '<f4'); motion = np.zeros((1024, 2), '<f4')
                sdk = np.tile(np.array([1, 2, 3, 1], '<f4'), (1024, 1))
                native_word = (15 << 6) | ((16 << 6) << 11) | (((16 << 5)+16) << 22)
                native = np.full(1024, native_word, '<u4')
                refs = []
                for i, array in enumerate((color, depth, motion)):
                    name = f'reference-{i}.bin'; (folder/name).write_bytes(array.tobytes()); refs.append(dict(file=name))
                c['owned_gpu_inputs']['sets'][4]['files'] = refs
                if damage == 'packed': native[0] ^= 1
                if damage == 'nonfinite': sdk[0, 0] = float('nan')
                if damage == 'sentinel': sdk[0, 0] = 65504
                if damage == 'conversion': depth[0] = .25
                if damage == 'unchanged': sdk = color.copy()
                files = []
                for suffix, array in zip(('color.f32', 'depth.f32', 'motion.f32', 'dlaa.rgba32f', 'native.u32'), (color, depth, motion, sdk, native)):
                    name = 'fixture.inline-'+suffix; raw = array.tobytes(); (folder/name).write_bytes(raw)
                    files.append(dict(file=name, bytes=len(raw), sha256=hashlib.sha256(raw).hexdigest()))
                if damage == 'hash': files[0]['sha256'] = '0'*64
                if damage == 'size': files[0]['bytes'] += 4
                c['inline_dlaa']['files'] = files
                if damage == 'none': self.assertEqual(review_pixels(h, c, folder)['inline_packed_pixels_checked'], 1024)
                else:
                    with self.assertRaises(ValueError): review_pixels(h, c, folder)


if __name__ == '__main__': unittest.main()
