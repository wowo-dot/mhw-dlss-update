"""Controls and summary for the in-game frame generation build (v0.7 and later).

  arm     --run <folder>   start Reflex frames, DLSS and FG (generation-checked)
  multiplier --run <folder> --x N   live frame multiplier (2 = ordinary FG .. 8,
                           limited by the GPU's reported capability; v0.8+)
  stop    --run <folder>   FG Off; Streamline, DLSS and Reflex stay live
  status  --run <folder>   latest FG session counters, plus Streamline's own
                           errors and warnings from sdk-logs/sl.log (an input
                           the SDK ignores is only reported there)
  lifecycle --run <folder>  host swap-chain lifecycle calls (resize, full-screen,
                           ResizeTarget, creation) and presentation state (v0.10+)
  summary --run <folder>   write summary.json (summary-<pid>.json when the folder
                           holds several launches) from the run's logs
  perf    --run <folder> --set a,b   live overhead switches (v0.12+): names from
                           PERF_TOGGLES, 'all' or 'none'; works before arming too
  frames  --run <folder>   host frame-interval windows (every 5 s, labelled with the
                           switch mask) and the driver's per-frame report while armed

--pid selects a launch when MHW was restarted inside one run folder (resolution
and screen-mode changes apply only on restart); the default is the newest.

Controls use the existing generation-checked marker files and process-identity
queries; nothing writes process memory.
"""
import argparse
import collections
import hashlib
import json
import re
from pathlib import Path

from reflex_frames import check, control, HOST_SHA

# src/perf/toggles.hpp, in bit order.
PERF_TOGGLES = ('cached_reads', 'cached_methods', 'list_locks', 'packet_filter', 'quiet_status', 'no_viewport_trace',
                'no_output_sentinel', 'sample_profile', 'pooled_selectors', 'keyed_staging')
LEAN_MASK = 895  # 'all': every lean path (pooled_selectors v0.12.3+, keyed_staging v0.12.5+); sample_profile by name
VERSIONS = ('0.7.0', '0.8.0', '0.9.0', '0.9.1', '0.9.2', '0.9.3', '0.10.0', '0.10.1', '0.10.2', '0.10.3', '0.10.4', '0.11.0', '0.11.1', '0.11.2', '0.11.3', '0.11.4', '0.11.5', '0.11.6', '0.11.7', '0.12.0', '0.12.1', '0.12.2', '0.12.3', '0.12.4', '0.12.5', '0.12.6', '0.12.7', '0.12.8', '0.12.9', '0.12.10', '0.12.11', '0.12.12', '0.12.13', '0.12.14', '0.13.0', '0.13.1', '0.13.2', '0.13.3', '0.13.4', '0.13.5', '0.13.6', '0.13.7', '0.13.8', '0.13.9', '0.13.10', '0.13.11')


def log_path(run, pid=None):
    # Side files (<log>.boot.jsonl, <log>.crash.jsonl) share the prefix. MHW
    # applies resolution and screen-mode changes only on restart, so one run
    # folder can hold several launches: take the given pid, else the newest
    # (reflex-frames-<pid>-<creation FILETIME>.jsonl).
    logs = [p for p in Path(run).glob('reflex-frames-*.jsonl') if p.suffixes == ['.jsonl']]
    check(logs, 'a process log in the run folder')
    parts = lambda p: tuple(int(x) for x in p.stem.split('-')[2:4])
    if pid is not None:
        logs = [p for p in logs if parts(p)[0] == pid]
        check(len(logs) == 1, 'exactly one process log for the pid')
    return max(logs, key=lambda p: parts(p)[1])


def rows_of(path, limit=64 * 1024 * 1024):
    raw = path.read_bytes()
    check(len(raw) <= limit, 'bounded log')
    complete = raw[:raw.rfind(b'\n') + 1]  # A live log may end mid-line.
    return [json.loads(line) for line in complete.splitlines() if line.strip()], raw


