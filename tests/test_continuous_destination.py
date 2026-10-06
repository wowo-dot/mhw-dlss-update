"""Destination acceptance boundaries, captured attachment changes, and receipts."""
from copy import deepcopy
import json
from pathlib import Path
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from continuous_depth import review, ATTACHMENT_MASK
from arm_continuous_dlaa import parse
from review_continuous_dlaa import review as review_run

FIXTURES = {}


def setUpModule():
    p = subprocess.run([str(ROOT / 'build/bin/Release/continuous_runtime_tests.exe'), 'depth-destination'],
                       capture_output=True, text=True, check=True)
    FIXTURES.update((v['case'], v) for v in map(json.loads, p.stdout.splitlines()))


class DestinationTests(unittest.TestCase):
    def setUp(self):
        self.s = deepcopy(FIXTURES['attachment-owner-resource'])

    def test_native_fixtures(self):
        self.assertEqual(len(FIXTURES), 20)
        for name, value in FIXTURES.items():
            with self.subTest(name=name):
                self.assertTrue(review(value)['destination']['destination_identity_verified'])

    def test_only_attachment_changes_admitted(self):
        for name in ('attachment-owner-resource', 'attachment-unavailable', 'attachment-read-failed'):
            self.assertEqual(review(FIXTURES[name])['destination']['attachment_only'], 1)
        for name in ('texture-resource', 'texture-flags', 'srv-and-descriptor', 'descriptor-only',
                     'optional-srv-read-failed', 'required-read-failed', 'invalid-container', 'attachment-and-texture-change'):
            self.assertEqual(review(FIXTURES[name])['destination']['rejected'], 1)

    def test_bounded_new_and_old_evidence(self):
        s = review(FIXTURES['bounded'])
        self.assertEqual((s['scene']['checks'], s['scene']['rejected']), (3000, 2000))
        self.assertEqual((s['destination']['accepted'], s['destination']['attachment_only']), (2000, 1000))
        self.assertEqual([row['ordinal'] for row in s['destination']['samples']], list(range(1, 9)) + [1000])

    def test_texture_change_cannot_hide_in_accepted_sample(self):
        row = self.s['depth_destination']['samples'][0]
        row['actual'][18] += 1
        row['difference'] |= 1 << 18
        with self.assertRaises(ValueError):
            review(self.s)

    def test_readability_change_cannot_hide(self):
        row = self.s['depth_destination']['samples'][0]
        row['actual'][20] = 1 - row['actual'][20]
        row['difference'] |= 1 << 20
        with self.assertRaises(ValueError):
            review(self.s)

    def test_counter_partition(self):
        self.s['depth_destination']['attachment_only'] = 0
        with self.assertRaises(ValueError):
            review(self.s)

    def test_missing_sample(self):
        self.s['depth_destination']['samples'] = []
        with self.assertRaises(ValueError):
            review(self.s)

    def test_cross_channel_evidence_disagreement(self):
        row = self.s['depth_destination']['samples'][0]
        row['actual'][4] += 1
        with self.assertRaises(ValueError):
            review(self.s)

    def test_immutable_receipt(self):
        old = review(self.s)
        self.s['depth_destination']['samples'][0]['at'][0] += 1
        with self.assertRaises(ValueError):
            review(self.s, old)

    def test_missing_channel_after_previous(self):
        old = review(self.s)
        self.s = deepcopy(FIXTURES['unchanged'])
        del self.s['depth_destination']
        with self.assertRaises(ValueError):
            review(self.s, old)

    def test_live_attachment_samples_replay(self):
        path = ROOT / 'local/native-aa-depth-eligibility-20260927/v1.21.4/final-33872.status.json'
        old = json.loads(path.read_text())
        for captured in old['depth_eligibility']['scene']['samples']:
            status = deepcopy(self.s)
            row = deepcopy(captured)
            row['ordinal'] = 1
            scene = status['depth_eligibility']['scene']
            scene['samples'] = [row]
            scene['differences'] = [int(bool(ATTACHMENT_MASK & (1 << i))) for i in range(21)]
            dest_row = {key: row[key] for key in ('ordinal', 'at', 'expected', 'actual', 'difference')}
            status['depth_destination']['samples'] = [dest_row]
            self.assertEqual(review(status)['destination']['attachment_only'], 1)

    def test_full_review_requires_new_header_and_channel(self):
        root = ROOT / 'local/native-aa-depth-eligibility-20260927/v1.21.4'
        rows = [json.loads(x) for x in (root / 'inputs-33872-134349990745610612.jsonl').read_bytes().splitlines()]
        startup = [json.loads(x) for x in (root / 'startup-33872-134349990745610612.jsonl').read_bytes().splitlines()]
        rows[0]['version'] = startup[0]['version'] = '1.21.5'
        with self.assertRaises(ValueError):
            parse((json.dumps(rows[0]) + '\n').encode())
        rows[0]['depth_destination'] = 'depth-destination-identity-v1'
        # Synthetic texture-mismatch trace tests parser integration only.
        for status in rows[4:-1]:
            scene = status['depth_eligibility']['scene']
            scene['differences'][18] = scene['mismatches']
            for row in scene['samples']:
                row['actual'][18] += 128
                row['difference'] |= 1 << 18
            status['depth_destination'] = dict(schema=1, policy='depth-destination-identity-v1',
                checks=scene['checks'], accepted=scene['passes'], rejected=scene['rejected'], attachment_only=0, samples=[])
        result = review_run(rows, startup, rows[0]['dll_sha256'])
        self.assertTrue(result['depth_eligibility']['destination']['destination_identity_verified'])
        del rows[-2]['depth_destination']
        with self.assertRaises(ValueError):
            review_run(rows, startup, rows[0]['dll_sha256'])


if __name__ == '__main__':
    unittest.main()
