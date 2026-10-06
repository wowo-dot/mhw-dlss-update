from copy import deepcopy
from pathlib import Path
import sys, unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from continuous_scaled_depth import review


def sample(total=1):
    trace = dict(schema=1, policy='bounded-scaled-depth-admission-v1', diagnostic_only=True,
                 capacity=128, event_limit=32, sample_limit=5, events=total, uncorrelated=0,
                 missing=total, readiness_rejected=2, counts=[[0]*16 for _ in range(7)], samples=[])
    trace['counts'][0][1] = total
    for ordinal in list(range(1, min(total, 4)+1)) + ([total] if total >= 5 else []):
        trace['samples'].append(dict(ordinal=ordinal, expected=[100+ordinal, ordinal, 2, 3, 0, 5, 6, 7],
                                    actual=[0]*5, matching=1, ring_overwritten=max(0, ordinal-128),
                                    events=[[100+ordinal, ordinal, 2, 0, 0, 1]+[0]*16]))
    reasons = [0]*12; reasons[7] = total+2
    return dict(scaled_depth_capture=trace, depth_eligibility=dict(lease=dict(rejected=total)), dispatch_reasons=reasons)


class ScaledDepthTests(unittest.TestCase):
    def test_empty_first_and_latest(self):
        for total in (0, 1, 4, 5, 6, 1000):
            self.assertTrue(review(sample(total))['diagnostic_consistency_verified'])

    def test_missing_and_readiness_are_distinct(self):
        for key in ('missing', 'readiness_rejected'):
            s = sample(); s['scaled_depth_capture'][key] += 1
            with self.assertRaises(ValueError): review(s)

    def test_exact_frame_epoch_and_time(self):
        for field, value in ((0, 102), (1, 2), (2, 3), (4, 7), (5, 15)):
            s = sample(); s['scaled_depth_capture']['samples'][0]['events'][0][field] = value
            with self.assertRaises(ValueError): review(s)

    def test_bounded_evidence(self):
        for key, value in (('matching', 129), ('ring_overwritten', 1), ('ordinal', 2), ('actual', [6,7,1,1,0])):
            s = sample(); s['scaled_depth_capture']['samples'][0][key] = value
            with self.assertRaises(ValueError): review(s)
        s = sample(); s['scaled_depth_capture']['samples'][0]['events'] *= 33
        with self.assertRaises(ValueError): review(s)

    def test_counter_coverage_and_partition(self):
        s = sample(); s['scaled_depth_capture']['counts'][0][1] = 0
        with self.assertRaises(ValueError): review(s)
        s['scaled_depth_capture']['counts'][0][2] = 1
        with self.assertRaises(ValueError): review(s)

    def test_rollback_and_immutable_receipt(self):
        old = review(sample(1)); self.assertTrue(review(sample(1000), old))
        with self.assertRaises(ValueError): review(sample(0), old)
        s = sample(1000); s['scaled_depth_capture']['samples'][0]['events'][0][6] = 1
        with self.assertRaises(ValueError): review(s, old)

    def test_unstamped_events_are_explicit(self):
        s = sample(); s['scaled_depth_capture']['counts'][0][1] += 1
        s['scaled_depth_capture']['uncorrelated'] = 1
        self.assertTrue(review(s))

if __name__ == '__main__': unittest.main()
