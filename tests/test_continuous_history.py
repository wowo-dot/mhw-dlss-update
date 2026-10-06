"""Review actual native fixture output and reject corrupted reason evidence."""
import copy
import json
from pathlib import Path
import subprocess
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from continuous_history import review


class HistoryReviewTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        exe = Path(__file__).resolve().parents[1] / 'build/Release/continuous_history_tests.exe'
        cls.fixture = json.loads(subprocess.check_output([str(exe), '--json'], text=True))

    def setUp(self):
        self.h = copy.deepcopy(self.fixture)

    def rejected(self):
        with self.assertRaises(ValueError):
            review(self.h, 10003, 103)

    def test_cpp_output(self):
        result = review(self.h, 10003, 103, 'camera-displacement-v1')
        self.assertEqual(result['counts']['camera_displacement'], 102)
        self.assertEqual(result['longest_retained_streak'], 99)
        self.assertEqual(result['translation_resets_avoided'], 1)

    def test_no_frames(self):
        h = dict(schema=2, policy='camera-displacement-v1', evaluations=0, resets=0,
                 retained=0, retained_streak=0, longest_retained_streak=0,
                 counts={key: 0 for key in self.h['counts']}, first=[], latest=None,
                 translation_resets_avoided=0, first_avoided=None, latest_avoided=None)
        review(h, 0, 0)

    def test_wrong_count(self):
        self.h['counts']['camera_displacement'] = 104
        self.rejected()

    def test_missing_reason(self):
        del self.h['counts']['projection']
        self.rejected()

    def test_missing_example(self):
        self.h['first'].pop()
        self.rejected()

    def test_false_reason(self):
        self.h['latest']['reasons'] = 1 << 7
        self.rejected()

    def test_changed_matrix(self):
        self.h['latest']['next']['view_bits'] = self.h['latest']['previous']['view_bits'][:]
        self.rejected()

    def test_false_projection_mask(self):
        self.h['first'][7]['example']['projection_mask'] = 0
        self.rejected()

    def test_nonfinite_matrix(self):
        self.h['latest']['next']['view_bits'][0] = 0x7fc00000
        self.rejected()

    def test_time_backwards(self):
        self.h['latest']['next']['microseconds'] = 1
        self.rejected()

    def test_wrong_totals(self):
        self.h['retained'] += 1
        self.rejected()

    def test_invented_streak(self):
        self.h['longest_retained_streak'] = 10001
        self.rejected()

    def test_wrong_build_policy(self):
        with self.assertRaises(ValueError):
            review(self.h, 10003, 103, 'v1.18.0-unchanged')

    def test_invented_camera_distance(self):
        self.h['latest_avoided']['camera_distance'] += 1
        self.rejected()

    def test_false_avoided_reset(self):
        self.h['latest_avoided']['reasons'] = 1 << 10
        self.rejected()

    def test_invented_avoided_count(self):
        self.h['translation_resets_avoided'] = 10003
        self.rejected()

    def test_nonrigid_camera(self):
        self.h['latest_avoided']['next']['view_bits'][0] = 0x40000000
        self.rejected()


if __name__ == '__main__':
    unittest.main()
