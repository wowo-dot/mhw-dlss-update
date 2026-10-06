"""Independently check diagnostic history decisions from the compiled fixture."""
import copy
import json
from pathlib import Path
import subprocess
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from continuous_history import review

class HistoryComparisonTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        exe=Path(__file__).resolve().parents[1]/'build/Release/history_comparison_tests.exe'
        cls.fixture=json.loads(subprocess.check_output([str(exe),'--json'],text=True))
    def setUp(self):self.h=copy.deepcopy(self.fixture)
    def rejected(self):
        with self.assertRaises(ValueError):review(self.h,121,4,'distance-only-comparison-v1')
    def test_cpp_output(self):
        r=review(self.h,121,4,'distance-only-comparison-v1')
        self.assertEqual(r['displacement_only_resets_suppressed'],117)
        self.assertEqual(r['control_resets'],121)
        self.assertEqual(r['counts']['frame_gap'],3)
    def test_control_policy_required(self):
        with self.assertRaises(ValueError):review(self.h,121,4,'camera-displacement-v1')
    def test_false_suppressed_reason(self):
        self.h['first_displacement_suppressed']['control_reasons']|=2;self.rejected()
    def test_actual_frame_gap_cannot_be_suppressed(self):
        self.h['first_displacement_suppressed']['next']['frame']+=1;self.rejected()
    def test_forged_distance(self):
        self.h['latest_displacement_suppressed']['camera_distance']=1;self.rejected()
    def test_false_suppression_marker(self):
        self.h['latest_displacement_suppressed']['displacement_only_suppressed']=False;self.rejected()
    def test_suppressed_count_bound(self):
        self.h['displacement_only_resets_suppressed']=118;self.rejected()
    def test_control_count(self):
        self.h['control_resets']-=1;self.rejected()
    def test_diagnostic_marker(self):
        self.h['diagnostic_only']=False;self.rejected()
    def test_empty_suppression(self):
        self.h.update(displacement_only_resets_suppressed=0,control_resets=4,first_displacement_suppressed=None,latest_displacement_suppressed=None)
        # Avoided examples still prove suppression: the aggregate cannot omit it.
        self.rejected()
    def test_false_actual_reset(self):
        self.h['latest']['control_reasons']=1024;self.rejected()

if __name__=='__main__':unittest.main()
