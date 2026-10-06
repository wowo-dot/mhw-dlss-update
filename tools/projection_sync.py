"""Bounded fence observations and same-queue declarations; not GPU safety proof."""
from projection_provenance import require, view_lifetime
from projection_submission import association, batch, recording, replay_depth


def same_queue_state(p, t, m, created, sub, resource, base, selected):
    require(resource['dimension'] == 3 and resource['format'] == 39 and
            resource['flags'] & 2 and not resource['flags'] & 0x20, 'unsupported state-decay contract')
    view_lifetime(p, t, dict(m, order=selected['exit']), resource, base)
    earlier = [e for e in t['history'] if e['kind'] == 10 and e['object'] == selected['object'] and e['exit'] < selected['enter']]
    previous = max(earlier, key=lambda e: e['enter']) if earlier else None
    submissions = ([previous] if previous else []) + [selected]
    events, generations = [], []
    for execute in submissions:
        prefix = batch(t, execute)
        if execute is selected: prefix = prefix[:prefix.index(m['command']) + 1]
        for obj in prefix:
            reset, close, calls = recording(t, obj, execute)
            owner = next(v for v in t['lists'] if v['object'] == obj)
            require(owner['desc'][0] == 0 and created['exit'] < reset['enter'], 'same-queue resource lifetime/type')
            generations.append(dict(submission=execute['enter'], command=obj, reset=reset['enter'], close=close['enter']))
            events.extend(e for e in calls if execute is not selected or obj != m['command'] or e['exit'] < m['order'])
    state = replay_depth(t, m, sub, events)
    return dict(same_queue_depth_state_declared=True, same_queue_declared_state=state['state'],
                same_queue_depth_transitions=state['transitions'], same_queue_depth_clears=state['clears'],
                same_queue_generations=generations, same_queue_scope='immediate previous Execute plus selected batch prefix')


def fence_pairs(t):
    # These are exact-value API correlations, not a proof that there were no
    # unobserved/higher competing writers or a complete cross-queue dependency graph.
    signals = [e for e in t['history'] if e['kind'] == 11 and e['result'] < 0x80000000]
    pairs, unmatched = [], []
    for wait in (e for e in t['history'] if e['kind'] == 12):
        matches = [s for s in signals if s['resource'] == wait['resource'] and s['handle'] == wait['handle'] and s['exit'] < wait['enter']]
        if wait['result'] < 0x80000000 and len(matches) == 1:
            s = matches[0]
            pairs.append(dict(signal=s['enter'], wait=wait['enter'], fence=s['resource'], identity=s['backing'],
                              value=s['handle'], signal_queue=s['object'], wait_queue=wait['object']))
        else: unmatched.append(wait['enter'])
    return dict(recorded_exact_value_fence_pairs=pairs, unmatched_queue_waits=unmatched,
                cross_queue_resource_safety_verified=False)


def completion_queue_intervals(queue, selected, signal):
    # A later Execute can synchronously call Wait on the same queue/thread.
    # Both intervals must be strictly after the selected Execute and before
    # the qualifying Signal. No ordering inside that later batch is inferred.
    stack, nested = [], []
    for e in sorted(queue, key=lambda e:e['enter']):
        require(e['kind'] == 10 or e['result'] < 0x80000000, 'failed/overlapping queue operation')
        while stack and stack[-1]['exit'] < e['enter']: stack.pop()
        if stack:
            outer = stack[-1]
            require(len(stack) == 1 and outer['kind'] == 10 and e['kind'] == 12 and
                    selected['exit'] < outer['enter'] < e['enter'] < e['exit'] < outer['exit'] < signal['enter'] and
                    outer['thread'] == e['thread'] and outer['object'] == e['object'] == selected['object'],
                    'failed/overlapping queue operation')
            nested.append(dict(execute=outer['enter'], wait=e['enter'], thread=e['thread']))
        stack.append(e)
    return nested


def completion(t, selected):
    signals = sorted((e for e in t['history'] if e['kind'] == 11 and e['object'] == selected['object'] and
                      e['enter'] > selected['exit']), key=lambda e:e['enter'])
    require(signals, 'no selected-queue signal after submission')
    signal = signals[0]
    require(signal['result'] < 0x80000000 and 0 < signal['handle'] < 2**64-1 and
            t['completion_signal'] == signal['enter'], 'failed/ambiguous completion signal')
    fence = next(v for v in t['fences'] if v['object'] == signal['resource'])
    require(fence['desc'][0] == 0, 'shared fence cannot qualify local completion observation')
    reads = sorted((e for e in t['history'] if e['kind'] == 13 and e['object'] == signal['resource']), key=lambda e:e['enter'])
    require(all(a['exit'] < b['enter'] and a['handle'] <= b['handle'] for a,b in zip(reads,reads[1:])), 'overlapping/rewound completion reads')
    baseline = [e for e in reads if e['exit'] < signal['enter'] and e['handle'] < signal['handle']]
    require(baseline, 'no observed below-target fence baseline')
    completed = [e for e in reads if e['enter'] > signal['exit'] and signal['handle'] <= e['handle'] < 2**64-1]
    require(completed and t['completion_read'] == completed[0]['enter'], 'completion threshold not observed')
    read = completed[0]
    writers = sorted((e for e in t['history'] if e['enter'] < read['exit'] and
                      ((e['kind'] == 11 and e['resource'] == signal['resource']) or
                       (e['kind'] == 14 and e['object'] == signal['resource']))), key=lambda e:e['enter'])
    require(all(e['kind'] == 11 and e['object'] == selected['object'] and e['result'] < 0x80000000 for e in writers), 'competing/CPU/failed fence writer')
    require(all(a['exit'] < b['enter'] and a['handle'] < b['handle'] for a,b in zip(writers,writers[1:])), 'nonmonotonic/overlapping fence writers')
    queue = sorted((e for e in t['history'] if e['kind'] in (10,11,12) and e['object'] == selected['object'] and
                    e['exit'] >= selected['enter'] and e['enter'] <= signal['exit']),key=lambda e:e['enter'])
    nested = completion_queue_intervals(queue, selected, signal)
    result = dict(selected_queue_fence_completion_observed=True, completion_queue=selected['object'],
                completion_fence=signal['resource'], completion_fence_identity=signal['backing'],
                completion_signal_order=signal['enter'], completion_signal_value=signal['handle'],
                completion_read_order=read['enter'], completion_read_value=read['handle'],
                completion_baseline_order=baseline[-1]['enter'],
                completion_scope='recorded window only; pre-window writers and complete resource access coverage are not established')
    if nested: result['completion_later_execute_nested_waits'] = nested
    return result


def validate(p, t, m, created, sub, resource, base):
    result = dict(same_queue_depth_state_declared=False, selected_queue_fence_completion_observed=False,
                  synchronization_reasons=[], **fence_pairs(t))
    try: selected, _, _ = association(t, m)
    except ValueError as e:
        result['synchronization_reasons'].append(str(e)); return result
    for action in (lambda:same_queue_state(p,t,m,created,sub,resource,base,selected), lambda:completion(t,selected)):
        try: result.update(action())
        except ValueError as e: result['synchronization_reasons'].append(str(e))
    return result
