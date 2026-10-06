"""Reconstruct AA input states and submission completion from independent events."""
import argparse
import json
from pathlib import Path
from arm_aa_boundary import parse, require
from review_aa_deferred import review as deferred_review


def review_trace(g, requests, read_points=False):
    require(g['event'] == 'gpu_trace' and g['fault'] == g['pending'] == 0 and
            all(g[k] for k in ('hooks_disabled', 'drained', 'qualified', 'completed', 'journal_released', 'watch_released')), 'GPU trace closure')
    watched = g['watched']
    require(len(watched) == 3 and len(set(watched)) == 3 and min(watched) > 0, 'watched inputs')
    j = g['submission']
    require(j['fault'] == j['pending'] == 0 and j['hooks_disabled'] and j['admission_closed'] and not j['retained'] and
            j['pins'] == j['released'] > 0, 'submission closure')
    require(not any(j[k] for k in ('copies_enabled', 'colors_enabled', 'depths_enabled')), 'no private image commands')
    events = j['events']; require(0 < len(events) <= 4096, 'event capacity')
    order = {}
    for e in events:
        require(e['returned'] and e['valid'] and 0 <= e['result'] < 0x80000000 and 0 < e['enter'] < e['exit'] and e['kind'] in range(6), 'API event')
        require(e['enter'] not in order and e['exit'] not in order, 'unique API event order')
        order[e['enter']] = order[e['exit']] = e
    require(len(g['records']) == len(j['samples']) == len(requests) == 8, 'eight GPU samples')
    calls = g['barrier_calls']
    require(len(calls) <= 2048 and sum(len(b['barriers']) for b in calls) <= 8192, 'barrier capacity')
    tokens = set()
    for b in calls:
        token = b['token']; require(0 <= token < len(events) and token not in tokens, 'unique barrier token'); tokens.add(token)
        e = events[token]
        require(e['kind'] == 5 and e['object'] == b['command'] and e['enter'] == b['enter'] and b['returned'] and b['unchanged'], 'barrier/API link')
        resets = [r for r in events if r['kind'] == 0 and r['object'] == b['command'] and r['exit'] < e['enter']]
        require(resets and max(resets, key=lambda r: r['enter'])['enter'] == b['reset'], 'barrier recording generation')
    require(tokens == {i for i, e in enumerate(events) if e['kind'] == 5}, 'complete selected-barrier payloads')
    fences, output = set(), []
    for i, (r, s, request) in enumerate(zip(g['records'], j['samples'], requests)):
        require(r['receipt'] == s['receipt'] == i and r['captured'], 'captured receipt')
        require(r['frame'] == s['frame'] == request['source_frame'] and r['packet'] == s['packet'] == request['packet'], 'immutable request identity')
        require(r['command'] == s['command'] == request['list'] and r['reset'] == s['reset'] and r['allocator'] == s['allocator'] > 0 and r['marker'] == s['dispatch'], 'recording identity')
        require([v['resource'] for v in request['inputs']] == watched, 'pinned input identity')
        require([v['format'] for v in request['inputs']] == [26, 41, 42], 'qualified input formats')
        chain = [(s['reset'], 0), (s['dispatch'], 3), (s['close'], 1), (s['execute'], 2), (s['retire'], 0)]
        linked = []
        for at, kind in chain:
            require(at in order and order[at]['enter'] == at and order[at]['kind'] == kind, 'lifecycle event')
            e = order[at]
            require(e['object'] == (s['queue'] if kind == 2 else s['command']), 'lifecycle object')
            linked.append(e)
        reset, marker, close, execute, retire = linked
        require(reset['allocator'] == s['allocator'] and reset['exit'] < marker['enter'] < marker['exit'] == s['dispatch_exit'] < close['enter'] < close['exit'] < execute['enter'] < execute['exit'] == s['execute_exit'], 'record-close-submit order')
        require(execute['lists'].count(s['command']) == 1 and execute['exit'] < retire['enter'] < retire['exit'] == s['retire_exit'], 'submission and retirement')
        for e in events:
            if e['kind'] == 0 and e['object'] == s['command'] and reset['enter'] < e['enter'] < retire['enter']:
                require(False, 'unexpected intervening Reset')
            if e['kind'] == 2 and execute['enter'] < e['enter'] < retire['enter'] and s['command'] in e['lists']:
                require(False, 'duplicate submission')
        require(s['fence'] > 0 and s['fence'] not in fences and s['signal_attempts'] == 1 and s['signal_result'] == 0 and
                s['signaled'] and s['completed'] and s['completed_value'] == 1 and
                execute['exit'] < s['signal'] < s['signal_exit'] < s['completed_order'], 'private GPU completion chain')
        fences.add(s['fence'])
        cutoff = s['dispatch']
        if read_points:
            require(r['read_ready'] and r['copy_ok'], 'read point completion')
            following = sorted((b for b in calls if b['command'] == s['command'] and b['reset'] == s['reset'] and b['enter'] > s['dispatch']), key=lambda b:b['enter'])
            require(following and following[0]['token'] == r['read_token'] and following[0]['enter'] == r['read_point'], 'first relevant barrier after marker')
            b = following[0]; e = events[b['token']]
            require(marker['exit'] < e['enter'] < e['exit'] < close['enter'], 'read inside recording')
            require(len(b['barriers']) == 2 and {v.get('resource') for v in b['barriers']} == {watched[0],watched[2]} and
                    all(v == dict(type=0,flags=0,resource=v['resource'],subresource=0xffffffff,before=4,after=128) for v in b['barriers']), 'paired native read transition')
            cutoff = e['exit']
        state = [None] * 3; counts = [0] * 3; last = [0] * 3
        marker_states = None
        relevant = sorted((b for b in calls if b['command'] == s['command'] and b['reset'] == s['reset'] and b['enter'] < cutoff), key=lambda b: b['enter'])
        for b in relevant:
            require(events[b['token']]['exit'] <= cutoff, 'barrier before observation point')
            if read_points and b['enter'] > s['dispatch'] and marker_states is None:
                marker_states = [dict(known=x is not None,state=x if x is not None else 0,issue=0 if x is not None else 1,transitions=counts[k],last=last[k]) for k,x in enumerate(state)]
            for index, v in enumerate(b['barriers']):
                require(v['type'] in (0, 1, 2), 'known barrier kind')
                if v['type'] == 1:
                    require(v['before'] and v['after'] and v['before'] not in watched and v['after'] not in watched, 'alias invalidates state')
                elif v['type'] == 2:
                    require(v['flags'] == 0, 'UAV flags')
                elif v['resource'] in watched:
                    k = watched.index(v['resource'])
                    require(v['flags'] == 0 and v['subresource'] in (0, 0xffffffff), 'whole-image transition')
                    require(state[k] is None or state[k] == v['before'], 'transition state conflict')
                    state[k] = v['after']; counts[k] += 1; last[k] = b['enter'] * 1025 + index + 1
        declared_states = r['read_states'] if read_points else r['states']
        require(len(declared_states) == 3 and all(x is not None for x in state), 'state known in this recording')
        if read_points:
            require(r['states'] == marker_states and state == [128]*3, 'marker to read state chain')
        for k, declared in enumerate(declared_states):
            require(declared == dict(known=True, state=state[k], issue=0, transitions=counts[k], last=last[k]), 'independent state reconstruction')
        output.append(state)
    result = dict(gpu_completed_requests=8, retired_recordings=8,
                private_fences=8, gpu_completion_verified=True, gpu_states_verified=True,
                pixel_freshness_verified=False, cross_queue_input_writes_verified=False)
    result['input_states_at_read_point' if read_points else 'input_states_at_marker'] = output
    return result


def review(raw, dll_sha):
    result = deferred_review(raw, dll_sha, gpu=True)
    rows = parse(raw, gpu=True)
    result.update(review_trace(rows[13], rows[12]['requests']))
    return result


if __name__ == '__main__':
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--log', type=Path, required=True)
    ap.add_argument('--dll-sha256', required=True)
    ap.add_argument('--output', type=Path, required=True)
    a = ap.parse_args()
    require(not a.output.exists() and a.log.stat().st_size <= 4194304, 'artifact bounds')
    result = review(a.log.read_bytes(), a.dll_sha256)
    with a.output.open('x') as out:
        json.dump(result, out, indent=2)
    print(json.dumps(result, indent=2))
