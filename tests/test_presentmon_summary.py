"""presentmon_summary: rates, percentiles, lows, drops and missing columns on owned CSVs."""
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'perf'))
import presentmon_summary as pm

V2 = ('Application,ProcessID,SwapChainAddress,PresentRuntime,FrameType,CPUStartTime,MsBetweenPresents,'
      'MsBetweenDisplayChange,MsUntilDisplayed,MsCPUBusy,MsGPUBusy,MsPCLatency\n')


def row(i, present, display, until='3.0', chain='0x1', app='MonsterHunterWorld.exe', latency='12.5'):
    return f'{app},1,{chain},DXGI,Application,2026-10-13 05:00:{i:02d},{present},{display},{until},1.5,3.0,{latency}\n'


class Summary(unittest.TestCase):
    def write(self, text):
        folder = Path(tempfile.mkdtemp())
        path = folder / 'capture-1.csv'
        path.write_text(text, encoding='utf-8')
        return folder, path

    def test_rates_and_lows(self):
        # 99 frames at 4 ms and one 20 ms hitch, plus another process's and
        # an overlay swap chain's presents, which are ignored.
        text = V2 + ''.join(row(i % 60, '4.0', '4.0') for i in range(99)) + row(59, '20.0', '20.0')
        text += row(1, '1.0', '1.0', app='Other.exe') + row(2, '50.0', '50.0', chain='0x2')
        folder, _ = self.write(text)
        [r] = [pm.summarise(p) for p in pm.captures([str(folder)])]
        self.assertEqual(r['presents'], 100)
        self.assertEqual(r['swap_chains'], 2)
        self.assertAlmostEqual(r['duration_s'], 0.416)
        self.assertAlmostEqual(r['presented_fps'], 100 * 1000 / 416)
        self.assertEqual(r['display_ms']['p50'], 4.0)
        self.assertEqual(r['display_ms']['p99'], 4.0)
        self.assertEqual(r['display_ms']['max'], 20.0)
        self.assertEqual(r['low_1pct_fps'], 250.0)
        self.assertEqual(r['low_0_1pct_fps'], 50.0)
        self.assertEqual(r['gpu_busy_ms'], 3.0)
        self.assertEqual(r['pc_latency_ms'], 12.5)
        self.assertEqual(r['dropped'], 0)
        self.assertEqual(r['frame_types'], {'Application': 100})
        self.assertIn('capture-1.csv', pm.table([r]))

    def test_dropped_and_na(self):
        text = V2 + row(0, '4.0', '4.0') + row(1, '4.0', 'NA', until='NA', latency='NA') + row(2, '4.0', '8.0')
        _, path = self.write(text)
        r = pm.summarise(path)
        self.assertEqual(r['dropped'], 1)
        self.assertAlmostEqual(r['displayed_fps'], 2 * 1000 / 12)
        self.assertEqual(r['pc_latency_ms'], 12.5)

    def test_missing_columns(self):
        _, path = self.write('Application,SwapChainAddress,MsBetweenPresents\nMonsterHunterWorld.exe,0x1,5\n')
        r = pm.summarise(path)
        self.assertEqual(r['presents'], 1)
        self.assertEqual(r['presented_fps'], 200.0)
        self.assertIsNone(r['gpu_busy_ms'])
        self.assertNotIn('dropped', r)

    def test_empty_capture(self):
        _, path = self.write(V2)
        self.assertEqual(pm.summarise(path), {'file': 'capture-1.csv', 'presents': 0})


if __name__ == '__main__':
    unittest.main()
