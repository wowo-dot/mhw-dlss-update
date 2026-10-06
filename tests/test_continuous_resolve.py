"""Corruption and concurrent-snapshot checks for qualified TAA replacement receipts."""
import copy
import json
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from arm_continuous_dlaa import parse
from continuous_resolve import review, COUNTERS
from review_continuous_dlaa import review as review_run
import test_continuous_retention as retention_fixture


def receipt(frames=100):
    return dict(schema=1, policy='qualified-resolve-no-native-retry-v1', exclusive_mode=False,
                candidates=frames+5, pending=0, unclaimed_native=5, claimed=frames,
                reconstructed=frames, unfiltered=0, recovered=0, rejected=0, unrecoverable=0,
                sdk_commands=frames, shader_checks=frames, shader_rejected=0)


def fixture():
    # Synthetic resolve metadata over a private archived v1.18.3 run. This is
    # parser regression evidence, never evidence of a live v1.19.0 bypass.
    rows, startup, dll = retention_fixture.RetentionTests().synthetic()
    h = rows[0]
    h.update(version='1.19.0', experiment='native-temporal-resolve',
             replacement_scope='qualified-native-resolve', exclusive_mode=False)
    h.pop('native_taa_forwarded', None)
    startup[0]['version'] = h['version']
    for s in rows[4:-1]:
        s['resolve'] = receipt(s['frames'])
    return rows, startup, dll


class ResolveTests(unittest.TestCase):
    def setUp(self):
        self.s = dict(frames=100, fault=0, sdk=dict(evaluations=100), resolve=receipt())

    def test_qualified_scope_reported(self):
        r = review(self.s, final=True)
        self.assertEqual(r['unclaimed_native'], 5)
        self.assertFalse(r['exclusive_mode'])
        self.assertFalse(r['whole_renderer_aa_coverage_verified'])

    def test_counter_types_ranges_and_missing(self):
        for key in COUNTERS:
            for value in (True, -1, 2**64, 1.5, None):
                s = copy.deepcopy(self.s);s['resolve'][key] = value
                with self.subTest(key=key, value=value), self.assertRaises(ValueError): review(s)

    def test_partitions_and_evaluation_cannot_disagree(self):
        for key in COUNTERS:
            s = copy.deepcopy(self.s);s['resolve'][key] += 1
            if key == 'shader_checks': continue  # Some qualified shaders fail later admission.
            with self.subTest(key=key), self.assertRaises(ValueError): review(s)
        self.s['sdk']['evaluations'] -= 1
        with self.assertRaises(ValueError): review(self.s)

    def test_inflight_publication_allowed_only_live(self):
        self.s['frames'] += 1;self.s['sdk']['evaluations'] += 1
        self.s['resolve']['candidates'] += 1;self.s['resolve']['pending'] += 1
        self.s['resolve']['shader_checks'] += 1
        self.assertTrue(review(self.s)['qualified_bypass_telemetry_verified'])
        with self.assertRaises(ValueError): review(self.s, final=True)
        self.s['frames'] += 1;self.s['sdk']['evaluations'] += 1
        with self.assertRaises(ValueError): review(self.s)

    def test_stop_after_claim_unfiltered(self):
        r = self.s['resolve']
        for k in ('candidates', 'claimed', 'shader_checks', 'unfiltered'): r[k] += 1
        self.assertEqual(review(self.s, final=True)['unfiltered'], 1)

    def test_fault_outcomes_are_not_success(self):
        for key in ('recovered', 'rejected', 'unrecoverable'):
            s = copy.deepcopy(self.s);r = s['resolve']
            for k in ('candidates', 'claimed', 'shader_checks', key): r[k] += 1
            with self.subTest(key=key), self.assertRaises(ValueError): review(s)
        self.s['fault'] = 30
        with self.assertRaises(ValueError): review(self.s)

    def test_rollback_detected(self):
        prior = copy.deepcopy(self.s['resolve']);prior['unclaimed_native'] += 1
        with self.assertRaises(ValueError): review(self.s, prior)

    def test_no_bypass_is_not_sustained_success(self):
        self.s.update(frames=0, sdk=dict(evaluations=0), resolve=receipt(0))
        review(self.s)
        with self.assertRaises(ValueError): review(self.s, final=True)

    def test_policy_and_scope_required(self):
        for key, value in (('schema', 2), ('policy', 'other'), ('exclusive_mode', True)):
            s = copy.deepcopy(self.s);s['resolve'][key] = value
            with self.subTest(key=key), self.assertRaises(ValueError): review(s)

    def test_complete_synthetic_run(self):
        r = review_run(*fixture())
        self.assertEqual(r['resolve']['reconstructed'], r['frames'])
        self.assertGreater(r['frames'], 9)

    def test_header_scope_and_version(self):
        for key, value in (('version', '1.19.1'), ('experiment', 'continuous-dlaa-prototype'),
                           ('replacement_scope', 'all'), ('exclusive_mode', True),
                           ('retention', 'bounded-process-lifetime')):
            rows, startup, dll = fixture();rows[0][key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                parse((json.dumps(rows[0])+'\n').encode())
            with self.subTest(direct=key), self.assertRaises(ValueError): review_run(rows, startup, dll)

    def test_startup_version_and_missing_resolve(self):
        rows, startup, dll = fixture();startup[0]['version'] = '1.18.4'
        with self.assertRaises(ValueError): review_run(rows, startup, dll)
        rows, startup, dll = fixture();del rows[-2]['resolve']
        with self.assertRaises(KeyError): review_run(rows, startup, dll)


if __name__ == '__main__': unittest.main()