def identity(rows):
    h = rows[0]
    check(h.get('event') == 'start' and h.get('version') in VERSIONS and h.get('experiment') == 'frame-generation-game' and
          (h.get('fg_plugin_loaded') is True or (h.get('fg_plugin_loaded') is False and VERSIONS.index(h['version']) >= VERSIONS.index('0.12.1'))) and
          h.get('host_sha256') == HOST_SHA, 'FG game build identity')
    return h


def latest(rows, event):
    found = [r for r in rows if r.get('event') == event]
    return found[-1] if found else None


def status(rows):
    found = [r for r in rows if r.get('event') in ('fg_status', 'fg_stopped')]
    s = found[-1] if found else None
    session = (s or {}).get('session', {})
    keys = ('armed', 'on', 'hudless', 'staged', 'staged_dropped', 'final_unstaged', 'final_mismatch', 'multiplier', 'generated', 'generated_changes', 'generated_limited', 'presented_frames',
            'presents', 'tagged', 'recorded', 'mode_changes', 'record_failed', 'exceptions', 'last_sdk_error', 'off',
            'lifecycle_calls', 'lifecycle_offs',
            'present_delta', 'camera_failures')
    return dict(event=(s or {}).get('event'), reason=(s or {}).get('reason'), **{k: session.get(k) for k in keys},
                ring=session.get('ring'), sdk_state=session.get('state'))


def streamline_log(run, limit=8):
    """Count sl.log errors and warnings by message, with sizes and timings
    normalised. Streamline reports an ignored input (for example a HUD-less
    extent that does not match the back buffer) only here, every frame."""
    path = Path(run) / 'sdk-logs' / 'sl.log'
    if not path.exists():
        return None
    counts = {'error': collections.Counter(), 'warn': collections.Counter()}
    for line in path.read_text(encoding='utf-8', errors='replace').splitlines():
        m = re.search(r'\]\[(error|warn)\]\[tid:\d+\]\[[^\]]*\](.*)$', line)
        if m:
            counts[m.group(1)][re.sub(r'\d+x\d+', 'WxH', m.group(2).strip())] += 1
    return {'errors': sum(counts['error'].values()), 'warnings': sum(counts['warn'].values()),
            'top_errors': counts['error'].most_common(limit), 'top_warnings': counts['warn'].most_common(limit)}


LIFECYCLE_EVENTS = ('swap_lifecycle', 'swap_lifecycle_dropped', 'presentation_state')


def lifecycle(rows):
    return [r for r in rows if r.get('event') in LIFECYCLE_EVENTS]


def request_multiplier(rows, path, total):
    check(2 <= total <= 8, 'multiplier 2..8')
    check(rows[0].get('version') != '0.7.0', 'build supports live multiplier')
    check(any(r.get('event') == 'fg_armed' for r in rows), 'FG armed')
    check(not any(r.get('event') in ('fg_stopped', 'failed', 'worker_failed') for r in rows), 'FG session live')
    n = 1
    while Path(f'{path}.mfg-{n}').exists():
        n += 1
    # Write then rename so the worker never reads a partial request.
    staged = Path(f'{path}.mfg-{n}.tmp')
    with staged.open('x', encoding='ascii') as f:
        f.write(f"{rows[0]['creation']} {total}\n")
    staged.rename(f'{path}.mfg-{n}')
    return dict(action='multiplier', request=n, multiplier=total, request_sent=True, acknowledged=False)


def perf_mask(names):
    names = [n.strip() for n in names.split(',') if n.strip()]
    if names == ['none']:
        return 0
    check(names and all(n in PERF_TOGGLES or n == 'all' for n in names), 'perf switch names')
    mask = 0
    for n in names:
        mask |= LEAN_MASK if n == 'all' else 1 << PERF_TOGGLES.index(n)
    return mask


