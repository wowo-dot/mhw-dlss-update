"""Controls and summary for the in-game frame generation build (v0.7.x).

  arm     --run <folder>   start Reflex frames, DLSS and FG (generation-checked)
  stop    --run <folder>   FG Off; Streamline, DLSS and Reflex stay live
  status  --run <folder>   latest FG session counters
  summary --run <folder>   write summary.json from the run's logs

Controls use the existing generation-checked marker files and process-identity
queries; nothing writes process memory.
"""
import argparse
import hashlib
import json
from pathlib import Path

from reflex_frames import check, control, HOST_SHA

VERSION = '0.7.0'


def log_path(run):
    logs = sorted(Path(run).glob('reflex-frames-*.jsonl'))
    check(len(logs) == 1, 'exactly one process log in the run folder')
    return logs[0]


def rows_of(path, limit=64 * 1024 * 1024):
    raw = path.read_bytes()
    check(len(raw) <= limit, 'bounded log')
    complete = raw[:raw.rfind(b'\n') + 1]  # A live log may end mid-line.
    return [json.loads(line) for line in complete.splitlines() if line.strip()], raw


def identity(rows):
    h = rows[0]
    check(h.get('event') == 'start' and h.get('version') == VERSION and h.get('experiment') == 'frame-generation-game' and
          h.get('fg_plugin_loaded') is True and h.get('host_sha256') == HOST_SHA, 'FG game build identity')
    return h


def latest(rows, event):
    found = [r for r in rows if r.get('event') == event]
    return found[-1] if found else None


def status(rows):
    found = [r for r in rows if r.get('event') in ('fg_status', 'fg_stopped')]
    s = found[-1] if found else None
    session = (s or {}).get('session', {})
    keys = ('armed', 'on', 'presents', 'tagged', 'recorded', 'mode_changes', 'record_failed', 'exceptions',
            'last_sdk_error', 'off', 'present_delta', 'camera_failures')
    return dict(event=(s or {}).get('event'), **{k: session.get(k) for k in keys},
                ring=session.get('ring'), sdk_state=session.get('state'))


def summary(rows, raw):
    h = identity(rows)
    one = lambda e: latest(rows, e)
    statuses = [r['session'] for r in rows if r.get('event') == 'fg_status']
    stopped = one('fg_stopped')
    final = (stopped or {}).get('session') or (statuses[-1] if statuses else {})
    return dict(
        schema=1, version=h['version'], pid=h['pid'], creation=h['creation'], dll_sha256=h.get('dll_sha256'),
        log_sha256=hashlib.sha256(raw).hexdigest(),
        startup=dict(fg_support=one('fg_support'), device_checked=bool(one('device_checked')),
                     presentation_installed=bool(one('presentation_installed')), frames_ready=bool(one('frames_ready')),
                     swapchain=one('swapchain_snapshot')),
        armed=one('fg_armed'),
        failures=[r for r in rows if r.get('event') in ('failed', 'worker_failed', 'runtime_rejection')],
        frame_heartbeats=len([r for r in rows if r.get('event') == 'frame_heartbeat']),
        status_samples=len(statuses),
        tagged_progression=[s.get('tagged') for s in statuses],
        frames_presented_progression=[(s.get('state') or {}).get('frames_presented') for s in statuses],
        final=final, stopped=bool(stopped), off_after_stop=(stopped or {}).get('off'),
        retained=bool(one('fg_runtime_retained')),
        visual_quality_verified=False, latency_measured=False, performance_measured=False)


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('action', choices=('arm', 'stop', 'status', 'summary'))
    p.add_argument('--run', type=Path, required=True)
    a = p.parse_args()
    path = log_path(a.run)
    rows, raw = rows_of(path)
    identity(rows)
    if a.action in ('arm', 'stop'):
        result = control(rows, path, a.action)
    elif a.action == 'status':
        result = status(rows)
    else:
        result = summary(rows, raw)
        with (a.run / 'summary.json').open('x', encoding='utf-8') as f:
            json.dump(result, f, indent=2)
    print(json.dumps(result, indent=1))


if __name__ == '__main__':
    main()
