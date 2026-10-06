"""Control/review the bounded Reflex frame trace; never writes process memory."""
import argparse
import ctypes as c
from ctypes import wintypes as w
import hashlib
import json
from pathlib import Path

HOST_SHA = 'c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea'
NAMES = ['engine-update', 'counter-bank', 'record-publish', 'renderer',
         'native-present', 'execute', 'present-api']
INPUT_NAMES = NAMES + ['keyboard-refresh', 'mouse-refresh', 'device-refresh',
                       'synchronous-job-batch', 'backend-submit', 'device-poll']


def require(value, message):
    if not value:
        raise ValueError(message)


def parse(path):
    require(path.stat().st_size <= 256 * 1024 * 1024, 'trace size bound')
    rows = [json.loads(line) for line in path.read_text().splitlines()]
    require(rows and rows[0].get('event') == 'start', 'start record')
    h = rows[0]
    require((h.get('schema'),h.get('version'),h.get('experiment')) in
            ((1,'0.1.0','reflex-frame-boundaries'),(2,'0.2.0','reflex-input-boundaries')) and
            h.get('host_sha256') == HOST_SHA, 'probe identity')
    require(h.get('sdk_loaded') is False and h.get('reflex_sleep') is False and
            h.get('upscaling') is False, 'observation scope')
    return rows


def review(rows):
    input_trace = rows[0].get('schema') == 2
    names = INPUT_NAMES if input_trace else NAMES
    kinds = len(names)
    require(not any(r['event'] == 'failed' for r in rows), 'setup failure')
    armed = [r for r in rows if r['event'] == 'armed']
    stopped = [r for r in rows if r['event'] == 'stopped']
    finished = [r for r in rows if r['event'] == 'worker_finished']
    require(len(armed) == len(stopped) == len(finished) == 1, 'complete lifecycle')
    a, s, f = armed[0], stopped[0], finished[0]
    require(a['duration_ms'] == 15000 and a['capacity'] == 262144 and
            len(a['targets']) == kinds and len(set(a['targets'])) == kinds, 'capture contract')
    require(len(s['visits']) == kinds, 'visit layout')
    if input_trace:
        require(a.get('input_values_recorded') is False and
                all(a.get(key,0)>0 for key in ('engine','keyboard_object','mouse_object','devices_object')) and
                len(a.get('original_head_sha256',[])) == kinds and
                all(isinstance(v,str) and len(v)==64 and set(v)<=set('0123456789abcdef')
                    for v in a['original_head_sha256']), 'input identities/head receipts')
        require(len(a.get('native_gates',[])) == 11 and
                {g['kind'] for g in a['native_gates']} == set(range(kinds))-{5,6}, 'native gate layout')
    require(s['drops'] == s['exceptions'] == s['pending'] == 0 and
            s['native_bytes_restored'] is True and f['installed'] is True and
            f['cleanup_passed'] is True, 'clean recording/restoration')
    events = [r for r in rows if r['event'] == 'boundary']
    require(len(events) == s['records'] and events, 'record count')
    orders = []
    stats = []
    for r in events:
        require(0 <= r['kind'] < kinds and r['thread'] > 0 and
                r['enter'] < r['exit'] and r['qpc_enter'] <= r['qpc_exit'] and
                r['exception'] == 0, 'complete ordered pair')
        require(len(r['before']) == len(r['after']) == 4 and
                len(r['args']) == 3 and len(r['stack']) <= 12, 'bounded record layout')
        orders.extend([r['enter'], r['exit']])
        if input_trace:
            for key in ('context_before','context_after'):
                v=r.get(key,[])
                require(len(v)==3 and type(v[0]) is int and 0<=v[0]<=256 and
                        type(v[1]) is int and 0<=v[1]<16 and type(v[2]) is bool, 'engine context layout')
            require(type(r.get('qualified')) is bool, 'qualification layout')
            if r['kind'] in (7,8,9,12):
                require(r['args']==[0,0,0] and r.get('returned')==0, 'input-value redaction')
    require(len(orders) == len(set(orders)), 'duplicate event order')
    for kind, name in enumerate(names):
        group = [r for r in events if r['kind'] == kind]
        require(len(group) == s['visits'][kind] and group, 'missing/count boundary ' + name)
        stats.append(dict(kind=kind, name=name, calls=len(group),
                          threads=sorted({r['thread'] for r in group}),
                          unstable_stamps=sum(not r['before'][3] or not r['after'][3] for r in group)))
        if input_trace:
            stats[-1].update(qualified_calls=sum(r['qualified'] for r in group),
                            unstable_contexts=sum(not r['context_before'][2] or not r['context_after'][2] for r in group),
                            worker_counts=sorted({r['context_before'][0] for r in group}),
                            pending_masks=sorted({r['context_before'][1] for r in group}))
    plans=[r for r in rows if r['event']=='worker_plan']
    if input_trace:
        require(1<=len(plans)<=2 and len({p['order'] for p in plans})==len(plans), 'worker plan count')
        publishes={r['enter'] for r in events if r['kind']==2}
        for p in plans:
            require(p['order'] in publishes and type(p['valid']) is bool and len(p['jobs'])<=64, 'worker plan layout')
            require(all(len(j)==3 and type(j[0]) is int and j[0]>=0 and type(j[1]) is int and
                        j[1]>0 and type(j[2]) is int and 0<=j[2]<2 for j in p['jobs']), 'worker routing metadata')
    tests = [r for r in events if r['kind'] == 6 and r['args'][1] & 1]
    real = [r for r in events if r['kind'] == 6 and not r['args'][1] & 1]
    require(real, 'no real Present calls')
    return dict(schema=2 if input_trace else 1, lifecycle_passed=True, native_bytes_restored=True,
                records=len(events), boundaries=stats, present_tests=len(tests),
                actual_presents=len(real), frame_alignment_proven=False,
                game_reflex_implemented=False, worker_plan_samples=len(plans),
                input_values_recorded=False)


