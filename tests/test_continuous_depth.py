"""Owned-memory fixtures and corrupt/missing depth diagnostic receipts."""
from copy import deepcopy
import json
from pathlib import Path
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from continuous_depth import review
from arm_continuous_dlaa import parse
from review_continuous_dlaa import review as review_run

FIXTURES = {}


def setUpModule():
    p = subprocess.run([str(ROOT / 'build/bin/Release/continuous_runtime_tests.exe'), 'depth-eligibility'],
                       capture_output=True, text=True, check=True)
    FIXTURES.update((v['case'], v) for v in map(json.loads, p.stdout.splitlines()))


class DepthTests(unittest.TestCase):
    def setUp(self):
        self.s = deepcopy(FIXTURES['descriptor-only'])

    def test_owned_memory_fixtures(self):
        self.assertEqual(len(FIXTURES), 18)
        for name, value in FIXTURES.items():
            with self.subTest(name=name):
                self.assertTrue(review(value)['depth_eligibility_verified'])

    def test_scaled_readiness_is_separate_from_missing_depth(self):
        s = deepcopy(FIXTURES['lease-6'])
        s['dispatch_reasons'][7] = 1
        self.assertEqual(review(s, scaled=True)['scaled_readiness_skips'], 1)
        with self.assertRaises(ValueError):
            review(s)  # A DLAA log cannot claim the additional scaled branch.
        s['dispatch_reasons'][7] = 2
        with self.assertRaises(ValueError):
            review(s, scaled=True)  # Only one depth lease passed.
        s = deepcopy(FIXTURES['lease-0'])
        s['dispatch_reasons'][7] = 0
        with self.assertRaises(ValueError):
            review(s, scaled=True)  # Cannot hide a rejected depth lease.

    def test_scaled_readiness_counter_cannot_roll_back(self):
        s = deepcopy(FIXTURES['lease-6'])
        s['dispatch_reasons'][7] = 1
        previous = review(s, scaled=True)
        s['dispatch_reasons'][7] = 0
        with self.assertRaises(ValueError):
            review(s, previous, scaled=True)

    def test_distinguish_metadata_from_texture(self):
        self.assertEqual(review(self.s)['scene_changed_fields'], {'cpu_descriptor': 1})
        self.assertEqual(review(FIXTURES['texture-resource'])['scene_changed_fields'], {'texture.resource': 1})
        self.assertEqual(review(FIXTURES['attachment-owner-resource'])['scene_changed_fields'],
                         {'attachment_owner': 1, 'attachment_resource': 1})

    def test_failed_read_and_invalid_pointer_distinct(self):
        failed = FIXTURES['required-read-failed']['depth_eligibility']['scene']['samples'][0]
        invalid = FIXTURES['invalid-container']['depth_eligibility']['scene']['samples'][0]
        self.assertFalse(failed['readable'])
        self.assertEqual(failed['reads'][1], 1)
        self.assertFalse(invalid['readable'])
        self.assertEqual(invalid['reads'][1], 0)
        optional = review(FIXTURES['optional-srv-read-failed'])['scene']
        self.assertEqual((optional['mismatches'], optional['unreadable']), (1, 0))

    def test_each_lease_predicate(self):
        fields = ['list', 'generation', 'frame', 'depth_available', 'already_evaluated']
        for i, field in enumerate(fields):
            self.assertEqual(review(FIXTURES[f'lease-{i}'])['lease_failed_predicates'], {field: 1})
        self.assertEqual(review(FIXTURES['lease-6'])['lease']['passes'], 1)

    def test_bounded_samples(self):
        result = review(FIXTURES['bounded'])
        for name in ('scene', 'lease'):
            self.assertEqual([s['ordinal'] for s in result[name]['samples']], list(range(1, 9)) + [1000])

    def test_wrong_difference(self):
        self.s['depth_eligibility']['scene']['samples'][0]['difference'] = 1
        with self.assertRaises(ValueError):
            review(self.s)

    def test_missing_sample(self):
        self.s['depth_eligibility']['scene']['samples'] = []
        with self.assertRaises(ValueError):
            review(self.s)

    def test_counter_coverage(self):
        self.s['dispatch_reasons'][3] += 1
        with self.assertRaises(ValueError):
            review(self.s)

    def test_failed_read_shape(self):
        self.s['depth_eligibility']['scene']['samples'][0]['reads'][2] = 1
        with self.assertRaises(ValueError):
            review(self.s)

    def test_partial_read_not_compared(self):
        self.s = deepcopy(FIXTURES['required-read-failed'])
        self.s['depth_eligibility']['scene']['samples'][0]['difference'] = 1
        with self.assertRaises(ValueError):
            review(self.s)

    def test_immutable_receipt(self):
        old = review(self.s)
        self.s['depth_eligibility']['scene']['samples'][0]['at'][0] += 1
        with self.assertRaises(ValueError):
            review(self.s, old)

    def test_rollback(self):
        old = review(FIXTURES['bounded'])
        with self.assertRaises(ValueError):
            review(self.s, old)

    def test_full_review_and_policy_required(self):
        root = ROOT / 'local/native-aa-state-admission-20260927/v1.21.3'
        rows = [json.loads(x) for x in (root / 'inputs-2524-134349976894368484.jsonl').read_bytes().splitlines()]
        startup = [json.loads(x) for x in (root / 'startup-2524-134349976894368484.jsonl').read_bytes().splitlines()]
        rows[0]['version'] = startup[0]['version'] = '1.21.4'
        with self.assertRaises(ValueError):
            parse((json.dumps(rows[0]) + '\n').encode())
        rows[0]['depth_eligibility'] = 'depth-eligibility-observation-v1'
        for status in rows[4:-1]:
            trace = deepcopy(FIXTURES['unchanged']['depth_eligibility'])
            frames = status['frames']
            scene_rejects, lease_rejects = status['dispatch_reasons'][3], status['dispatch_reasons'][7]
            trace['scene'].update(checks=frames + scene_rejects + lease_rejects, passes=frames + lease_rejects,
                                  mismatches=scene_rejects, rejected=scene_rejects)
            trace['lease'].update(checks=frames + lease_rejects, passes=frames, rejected=lease_rejects)
            for name, total, fixture in (('scene', scene_rejects, 'descriptor-only'), ('lease', lease_rejects, 'lease-5')):
                sample = FIXTURES[fixture]['depth_eligibility'][name]['samples'][0]
                trace[name]['differences'] = [int(bool(sample['difference'] & (1 << i))) * total
                                              for i in range(len(trace[name]['differences']))]
                trace[name]['samples'] = []
                for ordinal in list(range(1, min(total, 8) + 1)) + ([total] if total > 8 else []):
                    row = deepcopy(sample)
                    row['ordinal'] = ordinal
                    row['at'][0] += ordinal
                    trace[name]['samples'].append(row)
            status['depth_eligibility'] = trace
        self.assertTrue(review_run(rows, startup, rows[0]['dll_sha256'])['depth_eligibility']['depth_eligibility_verified'])
        del rows[-2]['depth_eligibility']
        with self.assertRaises(KeyError):
            review_run(rows, startup, rows[0]['dll_sha256'])


if __name__ == '__main__':
    unittest.main()
