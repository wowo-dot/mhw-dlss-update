"""Bounded presentation ownership evidence; FG is deliberately not initialized."""
import argparse
import hashlib
import json
from pathlib import Path
from reflex_frames import check, control, HOST_SHA
from reflex_temporal import review as combined_review


def one(rows, event):
    values = [r for r in rows if r.get('event') == event]
    check(len(values) == 1, event)
    return values[0]


def identity(rows):
    h = rows[0]
    check(h.get('event') == 'start' and h.get('schema') == 4 and
          h.get('version') == '0.6.0' and h.get('experiment') == 'frame-generation-game-ownership', 'ownership identity')
    check(h.get('host_sha256') == HOST_SHA and h.get('fg_plugin_loaded') is False and
          h.get('fg_enabled') is False and h.get('upscaling') is True and
          h.get('temporal_version') == '1.25.44', 'ownership scope')
    return h


def trace_review(rows):
    s = one(rows, 'ownership_summary')
    check(s.get('schema') == 1 and s.get('drained') is True and s.get('phase') == 1 and
          s.get('pointers_owned') is False and s.get('known_bucket_counts_exact') is True and
          s.get('sampling') == 'first-four-then-powers-of-two-to-65536', 'ownership contract')
    check(s.get('bucket_limit') == 64 and s.get('sample_limit') == 512 and
          s.get('dropped_buckets') == s.get('dropped_samples') == 0, 'complete bounded map')
    check(type(s.get('qpc_frequency')) is int and s['qpc_frequency'] > 0 and
          type(s.get('image')) is int and s['image'] > 0 and
          type(s.get('image_bytes')) is int and 0 < s['image_bytes'] <= 0x10000000, 'ownership clock/image')
    buckets = [r for r in rows if r.get('event') == 'ownership_bucket']
    samples = [r for r in rows if r.get('event') == 'ownership_sample']
    check(len(buckets) == s.get('buckets') and 0 < len(buckets) <= 64 and
          len(samples) == s.get('samples') and 0 < len(samples) <= 512, 'declared trace sizes')

    def key(r):
        check(type(r.get('method')) is int and 0 <= r['method'] < 12 and r.get('phase') in (0, 1) and
              type(r.get('thread')) is int and r['thread'] > 0 and
              type(r.get('caller')) is int and r['caller'] > 0, 'API identity')
        rva = r['caller'] - s['image']
        check(r.get('caller_rva') == (rva if 0 <= rva < s['image_bytes'] else 0), 'caller classification')
        return r['method'], r['phase'], r['thread'], r['caller']

    counts = {}
    for b in buckets:
        k = key(b)
        check(k not in counts and type(b.get('count')) is int and b['count'] > 0, 'unique exact buckets')
        counts[k] = b['count']
    check(sum(counts.values()) == s.get('calls'), 'all calls accounted for')
    previous = previous_qpc = 0
    grouped = {k: [] for k in counts}
    for v in samples:
        k = key(v)
        check(k in grouped and type(v.get('sequence')) is int and previous < v['sequence'] <= s['calls'] and
              type(v.get('qpc')) is int and v['qpc'] >= previous_qpc and v['qpc'] > 0, 'ordered observations')
        previous, previous_qpc = v['sequence'], v['qpc']
        check(type(v.get('object')) is int and v['object'] > 0 and
              type(v.get('returned')) is int and v['returned'] >= 0 and
              type(v.get('result')) is int and 0 <= v['result'] <= 0xffffffff and
              len(v.get('args', [])) == 8 and all(type(a) is int and 0 <= a < 1 << 64 for a in v['args']), 'API values')
        grouped[k].append(v)
    for k, count in counts.items():
        expected = min(count, 4) + sum((1 << n) <= count for n in range(3, 17))
        check(len(grouped[k]) == expected, 'complete exponential sample schedule')
    methods = sorted({b['method'] for b in buckets})
    check(0 in methods and 5 in methods and any(m in methods for m in (1, 2, 3)) and
          any(m in methods for m in (4, 9)), 'creation/buffer/present observations')
    return dict(calls=s['calls'], samples=len(samples), buckets=len(buckets), methods=methods,
                api_threads={str(m): sorted({b['thread'] for b in buckets if b['method'] == m and b['caller_rva']})
                             for m in methods}, sampling_complete=True, buffer_lifetime_verified=False,
                final_HUDless_color_verified=False, FG_activated=False)


