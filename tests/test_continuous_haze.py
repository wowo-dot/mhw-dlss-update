"""Corruption tests using explicitly supplied native synthetic capture output."""
import argparse
import copy
import json
from pathlib import Path
import sys
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from continuous_haze import review

FIXTURE = None


class HazeReviewTests(unittest.TestCase):
    def setUp(self):
        if FIXTURE is None:
            self.skipTest('supply --fixture from scaled_stream_route_fixture haze case')
        self.r = copy.deepcopy(FIXTURE)

    def check(self, previous=None, final=True):
        return review(dict(scaled_route=dict(haze_capture=self.r)), previous, final)

    def test_accepted_with_limited_claims(self):
        self.assertEqual(self.check()['count'], 3)
        self.check(previous=self.r)

    def test_rejects_corrupted_receipts(self):
        mutations = [
            lambda r: r.update(attempts=17),
            lambda r: r.update(count=4),
            lambda r: r.update(entered=True),
            lambda r: r['samples'][0]['queue'].__setitem__(2, 999),
            lambda r: r['samples'][0]['shaders'][1].update(sha256='0'*64),
            lambda r: r['samples'][0]['vertices_bits'].__setitem__(0, 0x7fc00000),
            lambda r: r['samples'][0]['stage_bindings'][1]['words'].__setitem__(0, 2+(64<<4)),
            lambda r: r['samples'][0]['textures'][0].update(index=2),
            lambda r: r['samples'][0]['named_objects'].__setitem__(0, 1),
            lambda r: r['samples'][0].update(gpu_bindings_verified=True),
            lambda r: r['samples'][0].update(resource_states_verified=True),
            lambda r: r['samples'][0].update(same_frame_enqueued=False),
            lambda r: r['samples'][0]['stage_bindings'][0].update(count=1, start=4096, words=[0]),
            lambda r: r['samples'][0]['textures'][0]['owner'].__setitem__(12, 0),
            lambda r: r['samples'][0]['stage_bindings'][1].update(count=129),
            lambda r: r['samples'][0]['stage_bindings'][0]['metadata'].__setitem__(0, 256),
        ]
        for i, mutate in enumerate(mutations):
            with self.subTest(case=i):
                self.r = copy.deepcopy(FIXTURE)
                mutate(self.r)
                with self.assertRaises(ValueError):
                    self.check()

    def test_opaque_metadata_and_empty_range(self):
        self.assertEqual(self.r['schema'], 2)
        for q in self.r['samples']:
            q['stage_bindings'][0]['start'] = 65535
        self.check()
        previous = copy.deepcopy(self.r)
        self.r['samples'][0]['stage_bindings'][0]['metadata'][0] ^= 1
        with self.assertRaises(ValueError):
            self.check(previous)

    def test_retained_capture_is_immutable(self):
        previous = copy.deepcopy(self.r)
        self.r['samples'][0]['strength_bits'] += 1
        with self.assertRaises(ValueError):
            self.check(previous)

    def test_uncaptured_run_is_not_recipe_proof(self):
        self.r.update(attempts=0, count=0, phase=0, reason=0, rejected=[0]*12, samples=[])
        self.assertEqual(self.check()['count'], 0)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--fixture', type=Path, required=True)
    args, remaining = parser.parse_known_args()
    FIXTURE = json.loads(args.fixture.read_bytes())
    unittest.main(argv=[sys.argv[0]] + remaining)