def signal(rows, action):
    require(not any(r['event'] in ('failed', 'stopped', 'worker_finished') for r in rows), 'inactive recorder')
    armed = any(r['event'] == 'armed' for r in rows)
    require(armed if action == 'stop' else not armed and any(r['event'] == 'ready' for r in rows), 'control state')
    k = c.WinDLL('kernel32', use_last_error=True)
    k.OpenProcess.argtypes = [w.DWORD, w.BOOL, w.DWORD]; k.OpenProcess.restype = w.HANDLE
    k.GetProcessTimes.argtypes = [w.HANDLE] + [c.POINTER(w.FILETIME)] * 4; k.GetProcessTimes.restype = w.BOOL
    k.QueryFullProcessImageNameW.argtypes = [w.HANDLE, w.DWORD, w.LPWSTR, c.POINTER(w.DWORD)]
    k.QueryFullProcessImageNameW.restype = w.BOOL
    k.OpenEventW.argtypes = [w.DWORD, w.BOOL, w.LPCWSTR]; k.OpenEventW.restype = w.HANDLE
    k.SetEvent.argtypes = [w.HANDLE]; k.SetEvent.restype = w.BOOL
    k.CloseHandle.argtypes = [w.HANDLE]
    k.GetTickCount64.restype = c.c_uint64
    h = rows[0]; process = k.OpenProcess(0x1000, False, h['pid']); require(process, 'process unavailable')
    try:
        times = [w.FILETIME() for _ in range(4)]
        require(k.GetProcessTimes(process, *map(c.byref, times)), 'process times')
        creation = (times[0].dwHighDateTime << 32) | times[0].dwLowDateTime
        require(creation == h['creation'] and not(times[1].dwHighDateTime or times[1].dwLowDateTime), 'process generation')
        image = c.create_unicode_buffer(32768); size = w.DWORD(len(image))
        require(k.QueryFullProcessImageNameW(process, 0, image, c.byref(size)), 'host path')
        require(hashlib.sha256(Path(image.value).read_bytes()).hexdigest() == HOST_SHA, 'host file identity')
        phase = 1 if action == 'arm' else 0
        prefix = 'MhwReflexInput-v2' if h['schema']==2 else 'MhwReflexBoundaries-v1'
        name = f'Local\\{prefix}-{h["pid"]}-{creation}-{phase}'
        event = k.OpenEventW(2, False, name); require(event, 'event unavailable')
        try:
            require(k.SetEvent(event), 'signal failed')
            tick = k.GetTickCount64()
        finally:
            k.CloseHandle(event)
    finally:
        k.CloseHandle(process)
    return dict(schema=1, action=action, pid=h['pid'], creation=creation,
                request_sent=True, acknowledged=False, tick=tick,
                process_access='PROCESS_QUERY_LIMITED_INFORMATION', process_memory_writes=0)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('action', choices=['arm', 'stop', 'review'])
    p.add_argument('--log', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args(); require(not a.output.exists(), 'preserve existing receipt')
    rows = parse(a.log)
    result = review(rows) if a.action == 'review' else signal(rows, a.action)
    result['log_sha256'] = hashlib.sha256(a.log.read_bytes()).hexdigest()
    with a.output.open('x', encoding='utf-8') as out:
        json.dump(result, out, indent=2); out.write('\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
