from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from fg_game import log_path, request_multiplier


def rows(version='0.8.0', armed=True, stopped=False):
    r = [dict(event='start', version=version, experiment='frame-generation-game', creation=1234)]
    if armed:
        r.append(dict(event='fg_armed'))
    if stopped:
        r.append(dict(event='fg_stopped'))
    return r


class FgGameTests(unittest.TestCase):
    def test_log_path_ignores_side_logs(self):
        with tempfile.TemporaryDirectory() as d:
            for name in ('reflex-frames-1-2.jsonl', 'reflex-frames-1-2.jsonl.boot.jsonl',
                         'reflex-frames-1-2.jsonl.crash.jsonl'):
                (Path(d) / name).write_text('')
            self.assertEqual(log_path(d).name, 'reflex-frames-1-2.jsonl')

    def test_multiplier_requests_are_sequenced_and_generation_checked(self):
        with tempfile.TemporaryDirectory() as d:
            log = Path(d) / 'reflex-frames-1-2.jsonl'
            self.assertEqual(request_multiplier(rows(), log, 4)['request'], 1)
            self.assertEqual(request_multiplier(rows(), log, 3)['request'], 2)
            self.assertEqual(Path(f'{log}.mfg-1').read_text(), '1234 4\n')
            self.assertEqual(Path(f'{log}.mfg-2').read_text(), '1234 3\n')
            self.assertFalse(list(Path(d).glob('*.tmp')))

    def test_multiplier_refusals(self):
        with tempfile.TemporaryDirectory() as d:
            log = Path(d) / 'reflex-frames-1-2.jsonl'
            for args in ((rows(), 1), (rows(), 9), (rows(version='0.7.0'), 4), (rows(armed=False), 4),
                         (rows(stopped=True), 4)):
                with self.assertRaises(ValueError):
                    request_multiplier(args[0], log, args[1])
            self.assertFalse(list(Path(d).iterdir()))


if __name__ == '__main__':
    unittest.main()
