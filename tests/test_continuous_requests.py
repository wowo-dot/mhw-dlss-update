"""Check actual owned callback fixtures and reject corrupt request telemetry."""
import argparse
import copy
import json
from pathlib import Path
import subprocess
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from continuous_requests import review
from review_continuous_dlaa import review as review_run

FIXTURES = {}
PRIOR = None
MODES = ('consumed', 'same-frame', 'wrap', 'blocked', 'unmatched', 'multiple', 'changed', 'reentrant', 'depth')


class RequestReviewTests(unittest.TestCase):
    def setUp(self):
        self.value = copy.deepcopy(FIXTURES['blocked'])

    def rejected(self):
        with self.assertRaises(ValueError):
            review(self.value)

    def test_all_native_fixtures(self):
        for mode, value in FIXTURES.items():
            with self.subTest(mode=mode):
                result = review(value)
                self.assertEqual(result['collisions'], int(mode in ('same-frame', 'wrap', 'blocked', 'multiple')))

    def test_collision_classification(self):
        self.assertEqual(review(FIXTURES['same-frame'])['collision_classification'], 'same-frame')
        self.assertEqual(review(FIXTURES['wrap'])['collision_classification'], 'forward-slot-reuse')
        self.assertIsNone(review(FIXTURES['consumed'])['collision_classification'])

    def test_bypass_evidence(self):
        old = self.value['first_collision']['old_request']
        self.assertEqual(old['bypasses'], 10000)
        self.assertEqual(old['last_bypass_publishers'], 2)
        self.assertEqual(self.value['taa']['last_blocked']['ordinal'], 10000)

    def test_schema_policy(self):
        self.value['policy'] = 'discard-old'
        self.rejected()

    def test_publication_partition(self):
        self.value['published'] += 1
        self.rejected()

    def test_consumer_partition(self):
        self.value['taa']['calls'] += 1
        self.rejected()

    def test_bad_slot(self):
        self.value['first_collision']['slot'] = 6
        self.rejected()

    def test_missing_collision(self):
        self.value['first_collision'] = None
        self.rejected()

    def test_incoming_already_published(self):
        self.value['first_collision']['new_request']['sequence'] = 3
        self.rejected()

    def test_empty_bypass_evidence(self):
        self.value['first_collision']['old_request']['last_bypass_publishers'] = 0
        self.rejected()

    def test_excess_matches(self):
        self.value['taa']['last_blocked']['matches'] = 33
        self.rejected()

    def test_wrong_matching_packet(self):
        self.value['taa']['last_blocked']['newest']['packet'] += 8
        self.rejected()

    def test_bad_sample_order(self):
        self.value['taa']['first_blocked']['ordinal'] = 10001
        self.rejected()

    def test_counter_type(self):
        self.value['collisions'] = True
        self.rejected()

    def test_fault_agreement(self):
        review(self.value, 6)
        with self.assertRaises(ValueError):
            review(self.value, 0)
        with self.assertRaises(ValueError):
            review(FIXTURES['consumed'], 6)

    def test_unmatched_metadata_empty(self):
        self.value = copy.deepcopy(FIXTURES['unmatched'])
        self.value['taa']['first_unmatched']['newest']['frame'] = 3
        self.rejected()

    def synthetic_run(self):
        rows, startup, dll = copy.deepcopy(PRIOR)
        rows[0]['version'] = startup[0]['version'] = '1.18.3'
        empty = copy.deepcopy(FIXTURES['unmatched'])
        empty['publication_attempts'] = empty['published'] = 0
        empty['taa'] = copy.deepcopy(empty['depth'])
        for row in rows[4:-1]:
            row['requests'] = copy.deepcopy(empty)
            q = row['requests'];q['publication_attempts'] = q['published'] = row['frames']
            q['taa']['calls'] = q['taa']['matched'] = row['frames']
        return rows, startup, dll

    def test_versioned_full_reviewer(self):
        result = review_run(*self.synthetic_run())
        self.assertEqual(result['version'], '1.18.3')
        self.assertEqual(result['requests']['published'], result['frames'])
        self.assertFalse(result['temporal_quality_verified'])

    def test_request_coverage(self):
        rows, startup, dll = self.synthetic_run()
        q = rows[-2]['requests'];q['published'] -= 1;q['publication_attempts'] -= 1
        with self.assertRaises(ValueError):
            review_run(rows, startup, dll)

    def test_fault_is_never_acceptance(self):
        rows, startup, dll = self.synthetic_run()
        rows[-2]['requests'] = self.value;rows[-2]['fault'] = 6;rows[-1]['healthy'] = False
        with self.assertRaises(ValueError):
            review_run(rows, startup, dll)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--native', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--prior-run', type=Path, required=True)
    args, rest = parser.parse_known_args()
    args.output.mkdir(parents=True, exist_ok=True)
    for mode in MODES:
        raw = subprocess.run([str(args.native.resolve()), 'requests-' + mode], check=True, capture_output=True).stdout
        FIXTURES[mode] = json.loads(raw)
        with (args.output / (mode + '.json')).open('xb') as stream:
            stream.write(raw)
    log, = args.prior_run.glob('inputs-*.jsonl')
    rows = [json.loads(line) for line in log.read_text(encoding='utf-8').splitlines()]
    h = rows[0]
    startup = args.prior_run / f"startup-{h['pid']}-{h['creation']}.jsonl"
    PRIOR = rows, [json.loads(line) for line in startup.read_text(encoding='utf-8').splitlines()], h['dll_sha256']
    unittest.main(argv=[sys.argv[0], *rest])
