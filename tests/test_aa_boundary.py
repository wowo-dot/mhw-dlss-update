import copy
import json
from pathlib import Path
import sys
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from arm_aa_boundary import HOST, parse, validate


class Cases(unittest.TestCase):
    def rows(self):
        return [dict(event="start", schema=1, version="0.1.0", host_sha256=HOST, host_version="0.0.0.0",
                     experiment="independent-aa-post-queue-boundary", sample_limit=8, game_data_writes=0,
                     private_gpu_commands=0, sdk_used=False, pid=7, creation=8, base=0x140000000, dll_sha256="a"*64),
                dict(event="ready", reserved=True, hooks_installed=False)]

    def test_ready(self):
        rows = parse("\n".join(map(json.dumps, self.rows())).encode())
        validate(rows, 7, 8, "A"*64)

    def test_stale_generation_or_dll(self):
        for pid, creation, sha in ((6, 8, "a"*64), (7, 9, "a"*64), (7, 8, "b"*64)):
            with self.assertRaises(ValueError):
                validate(self.rows(), pid, creation, sha)

    def test_no_repeat_arm_or_finished_stop(self):
        rows = self.rows() + [dict(event="armed")]
        with self.assertRaises(ValueError):
            validate(rows, 7, 8, "a"*64)
        validate(rows, 7, 8, "a"*64, stop=True)
        for event in ("stopped", "failed"):
            with self.assertRaises(ValueError):
                validate(rows + [dict(event=event)], 7, 8, "a"*64, stop=True)

    def test_reservation_required(self):
        for key, value in (("reserved", False), ("hooks_installed", True)):
            rows = self.rows()
            rows[1][key] = value
            with self.assertRaises(ValueError):
                validate(rows, 7, 8, "a"*64)

    def test_wrong_scope_and_identity(self):
        for key, value in (("schema", 2), ("version", "1.18.4"), ("host_sha256", "b"*64),
                           ("private_gpu_commands", 1), ("sdk_used", True), ("sample_limit", 100), ("pid", 0)):
            rows = copy.deepcopy(self.rows())
            rows[0][key] = value
            with self.assertRaises(ValueError):
                parse("\n".join(map(json.dumps, rows)).encode())


if __name__ == "__main__":
    unittest.main()
