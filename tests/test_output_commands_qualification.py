"""Reject malformed discovery receipts while retaining both producer forms."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from continuous_output_commands import qualification


class DependencyQualificationTests(unittest.TestCase):
    def test_historical_whole_plan_discovery_and_order(self):
        for code, branch in ((12, 10), (13, 8)):
            qualification(code, 17, 0, [branch, 0, 0, 2**32-1, 0, 0, 0, 2])

    def test_failed_draw_matches_v42_producer(self):
        # Recorded broad-run shape: filter phase15/order4100, selected packet,
        # unavailable queue index, no word read, five overlay draws in the plan.
        qualification(12, 17, 0, [10, 0x78001004, 3355561680, 2**32-1, 0, 0, 0, 5])

    def test_draw_receipt_fields_must_match_producer(self):
        for index, value in ((0, 8), (1, 0), (1, 1), (1, 0x70001004),
                             (1, 0x78000064), (1, 0x7ffffff1), (2, 0),
                             (3, 0), (4, 11), (5, 1), (6, 1), (7, 0), (7, 9)):
            with self.subTest(index=index, value=value):
                gate=[10, 0x78001004, 3355561680, 2**32-1, 0, 0, 0, 5]
                gate[index]=value
                with self.assertRaises(ValueError):
                    qualification(12, 17, 0, gate)

    def test_only_discovery_reports_draw_identity(self):
        gate=[10, 0x78001004, 3355561680, 2**32-1, 0, 0, 0, 5]
        for code, reason, detail in ((13,17,0), (14,17,0), (12,6,0),
                                    (12,17,1), ((4<<16)|12,17,0)):
            with self.subTest(code=code, reason=reason, detail=detail):
                with self.assertRaises(ValueError):
                    qualification(code, reason, detail, gate)

    def test_dof_and_order_keep_absent_packet_checks(self):
        qualification(14, 17, 0, [10, 0x78006595, 0, 2**32-1, 0, 0, 0, 1])
        for code, gate in ((13,[8,0x78001004,100,2**32-1,0,0,0,2]),
                           (14,[10,0x78006595,100,2**32-1,0,0,0,1])):
            with self.assertRaises(ValueError):
                qualification(code, 17, 0, gate)


if __name__ == '__main__':
    unittest.main()
