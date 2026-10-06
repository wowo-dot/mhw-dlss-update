"""Associate an observed draw with a submitted recording, never GPU completion.

State replay is deliberately limited to the prefix of that same Execute batch.
An earlier batch or another queue cannot silently provide missing state.
"""
from projection_provenance import require, uint, view_lifetime


def structure(p, t):
    registries = {}
    kinds = [('lists', 128), ('queues', 8)] + ([('fences', 32)] if t['schema'] >= 3 else [])
    for key, cap in kinds:
        rows = t.get(key)
        require(type(rows) is list and len(rows) <= cap, 'object registry capacity')
        require(len({v['object'] for v in rows}) == len(rows) and
                len({v['identity'] for v in rows}) == len(rows), 'duplicate object identity')
        for v in rows:
            require(all(uint(v.get(k)) and v[k] > 0 for k in ('object', 'identity', 'device', 'first')) and
                    v.get('ready') == 1 and v.get('pin') == 0 and v['device'] == p['device'], 'object pin/device')
            require(len(v.get('desc', [])) == 4 and all(uint(n, 32) for n in v['desc']) and
                    v['desc'][0] in ((0, 2, 3) if key == 'queues' else range(8) if key == 'fences' else (0, 1, 2, 3)), 'object description')
            if key != 'queues':
                require(v['desc'][1:] == [0, 0, 0], 'list description padding')
        registries[key] = {v['object']: v for v in rows}
    submitted = t.get('submitted')
    require(type(submitted) is list and uint(t.get('submitted_reserved'), 32) and
            len(submitted) == t['submitted_reserved'] <= 4096 and
            all(uint(v) and v in registries['lists'] for v in submitted), 'submitted list payload')
    require(uint(t.get('selected_submission')), 'selected submission order')
    coverage, first_uses = [], {key: {} for key, _ in kinds}
    for e in t['history']:
        key = 'fences' if e['kind'] in (13,14) else 'queues' if 10 <= e['kind'] <= 12 else 'lists'
        v = registries[key].get(e['object'])
        require(v is not None and e['identity'] == v['identity'] and v['first'] <= e['enter'], 'event object identity')
        first_uses[key].setdefault(e['object'], []).append(e['enter'])
        if e['kind'] in (11, 12):
            fence = registries['fences'].get(e['resource'])
            require(fence is not None and fence['identity'] == e['backing'] and fence['first'] <= e['enter'], 'queue fence identity')
            first_uses['fences'].setdefault(e['resource'], []).append(e['enter'])
        if 11 <= e['kind'] <= 14:
            require(e['count'] == e['first'] == 0, 'unexpected fence payload')
            if e['kind'] >= 13:
                require(e['resource'] == e['backing'] == 0, 'fence union')
            require(e['handle'] != 2**64-1, 'device removal/reserved fence value')
        if e['kind'] == 10:
            require(0 < e['count'] <= 128 and e['first'] + e['count'] <= len(submitted), 'submission slice')
            coverage.extend(range(e['first'], e['first'] + e['count']))
            batch = submitted[e['first']:e['first'] + e['count']]
            require(len(set(batch)) == len(batch), 'duplicate list in batch')
            for obj in batch:
                require(registries['lists'][obj]['first'] <= e['enter'], 'submitted object lifetime')
                first_uses['lists'].setdefault(obj, []).append(e['enter'])
    require(sorted(coverage) == list(range(len(submitted))), 'submission payload coverage')
    for key, rows in registries.items():
        for obj, v in rows.items():
            require(obj in first_uses[key] and v['first'] == min(first_uses[key][obj]), 'object first use')
    if t['selected_submission']:
        require(any(e['kind'] == 10 and e['enter'] == t['selected_submission'] for e in t['history']), 'selected submission absent')
    if t['schema'] >= 3:
        for k, kind in (('completion_signal', 11), ('completion_read', 13)):
            require(uint(t.get(k)) and (not t[k] or any(e['kind'] == kind and e['enter'] == t[k] for e in t['history'])), 'completion selector')


def batch(t, e):
    return t['submitted'][e['first']:e['first'] + e['count']]


def recording(t, command, submission):
    calls = [e for e in t['history'] if (e['kind'] < 10 or e['kind'] in (15,16)) and e['object'] == command]
    resets = [e for e in calls if e['kind'] == 8 and e['enter'] < submission['enter']]
    require(resets, 'submitted recording has no observed Reset')
    reset = max(resets, key=lambda e: e['enter'])
    require(reset['result'] < 0x80000000 and reset['resource'] > 0, 'submitted Reset failed')
    calls = sorted((e for e in calls if reset['enter'] <= e['enter'] < submission['exit']), key=lambda e: e['enter'])
    require(all(e['exit'] < submission['enter'] for e in calls), 'recording overlaps submission')
    require(all(a['exit'] < b['enter'] for a, b in zip(calls, calls[1:])), 'concurrent list recording')
    closes = [e for e in calls if e['kind'] == 5]
    require(len(closes) == 1 and closes[0]['result'] < 0x80000000 and calls[-1] is closes[0], 'missing/failed/nonterminal Close')
    require(all(e['kind'] in (5, 6, 8, 9, 15, 16) for e in calls), 'bundle state not covered')
    return reset, closes[0], calls


