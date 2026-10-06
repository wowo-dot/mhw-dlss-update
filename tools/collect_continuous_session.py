"""Preserve timed DLAA status snapshots outside the game for a bounded play session.

Uses only existing log files, process identity queries and the normal stop event.
Does not read/write process memory or install hooks. Run after an arm receipt.
"""
import argparse
import ctypes as c
from ctypes import wintypes as w
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import time

from arm_continuous_dlaa import parse, require, MAX_BYTES

MAX_TIMELINE = 128 * 1024 * 1024
MAX_TIMELINE_LIMIT = 1024 * 1024 * 1024


def utc():
    return datetime.now(timezone.utc).isoformat(timespec='milliseconds')


def digest(raw):
    return hashlib.sha256(raw).hexdigest()


def file_digest(path):
    value = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            value.update(chunk)
    return value.hexdigest()


def bounded_read(path):
    with Path(path).open('rb') as stream:
        raw = stream.read(MAX_BYTES + 1)
    require(0 < len(raw) <= MAX_BYTES, 'bounded file size')
    return raw


def decode_status(raw):
    require(0 < len(raw) <= MAX_BYTES, 'bounded status size')
    status = json.loads(raw)
    require(isinstance(status, dict) and status.get('event') == 'continuous' and status.get('schema') == 1, 'status envelope')
    for key in ('frames', 'fault', 'signal_value', 'completed_value'):
        require(type(status.get(key)) is int and status[key] >= 0, 'status ' + key)
    for component in ('scaled_route', 'resolution', 'activation'):
        if component in status:
            part = status[component]
            require(isinstance(part, dict) and type(part.get('fault')) is int and part['fault'] >= 0,
                    'status ' + component + ' fault')
    require(type(status.get('active')) is bool, 'status active')
    return status


def validate_session(rows, arm, expected):
    header = rows[0]
    require(header['dll_sha256'] == expected.lower(), 'DLL identity')
    require((arm['pid'], arm['creation']) == (header['pid'], header['creation']), 'arm generation')
    require(arm['stop'] is False and arm['request_sent'] is True, 'arm request')
    require(type(arm['requested_tick']) is int and arm['requested_tick'] > 0, 'arm clock')


class Timeline:
    def __init__(self, output, origin_tick, limit=MAX_TIMELINE):
        self.path = Path(output)
        self.stream = self.path.open('xb')
        self.origin = origin_tick
        self.limit = limit
        self.bytes = self.samples = 0
        self.last_hash = None
        self.first_fault = None

    def append(self, event, before_tick, after_tick, before_utc, after_utc, **fields):
        require(self.origin <= before_tick <= after_tick, 'observation clock')
        row = dict(schema=1, event=event, index=self.samples,
                   observed_begin_utc=before_utc, observed_end_utc=after_utc,
                   observed_begin_tick=before_tick, observed_end_tick=after_tick,
                   elapsed_ms=after_tick - self.origin, **fields)
        raw = (json.dumps(row, separators=(',', ':')) + '\n').encode('utf-8')
        if self.bytes + len(raw) > self.limit:
            return False
        self.stream.write(raw)
        self.stream.flush()
        self.bytes += len(raw)
        self.samples += 1
        return True

    def snapshot(self, raw, before_tick, after_tick, before_utc, after_utc, source='latest-status'):
        status = decode_status(raw)
        sha = digest(raw)
        if source == 'latest-status' and sha == self.last_hash:
            return 'duplicate'
        if not self.append('snapshot', before_tick, after_tick, before_utc, after_utc,
                           source=source, raw_sha256=sha, status=status):
            return 'full'
        self.last_hash = sha
        faults = {'runtime': status['fault']}
        faults.update((key, status[key]['fault']) for key in ('scaled_route', 'resolution', 'activation') if key in status)
        source_fault = next((key for key, value in faults.items() if value), None)
        if source_fault is not None and self.first_fault is None:
            self.first_fault = dict(fault=faults[source_fault], component=source_fault, faults=faults, observed_utc=after_utc,
                                    observed_tick=after_tick, elapsed_ms=after_tick - self.origin,
                                    frames=status['frames'], raw_sha256=sha)
        return 'saved'

    def close(self):
        self.stream.close()


