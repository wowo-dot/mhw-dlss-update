"""Cross-channel identity/lifetime negatives; SDK acceptance uses existing suites."""
import copy
import sys
import unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parents[1] / 'tools'))
from reflex_temporal import cross_links, HOST_SHA


def fixture():
    start = dict(event='start', schema=3, version='0.5.0', experiment='reflex-temporal-coexistence',
                 mode=0, target_mode=1, upscaling=True, temporal_version='1.25.44',
                 pid=10, creation=20, dll_sha256='a' * 64, host_sha256=HOST_SHA)
    def link(phase):
        n = ('prepared', 'armed', 'drained').index(phase)
        return dict(event='temporal_coexistence', phase=phase, version='1.25.44', device=30,
                    input_log='inputs-10-20.jsonl', startup_log='startup-10-20.jsonl', prepared=True,
                    installed=n > 0, finished=n == 2, quiescent=n == 2, NGX_shutdown_deferred=True)
    rows = [start, dict(event='device_checked', device=30), link('prepared'), dict(event='frames_ready'),
            link('armed'), dict(event='frame_hooks_installed'), dict(event='frames_armed'), link('drained'),
            dict(event='frame_finished'), dict(event='presentation_cleanup'), dict(event='sdk_shutdown'),
            dict(event='finished', temporal_drained=True)]
    paired = dict(start, schema=1, version='1.25.44', base=40)
    return rows, [paired], [dict(paired), dict(event='graphics', device=30)]


def stamp_fixture():
    data = fixture(); rows = data[0]; rows[0]['version'] = '0.5.1'
    rows[5].update(engine=50, renderer=60, backend=70)
    rows.insert(5, dict(event='frame_anchor_check', policy='bounded-worker-stamp-v1', passed=True,
                        reason='', hooks_created=False, device=30, engine=50, renderer=60, backend=70,
                        attempts=1, attempt_limit=64, timeout_ms=100, elapsed_ms=0,
                        first_status=0, final_status=0, first=[100,99,0,100,0], last=[100,99,0,100,0]))
    return data


class CrossChannelTests(unittest.TestCase):
    def test_matching_join_is_structurally_valid(self):
        cross_links(*fixture(), 'a' * 64)

    def test_other_process_or_binary_rejected(self):
        for channel in (1, 2):
            for key, value in (('pid', 11), ('creation', 21), ('dll_sha256', 'b' * 64),
                               ('host_sha256', 'b' * 64), ('version', '1.25.43'), ('base', 41)):
                data = copy.deepcopy(fixture()); data[channel][0][key] = value
                with self.subTest(channel=channel, key=key), self.assertRaises(ValueError):
                    cross_links(*data, 'a' * 64)

    def test_other_device_rejected(self):
        data = fixture(); data[2][1]['device'] = 31
        with self.assertRaises(ValueError): cross_links(*data, 'a' * 64)

    def test_evaluation_on_other_device_rejected(self):
        data = fixture(); data[1].append(dict(event='continuous', frames=1, sdk=dict(device=31)))
        with self.assertRaises(ValueError): cross_links(*data, 'a' * 64)

    def test_missing_or_unproven_drain_rejected(self):
        for key, value in (('finished', False), ('quiescent', False), ('installed', False),
                           ('NGX_shutdown_deferred', False), ('input_log', '../other.jsonl')):
            data = fixture(); data[0][7][key] = value
            with self.subTest(key=key), self.assertRaises(ValueError): cross_links(*data, 'a' * 64)

    def test_shutdown_before_temporal_drain_rejected(self):
        data = fixture(); data[0][7], data[0][10] = data[0][10], data[0][7]
        with self.assertRaises(ValueError): cross_links(*data, 'a' * 64)

    def test_final_false_cleanup_rejected(self):
        data = fixture(); data[0][-1]['temporal_drained'] = False
        with self.assertRaises(ValueError): cross_links(*data, 'a' * 64)

    def test_legacy_or_wrong_mode_cannot_masquerade(self):
        for key, value in (('schema', 2), ('version', '0.4.2'), ('upscaling', False), ('target_mode', 0)):
            data = fixture(); data[0][0][key] = value
            with self.subTest(key=key), self.assertRaises(ValueError): cross_links(*data, 'a' * 64)

    def test_new_stamp_contract_and_coherent_retry(self):
        cross_links(*stamp_fixture(), 'a' * 64)
        data = stamp_fixture(); a = data[0][5]
        a.update(attempts=2, elapsed_ms=1, first_status=1, first=[100,99,0,101,1])
        cross_links(*data, 'a' * 64)

    def test_new_stamp_contract_cannot_skip_or_weaken_admission(self):
        for key, value in (('passed', False), ('hooks_created', True), ('device', 31), ('renderer', 61),
                           ('attempts', 0), ('attempts', 65), ('attempt_limit', 128), ('timeout_ms', 200),
                           ('elapsed_ms', 151), ('final_status', 1), ('first_status', 2),
                           ('last', [100,99,0,101,1]), ('first', [100,99,0,101,1])):
            data = stamp_fixture(); data[0][5][key] = value
            with self.subTest(key=key), self.assertRaises(ValueError): cross_links(*data, 'a' * 64)

    def test_retry_does_not_admit_missing_or_invalid_initial_stamp(self):
        for status, first in ((2, [100,99,0,0,0]), (3, [100,99,2,100,2]), (1, [100,99,0,100,0])):
            data = stamp_fixture(); data[0][5].update(attempts=2, first_status=status, first=first)
            with self.subTest(status=status), self.assertRaises(ValueError): cross_links(*data, 'a' * 64)

    def test_new_stamp_receipt_required_before_hook_install(self):
        data = stamp_fixture(); del data[0][5]
        with self.assertRaises(ValueError): cross_links(*data, 'a' * 64)
        data = stamp_fixture(); data[0][5], data[0][6] = data[0][6], data[0][5]
        with self.assertRaises(ValueError): cross_links(*data, 'a' * 64)


if __name__ == '__main__': unittest.main()
