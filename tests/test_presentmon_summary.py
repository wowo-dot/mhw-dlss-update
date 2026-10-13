"""presentmon_summary: rates, lows, drops, slices, menu segments and missing columns on owned CSVs."""
import datetime as dt
import os
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'perf'))
import presentmon_summary as pm

V2 = ('Application,ProcessID,SwapChainAddress,PresentRuntime,FrameType,CPUStartDateTime,MsBetweenPresents,'
      'MsBetweenDisplayChange,MsUntilDisplayed,MsCPUBusy,MsGPUBusy,MsPCLatency\n')
START = dt.datetime(2026, 10, 13, 5, 0, 0)


def row(t, present, display, until='3.0', chain='0x1', app='MonsterHunterWorld.exe', latency='12.5'):
    # PresentMon's --date_time format: hours not padded, nanoseconds.
    stamp = f'{t.year}-{t.month}-{t.day} {t.hour}:{t.minute:02d}:{t.second:02d}.{t.microsecond * 1000:09d}'
    return f'{app},1,{chain},DXGI,Application,{stamp},{present},{display},{until},1.5,3.0,{latency}\n'


def frames(start, count, interval_ms):
    """`count` presents `interval_ms` apart from `start`."""
    return ''.join(row(start + dt.timedelta(milliseconds=i * interval_ms), f'{interval_ms}', f'{interval_ms}')
                   for i in range(count))


class Summary(unittest.TestCase):
    def write(self, text, name='capture-MonsterHunterWorld-1.csv', written=START):
        """The file's modification time is `written` (PresentMon closes the
        file after its last row; the summary corrects the clock by it)."""
        folder = Path(tempfile.mkdtemp())
        path = folder / name
        path.write_text(text, encoding='utf-8')
        os.utime(path, (written.timestamp(), written.timestamp()))
        return folder, path

    def test_rates_and_lows(self):
        # 99 frames at 4 ms and one 20 ms hitch, plus another process's and
        # an overlay swap chain's presents, which are ignored.
        text = V2 + frames(START, 99, 4.0) + row(START + dt.timedelta(milliseconds=400), '20.0', '20.0')
        text += row(START, '1.0', '1.0', app='Other.exe') + row(START, '50.0', '50.0', chain='0x2')
        folder, _ = self.write(text)
        [r] = [x for p in pm.captures([str(folder)]) for x in pm.summarise(p)]
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
        self.assertEqual(r['first'], '2026-10-13 05:00:00.000')
        self.assertIn('capture-MonsterHunterWorld-1', pm.table([r]))

    def test_dropped_and_na(self):
        text = V2 + row(START, '4.0', '4.0') + row(START, '4.0', 'NA', until='NA', latency='NA') + row(START, '4.0', '8.0')
        _, path = self.write(text)
        [r] = pm.summarise(path)
        self.assertEqual(r['dropped'], 1)
        self.assertAlmostEqual(r['displayed_fps'], 2 * 1000 / 12)
        self.assertEqual(r['pc_latency_ms'], 12.5)

    def test_slices_and_window(self):
        # 2 s at 4 ms, then 2 s at 8 ms.
        text = V2 + frames(START, 500, 4.0) + frames(START + dt.timedelta(seconds=2), 250, 8.0)
        _, path = self.write(text)
        whole, first, second = pm.summarise(path, slice_s=2)
        self.assertEqual(whole['presents'], 750)
        self.assertEqual((first['presents'], first['displayed_fps']), (500, 250.0))
        self.assertEqual((second['presents'], second['displayed_fps']), (250, 125.0))
        self.assertTrue(second['label'].endswith('05:00:02'))
        [window] = pm.summarise(path, window=('05:00:02', '05:00:03'))
        self.assertEqual((window['presents'], window['displayed_fps']), (125, 125.0))

    def test_menu_segments(self):
        # DLAA for 10 s at 5 ms, then Quality from 05:00:10 at 4 ms; the
        # first 5 s after each change are skipped.
        text = V2 + frames(START, 2000, 5.0) + frames(START + dt.timedelta(seconds=10), 2500, 4.0)
        folder, path = self.write(text)
        log = folder / 'mhw_dlss.log'
        log.write_text('04:59:00.000 INFO menu: dlss_mode = balanced (applied now)\n'
                       '05:00:10.000 INFO menu: dlss_mode = quality (applied now)\n'
                       '05:00:11.000 INFO menu: mhw_dlss.ini saved with dlss_mode = quality\n', encoding='utf-8')
        whole, start, quality = pm.summarise(path, log=log)
        self.assertEqual(whole['presents'], 4500)
        self.assertEqual(start['label'], 'capture-MonsterHunterWorld-1 05:00:00 start')
        self.assertEqual((start['presents'], start['displayed_fps']), (1000, 200.0))
        self.assertEqual(quality['label'], 'capture-MonsterHunterWorld-1 05:00:10 dlss_mode=quality')
        self.assertEqual((quality['presents'], quality['displayed_fps']), (1250, 250.0))

    def test_presentmon_26_columns_and_clock(self):
        # 2.6.0's names, and its clock three hours ahead of local time: the
        # rows say 08:00, the file was closed at 05:00:02 local.
        header = ('Application,ProcessID,SwapChainAddress,CPUStartDateTime,FrameTime,CPUBusy,GPUBusy,'
                  'DisplayLatency,DisplayedTime,AllInputToPhotonLatency\n')
        body = ''.join(f'MonsterHunterWorld.exe,1,0x1,2026-10-13 8:00:0{i // 250}.{(i % 250) * 4:03d}000000,4.0,1.0,3.5,'
                       f'{"NA" if i == 7 else "6.0"},{"NA" if i == 7 else "4.0"},NA\n' for i in range(500))
        folder, path = self.write(header + body, written=START + dt.timedelta(seconds=2, milliseconds=300))
        log = folder / 'mhw_dlss.log'
        log.write_text('05:00:01.000 INFO menu: dlss_mode = quality (applied now)\n', encoding='utf-8')
        whole, start, quality = pm.summarise(path, log=log, settle=0)
        self.assertEqual(whole['first'], '2026-10-13 05:00:00.000')
        self.assertEqual((whole['presents'], whole['presented_fps'], whole['displayed_fps']), (500, 250.0, 250.0))
        self.assertEqual((whole['cpu_busy_ms'], whole['gpu_busy_ms'], whole['display_latency_ms']), (1.0, 3.5, 6.0))
        self.assertEqual(whole['dropped'], 1)
        self.assertIsNone(whole['input_to_photon_ms'])
        self.assertEqual((start['presents'], quality['presents']), (250, 250))

    def test_missing_columns(self):
        _, path = self.write('Application,SwapChainAddress,MsBetweenPresents\nMonsterHunterWorld.exe,0x1,5\n')
        [r] = pm.summarise(path, slice_s=1)
        self.assertEqual(r['presents'], 1)
        self.assertEqual(r['presented_fps'], 200.0)
        self.assertIsNone(r['gpu_busy_ms'])
        self.assertNotIn('dropped', r)
        self.assertNotIn('first', r)

    def test_empty_capture(self):
        _, path = self.write(V2)
        self.assertEqual(pm.summarise(path), [{'label': 'capture-MonsterHunterWorld-1', 'presents': 0}])


if __name__ == '__main__':
    unittest.main()