def association(t, m):
    executes = [e for e in t['history'] if e['kind'] == 10]
    found = [e for e in executes if e['enter'] == t['selected_submission']]
    require(len(found) == 1, 'selected submission not captured')
    selected = found[0]
    require(t['draw_return'] < selected['enter'] and m['command'] in batch(t, selected), 'draw/submission interval')
    queue = next(v for v in t['queues'] if v['object'] == selected['object'])
    require(queue['desc'][0] == 0, 'selected queue is not DIRECT')
    same_queue = sorted((e for e in executes if e['object'] == selected['object']), key=lambda e: e['enter'])
    require(all(a['exit'] < b['enter'] for a, b in zip(same_queue, same_queue[1:])), 'concurrent queue submissions')
    reset, close, calls = recording(t, m['command'], selected)
    require(reset['exit'] < m['bind_enter'] < m['order'] < t['draw_return'] < close['enter'], 'marker is not in submitted generation')
    require(all(e['exit'] < m['bind_enter'] or e['enter'] > t['draw_return'] for e in calls), 'recording overlaps selected draw/binding')
    owner = next(v for v in t['lists'] if v['object'] == m['command'])
    require(owner['desc'][0] == 0 and owner['first'] <= reset['enter'], 'selected list type/lifetime')
    # Submitting this same generation twice is not qualified by this recorder.
    for e in executes:
        if e is not selected and m['command'] in batch(t, e):
            if t['schema'] >= 3 and e['enter'] > selected['exit']:
                later, _, _ = recording(t, m['command'], e)
                require(later['enter'] > selected['exit'], 'selected recording submitted more than once')
                continue
            require(e['exit'] < reset['enter'], 'selected recording submitted more than once')
    return selected, reset, close


def declared_state(p, t, m, created, sub, resource, base, submission):
    # A destruction/overwrite after CPU draw but before submission also breaks
    # this bounded descriptor association. No claim is made beyond this window.
    view_lifetime(p, t, dict(m, order=submission['exit']), resource, base)
    prefix = batch(t, submission)
    prefix = prefix[:prefix.index(m['command']) + 1]
    events, generations = [], []
    for obj in prefix:
        reset, close, calls = recording(t, obj, submission)
        owner = next(v for v in t['lists'] if v['object'] == obj)
        require(owner['desc'][0] == 0 and created['exit'] < reset['enter'], 'batch prefix type/resource lifetime')
        generations.append(dict(command=obj, reset=reset['enter'], close=close['enter']))
        events.extend(e for e in calls if obj != m['command'] or e['exit'] < m['order'])
    # Other queues require observed synchronization, which schema 2 does not
    # capture. Refuse to merge them even if wall-clock/CPU order looks plausible.
    require(all(e['object'] == submission['object'] for e in t['history'] if e['kind'] == 10 and
                e['enter'] < submission['exit']), 'other queue submission requires synchronization evidence')
    state = replay_depth(t, m, sub, events)
    return dict(submitted_depth_state_declared=True, submitted_declared_state=state['state'],
                submitted_depth_transitions=state['transitions'], submitted_depth_clears=state['clears'],
                submitted_prefix_generations=generations)


def replay_depth(t, m, sub, events):
    state, split, transitions, clears = None, None, 0, 0
    for e in events:
        if e['kind'] == 6 and e['handle'] == m['handle']:
            require(e['count'] == 0 and split is None and state in (None, 0x10), 'batch clear conflicts with recorded state')
            clears += 1
        if e['kind'] != 9:
            continue
        for kind, flags, res, other, index, before, after in t['barriers'][e['first']:e['first'] + e['count']]:
            if kind == 1:
                require(res not in (0, m['resource']) and other not in (0, m['resource']), 'batch relevant aliasing barrier')
            if kind != 0 or res != m['resource'] or index not in (sub, 0xffffffff):
                continue
            if flags == 2:
                require(split == (index, before, after), 'batch split end without begin')
                split = None; state = after; transitions += 1
            else:
                require(split is None and state in (None, before), 'batch transition continuity/split overlap')
                if flags == 1:
                    split = (index, before, after)
                else:
                    state = after; transitions += 1
    require(split is None and transitions > 0 and state == 0x10, 'batch DEPTH_WRITE declaration missing')
    return dict(state=state, transitions=transitions, clears=clears)



def validate(p, t, m, created, sub, resource, base):
    result = dict(selected_recording_submitted=False, submitted_depth_state_declared=False,
                  gpu_completion_verified=False, submission_reasons=[])
    try:
        submission, reset, close = association(t, m)
        result.update(selected_recording_submitted=True, submission_order=submission['enter'],
                      submission_queue=submission['object'], submission_queue_identity=submission['identity'],
                      submission_batch_index=batch(t, submission).index(m['command']),
                      submission_reset_order=reset['enter'], submission_close_order=close['enter'])
        result.update(declared_state(p, t, m, created, sub, resource, base, submission))
    except ValueError as e:
        result['submission_reasons'].append(str(e))
    return result
