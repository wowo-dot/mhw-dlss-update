import copy
import json
from pathlib import Path
import sys
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from arm_aa_boundary import HOST, parse, validate
from review_aa_owned import review


def fixture():
    base = 0x140000000
    header = dict(event='start', schema=1, version='0.2.0', host_sha256=HOST, host_version='0.0.0.0',
                  experiment='independent-aa-owned-in-place', sample_limit=8, write_scope='scoped-post-queue-only',
                  aa_option_writes=0, jitter_writes=0, pass_phase=15, pass_order=2, in_place=True,
                  private_gpu_commands=0, sdk_used=False, pid=7, creation=8, base=base, dll_sha256='a'*64)
    rows = [header, dict(event='ready', reserved=True, hooks_installed=False),
            dict(event='prepared', ok=True, graphics_api='D3D12', device=500, width=2560, height=1440, swapchain_format=87),
            dict(event='armed', hooks=1)]
    nodes = [dict(descriptor=d, unit=u, vtable=v, input=a, output=b, phase=15, order=o, flags=f)
             for d, u, v, a, b, o, f in ((100, 1000, base+0x3538400, 10, 20, 0, [0, 0, 0]),
                  (200, 2000, base+0x3537a70, 20, 30, 100, [0, 0, 0]), (300, 3000, base+0x3536c98, 0, 30, 1024, [0, 1, 0]),
                  (400, 0, 0, 30, 40, 134217713, [1, 0, 0]))]
    for i in range(8):
        before = dict(manager=1, context=2, view=3, renderer=4, frame=i+10, epoch=i+10, bank=i%2, count=4,
                      packets=200, enabled=1, collecting=1, taa=0, fxaa=0, rectangle=[0, 0, 2560, 1440],
                      scratch=[20, 30], attachments=[10, 40], nodes=copy.deepcopy(nodes))
        after = copy.deepcopy(before); after.update(collecting=0, packets=231)
        node = dict(descriptor=900+i, unit=1900+i, vtable=2900, input=0, output=20, phase=15, order=2, flags=[0, 1, 0])
        callback = copy.deepcopy(after); callback.update(count=5, packets=210)
        callback['nodes'].insert(1, node)
        inside_after = copy.deepcopy(callback); inside_after['packets'] = 231
        owned = dict(inserted=True, removed=True, returned=True, complete=True, reason=0, callbacks=1, thread=50,
                     descriptor=900+i, unit=1900+i, vtable=2900, phase=15, order=2, in_place=True, color_owner=20,
                     callback_valid=True, neutral=True, callback_count=5, callback_frame=i+10, after_count=5,
                     restored_count=4, packets_before=210, packets_after=210,
                     resource=dict(valid=True, identity=600, device=500, dimension=3, width=2560, height=1440,
                                   format=26, flags=4, mips=1, array=1, samples=1),
                     callback=callback, after=inside_after, restored=copy.deepcopy(after))
        rows.append(dict(event='sample', index=i, thread=50, entered=True, returned=True, complete=True,
                         same_frame=True, before=before, after=after, owned=owned))
    rows.append(dict(event='stopped', samples=8, visits=8, fault=0, pending=0, drained=True,
                     hooks_disabled=True, owned_descriptors_detached=True))
    return rows


def raw(rows):
    return '\n'.join(map(json.dumps, rows)).encode()


class Cases(unittest.TestCase):
    def test_accept_and_limit_claims(self):
        result = review(raw(fixture()), 'a'*64)
        self.assertTrue(result['accepted'])
        self.assertEqual(result['exact_queue_restorations'], 8)
        self.assertFalse(result['live_independent_dlaa_verified'])
        self.assertFalse(result['gpu_pixels_states_lifetime_verified'])

    def test_arm_scope_and_generation(self):
        rows = parse(raw(fixture()[:2]), owned=True)
        validate(rows, 7, 8, 'a'*64)
        with self.assertRaises(ValueError): validate(rows, 7, 9, 'a'*64)
        with self.assertRaises(ValueError): parse(raw(rows))
        for key, value in (('write_scope', 'all'), ('pass_order', 100), ('jitter_writes', 1), ('aa_option_writes', 1), ('sdk_used', True)):
            bad = copy.deepcopy(rows); bad[0][key] = value
            with self.assertRaises(ValueError): parse(raw(bad), owned=True)

    def test_mutated_native_graph(self):
        for where in ('callback', 'after', 'restored'):
            rows = fixture(); rows[4]['owned'][where]['nodes'][0]['output'] += 1
            with self.assertRaises(ValueError): review(raw(rows), 'a'*64)

    def test_wrong_owned_placement_or_double_call(self):
        for key, value in (('order', 3), ('callbacks', 2), ('removed', False), ('neutral', False), ('color_owner', 30), ('packets_after', 211)):
            rows = fixture(); rows[4]['owned'][key] = value
            with self.assertRaises(ValueError): review(raw(rows), 'a'*64)

    def test_bad_resource(self):
        for key, value in (('device', 501), ('width', 1920), ('format', 10), ('samples', 4), ('valid', False)):
            rows = fixture(); rows[4]['owned']['resource'][key] = value
            with self.assertRaises(ValueError): review(raw(rows), 'a'*64)

    def test_partial_or_unclean(self):
        rows = fixture()
        with self.assertRaises(ValueError): review(raw(rows[:-1]), 'a'*64)
        with self.assertRaises(ValueError): review(raw(rows), 'b'*64)
        for key, value in (('pending', 1), ('fault', 1), ('owned_descriptors_detached', False), ('hooks_disabled', False)):
            bad = copy.deepcopy(rows); bad[-1][key] = value
            with self.assertRaises(ValueError): review(raw(bad), 'a'*64)


if __name__ == '__main__':
    unittest.main()