class Process:
    def __init__(self, pid, creation):
        self.pid, self.creation = pid, creation
        self.k = c.WinDLL('kernel32', use_last_error=True)
        self.k.OpenProcess.argtypes = [w.DWORD, w.BOOL, w.DWORD]
        self.k.OpenProcess.restype = w.HANDLE
        self.k.GetProcessTimes.argtypes = [w.HANDLE] + [c.POINTER(w.FILETIME)] * 4
        self.k.GetProcessTimes.restype = w.BOOL
        self.k.OpenEventW.argtypes = [w.DWORD, w.BOOL, w.LPCWSTR]
        self.k.OpenEventW.restype = w.HANDLE
        self.k.SetEvent.argtypes = [w.HANDLE]
        self.k.SetEvent.restype = w.BOOL
        self.k.CloseHandle.argtypes = [w.HANDLE]
        self.k.GetTickCount64.restype = c.c_uint64
        self.handle = self.k.OpenProcess(0x1000, False, pid)
        require(self.handle, 'process unavailable')
        try:
            self.alive()
        except Exception:
            self.close()
            raise

    def tick(self):
        return self.k.GetTickCount64()

    def alive(self):
        times = [w.FILETIME() for _ in range(4)]
        require(self.k.GetProcessTimes(self.handle, *map(c.byref, times)), 'process times')
        require(((times[0].dwHighDateTime << 32) | times[0].dwLowDateTime) == self.creation, 'process generation changed')
        return not (times[1].dwHighDateTime or times[1].dwLowDateTime)

    def stop(self):
        require(self.alive(), 'cannot signal exited process')
        event = self.k.OpenEventW(2, False, f'Local\\MhwTemporalInputs-v1-{self.pid}-{self.creation}-0')
        require(event, 'normal stop event unavailable')
        try:
            require(self.k.SetEvent(event), 'normal stop signal failed')
            return self.tick()
        finally:
            self.k.CloseHandle(event)

    def close(self):
        if self.handle:
            self.k.CloseHandle(self.handle)
            self.handle = None


def save(path, value):
    with Path(path).open('x', encoding='utf-8') as stream:
        json.dump(value, stream, indent=2)
        stream.write('\n')


