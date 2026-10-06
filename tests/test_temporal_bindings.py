"""Reject incomplete, stale and malformed native-state restoration inputs."""
import copy
import json
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from temporal_bindings import review_bindings
from review_temporal_inputs import parse


def fixture():
    records, native, submitted = [], [], []
    for i in range(9):
        common = dict(receipt=i, frame=100+i, command=200+i%2)
        root = dict(signature=400, arguments=[dict(index=0, kind=1, address=600, mask=0, constants=[]),
            dict(index=2, kind=2, address=0, mask=(1 << 63)+1, constants=[[0, 42], [63, 0xffffffff]])])
        records.append(dict(**common, generation=10+i, ready=True, pipeline=300, expected_pipeline=300,
            expected_root=400, heaps=[500, 501], compute=root, graphics=dict(signature=0, arguments=[])))
        native.append(dict(**common, binding_pipeline=300, binding_root=400))
        submitted.append(dict(**common, reset=10+i))
    return dict(native_bindings=dict(schema=1, fault=0, pending=0, hooks_disabled=True, drained=True,
        hook_count=18, list_count=2, records=records), native_compute=dict(records=native), submission=dict(samples=submitted))


class BindingReviewTests(unittest.TestCase):
    def test_complete(self):
        r = review_bindings(fixture())
        self.assertTrue(r['native_binding_snapshots_verified'])
        self.assertEqual(r['native_binding_lists'], 2)
        self.assertFalse(r['native_state_restoration_verified'])
        self.assertFalse(r['presentation_changed'])

    def test_closure(self):
        for key, value in [('fault', 1), ('pending', 1), ('hooks_disabled', False), ('drained', False),
                           ('hook_count', 17), ('list_count', 0), ('list_count', 129), ('list_count', 1)]:
            f = fixture(); f['native_bindings'][key] = value
            with self.subTest(key=key, value=value), self.assertRaises(ValueError): review_bindings(f)

    def test_association(self):
        for key in ('frame', 'command', 'generation', 'pipeline', 'expected_pipeline', 'expected_root', 'receipt'):
            f = fixture(); f['native_bindings']['records'][4][key] += 1
            with self.subTest(key=key), self.assertRaises(ValueError): review_bindings(f)

    def test_missing_and_unready(self):
        for change in ('missing', 'unready', 'native_cache', 'signature', 'no_table'):
            f = fixture(); r = f['native_bindings']['records'][8]
            if change == 'missing': f['native_bindings']['records'].pop()
            if change == 'unready': r['ready'] = False
            if change == 'native_cache': f['native_compute']['records'][8]['binding_pipeline'] += 1
            if change == 'signature': r['compute']['signature'] += 1
            if change == 'no_table': r['compute']['arguments'].pop(0)
            with self.subTest(change=change), self.assertRaises(ValueError): review_bindings(f)

    def test_heap_bounds(self):
        for heaps in ([], [0], [500, 500], [500, 501, 502], [True], [2**64]):
            f = fixture(); f['native_bindings']['records'][0]['heaps'] = heaps
            with self.subTest(heaps=heaps), self.assertRaises(ValueError): review_bindings(f)

    def test_argument_damage(self):
        for change in ('index', 'kind', 'mask', 'word', 'offset', 'duplicate', 'null_table', 'rootless', 'descriptor_words'):
            f = fixture(); root = f['native_bindings']['records'][0]['compute']; arg = root['arguments'][1]
            if change == 'index': arg['index'] = 64
            if change == 'kind': arg['kind'] = 6
            if change == 'mask': arg['mask'] = 1
            if change == 'word': arg['constants'][1][1] = 2**32
            if change == 'offset': arg['constants'][1][0] = 64
            if change == 'duplicate': root['arguments'].append(copy.deepcopy(arg))
            if change == 'null_table': root['arguments'][0]['address'] = 0
            if change == 'rootless': root['signature'] = 0
            if change == 'descriptor_words': root['arguments'][0]['constants'] = [[0, 0]]
            with self.subTest(change=change), self.assertRaises(ValueError): review_bindings(f)

    def test_empty_graphics_and_root_descriptors(self):
        f = fixture()
        for kind in (3, 4, 5):
            f['native_bindings']['records'][0]['compute']['arguments'].append(
                dict(index=kind, kind=kind, address=0, mask=0, constants=[]))
        self.assertTrue(review_bindings(f)['native_binding_snapshots_verified'])

    def test_header_budgets(self):
        prior = Path(__file__).resolve().parents[1] / 'local/dlaa-sequence-live-20260926/v1.15.3/inputs-14468-134348456500809665.jsonl'
        if not prior.exists(): self.skipTest('local accepted capture unavailable')
        rows = [json.loads(l) for l in prior.read_text().splitlines()]
        rows[0].update(version='1.16.0', hooks=29, binding_observer=True, binding_hooks=18,
                       binding_lists=128, binding_arguments=64, binding_constant_words=64, presentation_writes=0)
        encode = lambda r: ('\n'.join(json.dumps(x) for x in r)).encode()
        self.assertEqual(parse(encode(rows))[0]['version'], '1.16.0')
        for field in ('hooks', 'binding_hooks', 'binding_lists', 'binding_arguments', 'binding_constant_words', 'presentation_writes'):
            bad = copy.deepcopy(rows); bad[0][field] += 1
            with self.subTest(field=field), self.assertRaises(ValueError): parse(encode(bad))


if __name__ == '__main__': unittest.main()
