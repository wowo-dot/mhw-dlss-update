import copy
import json
from pathlib import Path
import sys
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from continuous_retention import review
from review_continuous_dlaa import review as review_run


class RetentionTests(unittest.TestCase):
    def setUp(self):
        self.row = dict(retention='reset-and-fence-v1', depth_copies=300, signal_value=300,
            resource_count=12, hooks_disabled=False, sdk=dict(caller_retained=True),
            recordings=dict(schema=1, policy='reset-and-fence-v1', capacity=128, created=300,
                submitted=300, retired=298, released=297, live=3, peak=4, resource_references=12,
                awaiting_submission=0, awaiting_reset=2, awaiting_gpu=1))

    def test_pending_gpu_and_reset(self):
        self.assertEqual(review(self.row)['released'], 297)

    def test_counts_cannot_hide_unsafe_release(self):
        for key, value in dict(created=301, submitted=296, retired=296, released=300,
                               live=1, peak=129, resource_references=8, awaiting_submission=1,
                               awaiting_reset=1, awaiting_gpu=0).items():
            row = copy.deepcopy(self.row);row['recordings'][key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                review(row)

    def test_ownership_required(self):
        self.row['sdk']['caller_retained'] = False
        with self.assertRaises(ValueError): review(self.row)

    def test_stop_requires_completed_gpu(self):
        self.row['hooks_disabled'] = True
        with self.assertRaises(ValueError): review(self.row)
        r = self.row['recordings'];r.update(released=298, live=2, resource_references=8, awaiting_gpu=0)
        self.row['resource_count'] = 8
        self.assertEqual(review(self.row)['live'], 2)

    def synthetic(self):
        root = Path(__file__).resolve().parents[1] / 'local/dlaa-request-lifetime-20260926/v1.18.3'
        log, = root.glob('inputs-*.jsonl')
        rows = [json.loads(line) for line in log.read_text().splitlines()]
        h = rows[0]; startup = [json.loads(line) for line in (root / f"startup-{h['pid']}-{h['creation']}.jsonl").read_text().splitlines()]
        h['version'] = startup[0]['version'] = '1.18.4';h['retention'] = 'reset-and-fence-v1'
        for row in rows[4:-1]:
            c, n = row['depth_copies'], row['signal_value'];live = min(c, 2);retired = c-live
            row['retention'] = h['retention'];row['sdk']['caller_retained'] = True;row['resource_count'] = 4*live
            row['recordings'] = dict(schema=1, policy=h['retention'], capacity=128, created=c, submitted=n,
                retired=retired, released=retired, live=live, peak=live, resource_references=4*live,
                awaiting_submission=c-n, awaiting_reset=live, awaiting_gpu=0)
        return rows, startup, h['dll_sha256']

    def test_full_review(self):
        r = review_run(*self.synthetic())
        self.assertTrue(r['accepted']);self.assertGreater(r['recordings']['released'], 100)

    def test_full_review_corruption(self):
        for field in ('resource_count', 'released', 'retention'):
            rows, startup, dll = self.synthetic()
            if field == 'resource_count': rows[-2][field] = 0
            elif field == 'released': rows[-2]['recordings'][field] = 0
            else: rows[0][field] = 'bounded-process-lifetime'
            with self.subTest(field=field), self.assertRaises(ValueError): review_run(rows, startup, dll)


if __name__ == '__main__': unittest.main()
