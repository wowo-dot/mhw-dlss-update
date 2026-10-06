import copy
import sys
import unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from frame_generation_ownership import trace_review


def evidence():
    rows = [dict(event='ownership_summary', schema=1, drained=True, phase=1, pointers_owned=False,
                 known_bucket_counts_exact=True, sampling='first-four-then-powers-of-two-to-65536',
                 bucket_limit=64, sample_limit=512, dropped_buckets=0, dropped_samples=0,
                 image=1000, image_bytes=2000, qpc_frequency=1000000, calls=4, buckets=4, samples=4)]
    for n, m in enumerate((0, 2, 5, 4), 1):
        common = dict(method=m, phase=int(m == 4), thread=7, caller=1100, caller_rva=100)
        rows.append(dict(event='ownership_bucket', **common, count=1))
        rows.append(dict(event='ownership_sample', **common, sequence=n, qpc=n,
                         object=2222, returned=3333, result=0, args=[0] * 8))
    return rows


class OwnershipReview(unittest.TestCase):
    def test_complete(self):
        r = trace_review(evidence())
        self.assertEqual(r['calls'], 4)
        self.assertFalse(r['FG_activated'])
        self.assertFalse(r['final_HUDless_color_verified'])

    def rejected(self, rows):
        with self.assertRaises((AssertionError, RuntimeError, ValueError)):
            trace_review(rows)

    def test_truncation_is_not_accepted(self):
        for field in ('dropped_buckets', 'dropped_samples'):
            rows = evidence(); rows[0][field] = 1; self.rejected(rows)

    def test_false_count_or_missing_sample(self):
        rows = evidence(); rows[1]['count'] = 2; rows[0]['calls'] = 5; self.rejected(rows)
        rows = evidence(); rows.pop(); rows[0]['samples'] = 3; self.rejected(rows)

    def test_classification_and_order(self):
        rows = evidence(); rows[-1]['caller_rva'] = 101; self.rejected(rows)
        rows = evidence(); rows[-1]['sequence'] = 2; self.rejected(rows)
        rows = evidence(); rows[-1]['qpc'] = 0; self.rejected(rows)

    def test_unrestored_or_reference_ownership(self):
        for field, value in (('drained', False), ('pointers_owned', True)):
            rows = copy.deepcopy(evidence()); rows[0][field] = value; self.rejected(rows)


if __name__ == '__main__':
    unittest.main()
