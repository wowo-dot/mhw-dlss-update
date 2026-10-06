"""Corruption checks against an explicitly supplied, local continuous-run log."""
import argparse
import copy
import json
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from arm_continuous_dlaa import parse
from review_continuous_dlaa import review

FIXTURE = None


class ContinuousReviewTests(unittest.TestCase):
    def setUp(self):
        if FIXTURE is None:
            self.skipTest('supply --run with an archived local run')
        self.rows, self.startup, self.dll = copy.deepcopy(FIXTURE)

    def rejected(self):
        with self.assertRaises(ValueError):
            review(self.rows, self.startup, self.dll)

    def test_complete_and_limited_claims(self):
        result = review(self.rows, self.startup, self.dll)
        self.assertTrue(result['continuous_path_telemetry_verified'])
        for key in ('live_pixel_readback_verified', 'temporal_quality_verified',
                    'full_transition_stability_verified'):
            self.assertFalse(result[key])

    def test_wrong_build(self):
        self.dll = '0' * 64
        self.rejected()

    def test_wrong_startup_generation(self):
        self.rows[0]['creation'] += 1
        self.rejected()

    def test_runtime_fault(self):
        self.rows[-2]['fault'] = 1
        self.rejected()

    def test_incomplete_gpu_work(self):
        self.rows[-2]['completed_value'] -= 1
        self.rejected()

    def test_unrestored_jitter(self):
        self.rows[-2]['jitter_restorations'] -= 1
        self.rejected()

    def test_pending_callbacks(self):
        self.rows[-2]['pending'] = 1
        self.rejected()

    def test_open_hooks(self):
        self.rows[-2]['hooks_disabled'] = False
        self.rejected()

    def test_pending_recording(self):
        self.rows[-2]['pending_private_recording'] = True
        self.rejected()

    def test_failed_native_handoff(self):
        for key in ('copied', 'restored'):
            with self.subTest(key=key):
                saved = self.rows[-2]['sdk'][key]
                self.rows[-2]['sdk'][key] = False
                self.rejected()
                self.rows[-2]['sdk'][key] = saved

    def test_wrong_motion_parameters(self):
        self.rows[-2]['sdk']['mv_scale'] = [2560, 1440]
        self.rejected()

    def test_counter_rollback(self):
        self.rows[-2]['frames'] = self.rows[-3]['frames'] - 1
        self.rejected()

    def test_truncated_stop(self):
        self.rows.pop()
        self.rejected()


if __name__ == '__main__':
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--run', type=Path, required=True)
    args, rest = ap.parse_known_args()
    logs = list(args.run.glob('inputs-*.jsonl'))
    if len(logs) != 1:
        ap.error('expected exactly one continuous log')
    rows = parse(logs[0].read_bytes())
    h = rows[0]
    startup_path = args.run / f"startup-{h['pid']}-{h['creation']}.jsonl"
    startup = [json.loads(line) for line in startup_path.read_text(encoding='utf-8').splitlines()]
    FIXTURE = rows, startup, h['dll_sha256']
    unittest.main(argv=[sys.argv[0], *rest])