def review(rows, inputs, startup, dll):
    identity(rows)
    trace = trace_review(rows)
    # Preserve the exact established combined renderer rules. Normalize only
    # this wrapper identity after independently validating its added scope.
    normalized = [dict(r) for r in rows]
    normalized[0].update(schema=3, version='0.5.1', experiment='reflex-temporal-coexistence')
    combined = combined_review(normalized, inputs, startup, dll)
    end = one(rows, 'frame_finished')
    present_threads = set(trace['api_threads'].get('4', []) + trace['api_threads'].get('9', []))
    check(present_threads == {end['owner']}, 'native Present and Reflex owner agree')
    check(any(r.get('event') == 'ownership_sample' and r['method'] in (4, 9) and r['phase'] == 1 and
              r['args'][2] == 1 and r['args'][3] == 1 for r in rows), 'marked native presentation')
    return dict(schema=1, accepted=True, version='0.6.0', ownership=trace, combined=combined,
                game_frame_generation_implemented=False)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('action', choices=('arm', 'stop', 'review', 'owned-review'))
    p.add_argument('--log', required=True, type=Path)
    p.add_argument('--output', required=True, type=Path)
    p.add_argument('--dll-sha256')
    a = p.parse_args()
    check(not a.output.exists() and a.log.stat().st_size <= 8 * 1024 * 1024, 'preserve bounded evidence')
    raw = a.log.read_bytes()
    rows = [json.loads(r) for r in raw.splitlines()]
    check(bool(rows), 'nonempty log')
    if a.action == 'owned-review':
        check(rows[0].get('game_access') is False, 'owned scope')
        session = one(rows, 'frame_session')
        check(session.get('mode') == 1 and session.get('faults') == 0 and
              all(session.get(k) == 144 for k in ('completed', 'tokens', 'sleeps', 'present_begin_callbacks', 'present_end_callbacks')) and
              session.get('markers') == 864 and session.get('test_callbacks') == 1, 'owned Reflex frame alignment')
        off = [r for r in rows if r.get('event') == 'driver_sleep_status' and r.get('phase') == 'restored_off']
        check(len(off) == 1 and off[0].get('result') == 0 and off[0].get('low_latency') is False, 'owned driver Off')
        end = one(rows, 'finished')
        check(end.get('presents') == end.get('native_presents') == 144 and end.get('resizes') == end.get('swapchains') == 2 and
              end.get('faults') == end.get('device_refs') == 0 and end.get('hooks_restored') == 12 and
              all(end.get(k) is True for k in ('sdk_shutdown', 'pins_released', 'native_after_stop')), 'owned cleanup')
        result = dict(schema=1, accepted=True, ownership=trace_review(rows), fixture_renderer=True)
    else:
        h = identity(rows)
        if a.action == 'review':
            check(a.dll_sha256, 'expected DLL')
            def paired(prefix):
                path = a.log.parent / f"{prefix}-{h['pid']}-{h['creation']}.jsonl"
                check(path.stat().st_size <= 64 * 1024 * 1024, 'bounded paired log')
                return [json.loads(r) for r in path.read_bytes().splitlines()]
            result = review(rows, paired('inputs'), paired('startup'), a.dll_sha256)
        else:
            result = control(rows, a.log, a.action)
    result['log_sha256'] = hashlib.sha256(raw).hexdigest()
    with a.output.open('x', encoding='utf-8') as f:
        json.dump(result, f, indent=2)
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
