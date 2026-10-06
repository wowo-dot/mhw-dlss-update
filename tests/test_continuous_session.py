"""Exercise bounded capture, timeline timing and stop behavior with owned files."""
import ctypes as c
from ctypes import wintypes as w
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from collect_continuous_session import Timeline, Process, collect, decode_status, validate_session, bounded_read, file_digest
from arm_continuous_dlaa import HOST, MAX_BYTES

DLL = 'a' * 64
UTC = '2026-09-26T13:00:00.000+00:00'
ARM = dict(pid=123, creation=456, stop=False, request_sent=True, requested_tick=1000)


def status(frame=1, fault=0):
    return dict(event='continuous', schema=1, active=fault == 0, frames=frame, fault=fault,
                signal_value=frame, completed_value=frame)


def encode(value):
    return (json.dumps(value) + '\n').encode('utf-8')


def header():
    return dict(event='start', schema=1, version='1.18.3', pid=123, creation=456, base=1234,
                host_sha256=HOST, host_version='0.0.0.0', dll_sha256=DLL,
                experiment='continuous-dlaa-prototype', frame_limit=None, graphics_api='D3D12')


class FakeProcess:
    def __init__(self):
        self.now = 1000
        self.running = True
        self.stops = []

    def tick(self):
        return self.now

    def alive(self):
        return self.running

    def stop(self):
        self.stops.append(self.now)
        return self.now


class SessionTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='session-test-', dir=Path(__file__).resolve().parents[1] / 'build')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.log = self.root / 'inputs.jsonl'
        self.log.write_bytes(encode(header()) + encode(dict(event='armed', installed=True)))
        self.status_path = self.log.with_suffix('.status.json')
        self.status_path.write_bytes(encode(status()))
        self.process = FakeProcess()

    def timeline(self, limit=1024 * 1024):
        result = Timeline(self.root / 'timeline.jsonl', 1000, limit)
        self.addCleanup(result.close)
        return result

    def terminal(self, fault=0):
        with self.log.open('ab') as stream:
            stream.write(encode(status(25, fault)))
            stream.write(encode(dict(event='stopped', armed=True, healthy=fault == 0, dll_pinned=True)))

    def run_collector(self, callback, minutes=1):
        return collect(self.log, self.root / 'capture', ARM, DLL, self.process, minutes, callback)

    def test_snapshot_clock_and_duplicate(self):
        t = self.timeline()
        raw = encode(status())
        self.assertEqual(t.snapshot(raw, 1001, 1004, UTC, UTC), 'saved')
        self.assertEqual(t.snapshot(raw, 1005, 1008, UTC, UTC), 'duplicate')
        row = json.loads(t.path.read_bytes())
        self.assertEqual(row['elapsed_ms'], 4)
        self.assertEqual(row['observed_begin_tick'], 1001)
        self.assertEqual(t.samples, 1)

    def test_partial_status_rejected(self):
        t = self.timeline()
        with self.assertRaises(ValueError):
            t.snapshot(b'{"event":', 1001, 1004, UTC, UTC)
        self.assertEqual(t.bytes, 0)

    def test_invalid_status_types(self):
        value = status(); value['frames'] = True
        with self.assertRaises(ValueError):
            decode_status(encode(value))

    def test_byte_limit_preserves_complete_lines(self):
        t = self.timeline(550)
        self.assertEqual(t.snapshot(encode(status()), 1000, 1001, UTC, UTC), 'saved')
        before = t.path.read_bytes()
        self.assertEqual(t.snapshot(encode(status(2)), 1001, 1002, UTC, UTC), 'full')
        self.assertEqual(t.path.read_bytes(), before)
        json.loads(before)

    def test_fault_retained_on_terminal(self):
        t = self.timeline()
        t.snapshot(encode(status(3)), 1000, 1001, UTC, UTC)
        t.snapshot(encode(status(4, 6)), 1001, 1002, UTC, UTC, 'terminal-log')
        t.snapshot(encode(status(5, 9)), 1002, 1003, UTC, UTC, 'terminal-log')
        self.assertEqual(t.first_fault['fault'], 6)
        self.assertEqual(t.first_fault['elapsed_ms'], 2)

    def test_backward_clock_rejected(self):
        t = self.timeline()
        with self.assertRaises(ValueError):
            t.snapshot(encode(status()), 1002, 1001, UTC, UTC)

    def test_scaled_fault_without_top_level_fault(self):
        t = self.timeline()
        value = status(2755)
        value['scaled_route'] = dict(fault=6, failure='effect native recipe')
        t.snapshot(encode(value), 1000, 1001, UTC, UTC)
        self.assertEqual(t.first_fault['fault'], 6)
        self.assertEqual(t.first_fault['component'], 'scaled_route')
        self.assertEqual(t.first_fault['faults']['runtime'], 0)

    def test_component_fault_type_rejected(self):
        value = status()
        value['scaled_route'] = dict(fault=False)
        with self.assertRaises(ValueError):
            decode_status(encode(value))

    def test_session_generation_and_dll(self):
        validate_session([header()], ARM, DLL)
        for change in (dict(pid=999), dict(creation=999), dict(stop=True), dict(request_sent=False)):
            with self.assertRaises(ValueError):
                validate_session([header()], dict(ARM, **change), DLL)
        with self.assertRaises(ValueError):
            validate_session([header()], ARM, 'b' * 64)

    def test_create_only(self):
        self.timeline()
        with self.assertRaises(FileExistsError):
            Timeline(self.root / 'timeline.jsonl', 1000)

    def test_file_limit(self):
        self.status_path.write_bytes(b'x' * (MAX_BYTES + 1))
        with self.assertRaises(ValueError):
            bounded_read(self.status_path)

    def test_duration_stop_and_terminal(self):
        def poll(_):
            self.process.now += 10000
            if self.process.stops:
                self.terminal()
        result = self.run_collector(poll)
        self.assertEqual(self.process.stops, [61000])
        self.assertEqual(result['stop_reason'], 'duration-limit')
        self.assertEqual(result['reason'], 'native-terminal')

    def test_manual_stop_file(self):
        def poll(_):
            self.process.now += 1000
            (self.root / 'capture/stop.request').touch()
            if self.process.stops:
                self.terminal()
        result = self.run_collector(poll)
        self.assertEqual(result['stop_reason'], 'requested')
        self.assertEqual(len(self.process.stops), 1)

    def test_custom_timeline_limit_still_stops_before_partial_line(self):
        def poll(_):
            self.process.now += 1000
            self.status_path.write_bytes(encode(status(self.process.now)))
            if self.process.stops:self.terminal()
        result = collect(self.log, self.root / 'capture', ARM, DLL, self.process, 60, poll, timeline_limit=600)
        self.assertEqual(result['stop_reason'], 'timeline-capacity')
        self.assertEqual(result['reason'], 'native-terminal')
        self.assertLessEqual(result['timeline_bytes'], 600)
        for line in (self.root / 'capture/timeline.jsonl').read_text().splitlines():json.loads(line)

    def test_invalid_timeline_limit_creates_no_capture(self):
        for value in (0, -1, True, 1024**3+1):
            with self.subTest(value=value),self.assertRaises(ValueError):
                collect(self.log, self.root / 'capture', ARM, DLL, self.process, 60, timeline_limit=value)
            self.assertFalse((self.root / 'capture').exists())

    def test_large_status_full_hour(self):
        # Representative modern renderer receipts exceed the original 128 MiB
        # limit over an hour. Exercise the actual collector and stop deadline.
        def poll(_):
            self.process.now += 5000
            value=status(self.process.now // 5);value['bounded_diagnostics']='x'*(200*1024)
            self.status_path.write_bytes(encode(value))
            if self.process.stops:self.terminal()
        result=collect(self.log,self.root/'capture',ARM,DLL,self.process,60,poll,timeline_limit=1024**3)
        self.assertEqual(self.process.stops,[3601000])
        self.assertEqual(result['stop_reason'],'duration-limit')
        self.assertEqual(result['reason'],'native-terminal')
        self.assertGreater(result['timeline_bytes'],128*1024*1024)
        self.assertEqual(result['timeline_sha256'],file_digest(self.root/'capture/timeline.jsonl'))
        meta=json.loads((self.root/'capture/collector-start.json').read_text())
        self.assertEqual(meta['timeline_limit_bytes'],1024**3)

    def test_process_exit_preserves_partial_write(self):
        def poll(_):
            self.process.now += 1000
            self.process.running = False
            with self.log.open('ab') as stream:
                stream.write(b'{"event":')
        result = self.run_collector(poll)
        self.assertEqual(result['reason'], 'process-exited')
        self.assertTrue((self.root / 'capture/native-log-at-end.jsonl').read_bytes().endswith(b'{"event":'))
        self.assertEqual(self.process.stops, [])

    def test_partial_status_recovers(self):
        self.status_path.write_bytes(b'{')
        def poll(_):
            self.process.now += 1000
            self.status_path.write_bytes(encode(status(2)))
            self.terminal()
        result = self.run_collector(poll)
        self.assertEqual(result['reason'], 'native-terminal')
        self.assertEqual(result['read_retries']['status_reads'], 1)

    def test_native_fault_ends_collection(self):
        self.terminal(6)
        result = self.run_collector(lambda _: self.fail('Unexpected wait'))
        self.assertEqual(result['first_fault_observed']['fault'], 6)
        self.assertFalse(result['terminal']['healthy'])
        self.assertEqual(self.process.stops, [])

    def test_changed_generation_requests_stop(self):
        def poll(_):
            self.process.now += 1000
            wrong = header(); wrong['creation'] += 1
            self.log.write_bytes(encode(wrong))
        result = self.run_collector(poll)
        self.assertEqual(result['reason'], 'collector-error')
        self.assertEqual(len(self.process.stops), 1)

    def test_stale_status_does_not_terminate_game(self):
        def poll(_):
            self.process.now += 10000
            if self.process.now > 41000:
                self.process.running = False
        result = self.run_collector(poll)
        self.assertEqual(result['reason'], 'process-exited')
        self.assertEqual(self.process.stops, [])
        events = [json.loads(line)['event'] for line in (self.root / 'capture/timeline.jsonl').read_text().splitlines()]
        self.assertIn('status-stale', events)

    def test_unacknowledged_stop_is_incomplete(self):
        def poll(_):
            self.process.now += 10000
        result = self.run_collector(poll)
        self.assertEqual(result['reason'], 'stop-not-acknowledged')
        self.assertIsNone(result['terminal'])
        self.assertEqual(len(self.process.stops), 1)

    def test_one_hour_replay(self):
        def poll(_):
            self.process.now += 5000
            self.status_path.write_bytes(encode(status(self.process.now // 5)))
            if self.process.stops:
                self.terminal()
        result = self.run_collector(poll, minutes=60)
        self.assertEqual(self.process.stops, [3601000])
        self.assertEqual(result['reason'], 'native-terminal')
        self.assertGreaterEqual(result['snapshots_and_events'], 721)
        self.assertLess(result['timeline_bytes'], 1024 * 1024)

    @unittest.skipUnless(os.name == 'nt', 'Windows process/event API')
    def test_real_owned_process_and_stop_event(self):
        k = c.WinDLL('kernel32', use_last_error=True)
        k.GetCurrentProcess.restype = w.HANDLE
        k.GetProcessTimes.argtypes = [w.HANDLE] + [c.POINTER(w.FILETIME)] * 4
        k.GetProcessTimes.restype = w.BOOL
        times = [w.FILETIME() for _ in range(4)]
        self.assertTrue(k.GetProcessTimes(k.GetCurrentProcess(), *map(c.byref, times)))
        creation = (times[0].dwHighDateTime << 32) | times[0].dwLowDateTime
        process = Process(os.getpid(), creation)
        self.addCleanup(process.close)
        self.assertTrue(process.alive())
        k.CreateEventW.argtypes = [c.c_void_p, w.BOOL, w.BOOL, w.LPCWSTR]
        k.CreateEventW.restype = w.HANDLE
        k.CloseHandle.argtypes = [w.HANDLE]
        k.WaitForSingleObject.argtypes = [w.HANDLE, w.DWORD]
        k.WaitForSingleObject.restype = w.DWORD
        event = k.CreateEventW(None, False, False, f'Local\\MhwTemporalInputs-v1-{os.getpid()}-{creation}-0')
        self.assertTrue(event)
        self.addCleanup(k.CloseHandle, event)
        before = process.tick()
        self.assertGreaterEqual(process.stop(), before)
        self.assertEqual(k.WaitForSingleObject(event, 0), 0)

    @unittest.skipUnless(os.name == 'nt', 'Windows process identity API')
    def test_real_process_rejects_wrong_generation(self):
        with self.assertRaises(ValueError):
            Process(os.getpid(), 1)


if __name__ == '__main__':
    unittest.main()
