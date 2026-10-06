from copy import deepcopy
from pathlib import Path
import json, sys, unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from continuous_depth_schedule import review

FIXTURE = None


def status(t):
    return dict(depth_schedule=t, depth_eligibility=dict(lease=dict(checks=t['checks'], rejected=t['missing'])))


class DepthScheduleTests(unittest.TestCase):
    def setUp(self):
        if FIXTURE is None: raise RuntimeError('Supply JSONL output from depth_schedule_trace_tests')
        self.cases = {r['case']: deepcopy(r['trace']) for r in FIXTURE}

    def test_native_serialization_and_bounded_sampling(self):
        for t in self.cases.values(): self.assertTrue(review(status(t))['diagnostic_consistency_verified'])
        t = self.cases['bounded']
        self.assertEqual((t['checks'], t['missing'], t['captured'], len(t['samples'])), (105, 4, 4, 4))
        self.assertEqual(review(status(t))['stable_writer_before_consumer_observed'], [1, 2, 3, 4])

    def test_current_interval_not_final_state(self):
        s = self.cases['bounded']['samples'][0]
        self.assertEqual(s['positions']['writer'], [1, 2]); self.assertEqual(s['plan']['index'], 4)
        self.assertEqual(s['positions']['span_packets'][0][:2], [[1, 2], [1, 6]])
        self.assertEqual((s['plan']['trackers'][0]['spans'][0][6], s['plan']['trackers'][0]['final']), (4, 128))
        self.assertFalse(review(status(self.cases['bounded']))['gpu_depth_admission_proven'])

    def test_invalid_or_late_evidence_does_not_imply_order(self):
        for name in ('changed-writer', 'changed-owner', 'unstable-plan', 'late-writer', 'missing-writer', 'oversized-plan'):
            with self.subTest(name=name): self.assertEqual(review(status(self.cases[name]))['stable_writer_before_consumer_observed'], [])

    def test_reject_corrupt_coverage_or_roles(self):
        for key, value in (('checks', 104), ('missing', 3), ('roles', ['color']*3), ('capacity', 8), ('captured', 5)):
            s = status(deepcopy(self.cases['bounded'])); s['depth_schedule'][key] = value
            with self.subTest(key=key), self.assertRaises(ValueError): review(s)

    def test_reject_corrupt_positions(self):
        for key, value in (('writer', [1, 7]), ('writer', [0, 2]), ('span_packets', []), ('scanned', 32769)):
            s = status(deepcopy(self.cases['bounded'])); s['depth_schedule']['samples'][0]['positions'][key] = value
            with self.assertRaises(ValueError): review(s)

    def test_reject_unbounded_or_reclassified_samples(self):
        for action in ('duplicate', 'missing', 'fast', 'writer'):
            t = deepcopy(self.cases['bounded'])
            if action == 'duplicate': t['samples'].append(deepcopy(t['samples'][0]))
            if action == 'missing': t['samples'][0]['missing'] = True
            if action == 'fast': t['samples'][-1]['at'][0] -= 1
            if action == 'writer': t['samples'][0]['actual_writer'][3] += 1
            with self.assertRaises(ValueError): review(status(t))

    def test_retained_baseline_and_monotonic_counters(self):
        t = self.cases['bounded']; old = review(status(t)); self.assertTrue(review(status(t), old))
        changed = deepcopy(t); changed['samples'][0]['at'][0] += 1
        with self.assertRaises(ValueError): review(status(changed), old)
        changed = deepcopy(t); changed['checks'] -= 1
        with self.assertRaises(ValueError): review(status(changed), old)

if __name__ == '__main__':
    FIXTURE = [json.loads(line) for line in Path(sys.argv.pop(1)).read_text().splitlines()]
    unittest.main()