def request_perf(rows, path, mask):
    check(any(r.get('event') == 'perf_toggles' for r in rows), 'build supports perf switches')
    check(not any(r.get('event') in ('failed', 'worker_failed', 'finished') for r in rows), 'session live')
    n = 1
    while Path(f'{path}.perf-{n}').exists():
        n += 1
    staged = Path(f'{path}.perf-{n}.tmp')
    with staged.open('x', encoding='ascii') as f:
        f.write(f"{rows[0]['creation']} {mask}\n")
    staged.rename(f'{path}.perf-{n}')
    return dict(action='perf', request=n, mask=mask, names=[n for i, n in enumerate(PERF_TOGGLES) if mask >> i & 1],
                request_sent=True, acknowledged=False)


def frames(rows):
    """Frame windows in log order; each latency report follows its armed window."""
    out = []
    for r in rows:
        if r.get('event') == 'frame_stats':
            out.append(dict(phase=r['phase'], perf=r['perf'], frames=r['frames'], fps=round(r['fps'], 1),
                            p50_ms=r['p50_us'] / 1000, p99_ms=r['p99_us'] / 1000, max_ms=r['max_us'] / 1000,
                            over_50ms=r['over_50ms']))
        elif r.get('event') == 'latency' and r.get('available') and out:
            out[-1]['driver_ms'] = {k: [v / 1000 for v in r[k]] for k in
                                    ('interval', 'simulation', 'submit', 'present', 'driver', 'queue', 'gpu', 'gpu_active', 'gpu_frame')
                                    if k in r}
        elif r.get('event') == 'perf_toggles':
            out.append(dict(perf_switch=r['mask'], accepted=r['accepted']))
    return out


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
        multiplier_requests=[r for r in rows if r.get('event') == 'fg_multiplier'],
        failures=[r for r in rows if r.get('event') in ('failed', 'worker_failed', 'runtime_rejection')],
        frame_heartbeats=len([r for r in rows if r.get('event') == 'frame_heartbeat']),
        status_samples=len(statuses),
        tagged_progression=[s.get('tagged') for s in statuses],
        frames_presented_progression=[(s.get('state') or {}).get('frames_presented') for s in statuses],
        final=final, stopped=bool(stopped), off_after_stop=(stopped or {}).get('off'),
        stop_reason=(stopped or {}).get('reason'), lifecycle=lifecycle(rows),
        retained=bool(one('fg_runtime_retained')), frames=frames(rows),
        visual_quality_verified=False, latency_measured=False, performance_measured=False)


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('action', choices=('arm', 'multiplier', 'stop', 'status', 'lifecycle', 'summary', 'perf', 'frames'))
    p.add_argument('--run', type=Path, required=True)
    p.add_argument('--x', type=int, help='total frame multiplier for the multiplier action')
    p.add_argument('--set', help="perf switches: comma-separated names, 'all' or 'none'")
    p.add_argument('--pid', type=int, help='launch to use when the run folder holds several (default: newest)')
    a = p.parse_args()
    path = log_path(a.run, a.pid)
    rows, raw = rows_of(path)
    identity(rows)
    if a.action in ('arm', 'stop'):
        result = control(rows, path, a.action)
    elif a.action == 'multiplier':
        result = request_multiplier(rows, path, a.x or 0)
    elif a.action == 'status':
        result = status(rows)
        result['sl_log'] = streamline_log(a.run)
    elif a.action == 'lifecycle':
        result = lifecycle(rows)
    elif a.action == 'perf':
        result = request_perf(rows, path, perf_mask(a.set or ''))
    elif a.action == 'frames':
        result = frames(rows)
    else:
        result = summary(rows, raw)
        result['sl_log'] = streamline_log(a.run)
        name = 'summary.json' if len(list(a.run.glob('reflex-frames-*[0-9].jsonl'))) == 1 else f"summary-{rows[0]['pid']}.json"
        with (a.run / name).open('x', encoding='utf-8') as f:
            json.dump(result, f, indent=2)
    print(json.dumps(result, indent=1))


if __name__ == '__main__':
    main()
