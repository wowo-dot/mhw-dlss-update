"""Validate consumer-point planner observations, without granting GPU admission."""
from copy import deepcopy
from arm_continuous_dlaa import require
from continuous_depth import count, vector
from continuous_native_states import review as native


def review(status, previous=None):
    t = status['depth_schedule']
    require(t['schema'] == 1 and t['policy'] == 'depth-scheduled-consumer-observation-v1'
            and t['diagnostic_only'] is True, 'depth schedule policy')
    require(t['roles'] == ['scene_depth', 'color', 'temporal_output'] and t['capacity'] == 4
            and t['interval_ms'] == 1000, 'depth schedule bounds/roles')
    for key in ('checks', 'missing', 'captured'):
        count(t[key], 'depth schedule ' + key)
    lease = status['depth_eligibility']['lease']
    require(t['checks'] == lease['checks'] and t['missing'] == lease['rejected'], 'depth schedule coverage')
    require(t['missing'] <= t['checks'] and t['captured'] <= t['checks'], 'depth schedule count bounds')
    rows = t['samples']; require(type(rows) is list and len(rows) <= 4, 'depth schedule sample bound')
    slots, ordinals, candidates = set(), set(), []
    for s in rows:
        slot = count(s['slot'], 'depth schedule slot', 4)
        require(slot not in slots, 'depth schedule duplicate slot'); slots.add(slot)
        ordinal = count(s['ordinal'], 'depth schedule ordinal')
        require(0 < ordinal <= t['captured'] and ordinal not in ordinals, 'depth schedule ordinal range'); ordinals.add(ordinal)
        at = vector(s['at'], 9, 'depth schedule identity')
        require(all(at[i] > 0 for i in (0, 1, 2, 3, 5, 6, 7)), 'depth schedule positive identity')
        for key in ('missing', 'owner_stable', 'writer_stable', 'plan_equal', 'positions_equal'):
            require(type(s[key]) is bool, 'depth schedule observation flags')
        require(s['missing'] == (slot != 0), 'depth schedule slot class')
        owners = s['owners']; require(type(owners) is list and len(owners) == 3, 'depth schedule owner roles')
        for owner in owners:
            vector(owner, 6, 'depth schedule owner'); require(all(owner[:5]), 'depth schedule owner identity')
        expected, actual = (vector(s[k], 11, 'depth schedule writer') for k in ('expected_writer', 'actual_writer'))
        if s['writer_stable']:
            require(at[4] > 0 and at[8] > 0 and expected == actual and expected[:2] == [10, 8]
                    and expected[3] == owners[0][0], 'depth schedule stable writer agreement')
        for key in ('barrier_count', 'relevant'):
            count(s[key], 'depth schedule barriers', 129)
        barriers = s['barriers']
        require(s['relevant'] <= s['barrier_count'] and type(barriers) is list
                and len(barriers) == min(16, s['relevant']), 'depth schedule barrier bound')
        for b in barriers:
            vector(b, 5, 'depth schedule barrier'); require(b[0] <= 2, 'depth schedule barrier type')
        p, positions = s['plan'], s['positions']; native(p)
        for k in ('complete', 'stable'):
            require(type(positions[k]) is bool, 'depth schedule positions flag')
        scanned = count(positions['scanned'], 'depth schedule positions scanned', 32769)
        require(scanned <= min(p['packets'], 32768), 'depth schedule scan bound')
        if positions['complete']:
            require(0 < scanned == p['packets'] and p['readable'] and p['scan_complete']
                    and p['stable'] and p['matches'] == 1 and at[8] > 0, 'depth schedule full positions scan')
        if positions['stable']: require(positions['complete'], 'depth schedule stable complete scan')
        def position(v):
            vector(v, 2, 'depth schedule position')
            require(v[0] <= scanned and (v[1] < scanned if v[0] else v[1] == 2**32-1), 'depth schedule position range')
        position(positions['writer'])
        span_positions = positions['span_packets']
        require(type(span_positions) is list and len(span_positions) == 3, 'depth schedule span roles')
        for i, role in enumerate(span_positions):
            require(type(role) is list and len(role) == 16, 'depth schedule span bound')
            for j, pos in enumerate(role):
                position(pos)
                if j >= len(p['trackers'][i]['spans']): require(pos == [0, 2**32-1], 'depth schedule unused span')
        if (s['owner_stable'] and s['writer_stable'] and s['plan_equal'] and s['positions_equal']
                and p['stable'] and positions['stable'] and p['epoch'] == at[2]+1
                and positions['writer'][0] == 1 and positions['writer'][1] < p['index']):
            candidates.append(ordinal)
    require((0 in slots) == (t['checks'] > t['missing']), 'depth schedule baseline coverage')
    require((1 in slots) == (t['missing'] > 0) and (2 in slots) == (t['missing'] > 1), 'depth schedule first misses')
    require((max(ordinals) if ordinals else 0) == t['captured'], 'depth schedule latest capture retained')
    if 3 in slots:
        require(t['missing'] >= 3, 'depth schedule last miss coverage')
        first2 = next(s for s in rows if s['slot'] == 2); last = next(s for s in rows if s['slot'] == 3)
        require(last['at'][0] >= first2['at'][0]+1000 and last['ordinal'] > first2['ordinal'], 'depth schedule sample interval')
    if previous is not None:
        require(all(t[k] >= previous[k] for k in ('checks', 'missing', 'captured')), 'depth schedule counter rollback')
        current = {s['slot']: s for s in rows}
        for old in previous['samples']:
            require(old['slot'] in current, 'depth schedule lost sample')
            new = current[old['slot']]
            if old['slot'] != 3 or new['ordinal'] == old['ordinal']: require(new == old, 'depth schedule changed retained sample')
            else: require(new['ordinal'] > old['ordinal'] and new['at'][0] >= old['at'][0]+1000, 'depth schedule latest rollback')
    result = deepcopy(t)
    result['diagnostic_consistency_verified'] = True
    result['stable_writer_before_consumer_observed'] = candidates
    result['gpu_depth_admission_proven'] = False
    return result