def collect(log, output, arm, expected, process, minutes, poll=time.sleep, *, timeline_limit=MAX_TIMELINE):
    require(type(timeline_limit) is int and 0 < timeline_limit <= MAX_TIMELINE_LIMIT, 'timeline byte limit')
    # Create a fresh directory so a failed/repeated launch cannot replace evidence.
    output.mkdir(exist_ok=False)
    initial = bounded_read(log)
    rows = parse(initial)
    validate_session(rows, arm, expected)
    origin = arm['requested_tick']
    require(origin <= process.tick(), 'arm is in future')
    timeline = Timeline(output / 'timeline.jsonl', origin, timeline_limit)
    deadline = origin + minutes * 60000
    last_progress = process.tick()
    stale = False
    stop_tick = None
    stop_reason = None
    terminal = None
    reason = None
    errors = dict(status_reads=0, log_reads=0)
    start_tick = process.tick()
    save(output / 'collector-start.json', dict(schema=1, pid=rows[0]['pid'], creation=rows[0]['creation'],
        dll_sha256=expected, started_utc=utc(), started_tick=start_tick, arm_tick=origin,
        duration_minutes=minutes, stop_deadline_tick=deadline, timeline_limit_bytes=timeline_limit,
        poll_seconds=1, native_status_interval_seconds=5, initial_log_sha256=digest(initial),
        timing_meaning='Observation times bracket the file read, not exact GPU frame timestamps.',
        clock_correlation='Use UTC wall time or elapsed since arm; request collision ticks use the same Windows boot clock.',
        process_memory_reads=0, process_memory_writes=0))
    try:
        while True:
            before_tick, before_utc = process.tick(), utc()
            try:
                raw = bounded_read(log.with_suffix('.status.json'))
                state = timeline.snapshot(raw, before_tick, process.tick(), before_utc, utc())
                if state == 'saved':
                    last_progress = process.tick()
                    if stale:
                        timeline.append('status-resumed', last_progress, last_progress, utc(), utc())
                        stale = False
                elif state == 'full' and stop_reason is None:
                    stop_reason = 'timeline-capacity'
            except (OSError, ValueError, KeyError, TypeError):
                errors['status_reads'] += 1
            try:
                raw_log = bounded_read(log)
                current = parse(raw_log)
            except (OSError, ValueError, KeyError, TypeError):
                errors['log_reads'] += 1
                current = None
            if current is not None:
                validate_session(current, arm, expected)
                if current[-1]['event'] in ('stopped', 'failed'):
                    terminal = current[-1]
                    statuses = [row for row in current if row['event'] == 'continuous']
                    if statuses:
                        raw_final = (json.dumps(statuses[-1]) + '\n').encode('utf-8')
                        timeline.snapshot(raw_final, before_tick, process.tick(), before_utc, utc(), 'terminal-log')
                        save(output / 'terminal-status.json', statuses[-1])
                    reason = 'native-terminal'
                    break
            if not process.alive():
                reason = 'process-exited'
                break
            now = process.tick()
            if not stale and now - last_progress >= 30000:
                timeline.append('status-stale', now, now, utc(), utc(), age_ms=now - last_progress)
                stale = True  # Observation only: does not diagnose a hang or stop rendering.
            if stop_reason is None and (now >= deadline or (output / 'stop.request').exists()):
                stop_reason = 'duration-limit' if now >= deadline else 'requested'
            if stop_reason is not None and stop_tick is None:
                stop_tick = process.stop()
                save(output / 'stop-request.json', dict(reason=stop_reason, requested_utc=utc(),
                    requested_tick=stop_tick, request_sent=True, acknowledged=False))
            if stop_tick is not None and now - stop_tick >= 60000:
                reason = 'stop-not-acknowledged'
                break
            poll(1)
    except Exception as error:
        reason = 'collector-error'
        # Loss of observation must not silently leave an unattended experiment active.
        try:
            if process.alive() and stop_tick is None:
                stop_tick = process.stop()
        except (OSError, ValueError):
            pass
        save(output / 'collector-error.json', dict(type=type(error).__name__, message=str(error),
             normal_stop_requested_tick=stop_tick))
    finally:
        timeline.close()
    for source, name in ((log, 'native-log-at-end.jsonl'), (log.with_suffix('.status.json'), 'latest-status-at-end.json')):
        try:
            raw = bounded_read(source)
            with (output / name).open('xb') as stream:
                stream.write(raw)  # Preserve a partial final native write as evidence too.
        except (OSError, ValueError):
            pass
    result = dict(schema=1, reason=reason, ended_utc=utc(), ended_tick=process.tick(),
        elapsed_ms=process.tick() - origin, snapshots_and_events=timeline.samples,
        timeline_bytes=timeline.bytes, timeline_sha256=file_digest(timeline.path),
        first_fault_observed=timeline.first_fault, stop_reason=stop_reason,
        stop_requested_tick=stop_tick, terminal=terminal, read_retries=errors,
        full_run_review_required=True, game_terminated=False)
    save(output / 'collector-result.json', result)
    return result


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--log', type=Path, required=True)
    ap.add_argument('--arm', type=Path, required=True)
    ap.add_argument('--dll-sha256', required=True)
    ap.add_argument('--output', type=Path, required=True)
    ap.add_argument('--minutes', type=int, default=60, choices=range(1, 61))
    ap.add_argument('--timeline-mib', type=int, default=128, choices=(128,256,512,1024),
                    help='Bounded timeline storage; use 1024 for an hour of detailed renderer snapshots.')
    args = ap.parse_args()
    log = args.log.resolve()
    output = args.output.resolve()
    require(output.parent == log.parent, 'collector output must be in the session evidence root')
    arm = json.loads(args.arm.read_text(encoding='utf-8-sig'))
    rows = parse(bounded_read(log))
    validate_session(rows, arm, args.dll_sha256)
    process = Process(rows[0]['pid'], rows[0]['creation'])
    try:
        result = collect(log, output, arm, args.dll_sha256, process, args.minutes,
                         timeline_limit=args.timeline_mib * 1024 * 1024)
        print(json.dumps(result, indent=2))
        return 0 if result['reason'] == 'native-terminal' else 1
    finally:
        process.close()


if __name__ == '__main__':
    raise SystemExit(main())
