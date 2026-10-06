"""Replay full depth observations and, when present, destination admission."""
from copy import deepcopy
from arm_continuous_dlaa import require

FIELDS = ('container', 'srv', 'cpu_descriptor', 'attachment', 'attachment_owner',
          'attachment_resource', 'texture.id', 'texture.kind', 'texture.flags',
          'texture.width', 'texture.height', 'texture.format', 'texture.gpu_format',
          'texture.object', 'texture.type', 'texture.primary', 'texture.secondary',
          'texture.backing', 'texture.resource', 'attachment_readable', 'srv_readable')
LEASE_FIELDS = ('list', 'generation', 'frame', 'depth_available', 'already_evaluated')
IDENTITY_FIELDS = ('tick', 'frame', 'epoch', 'sequence', 'packet', 'list', 'generation',
                   'view', 'context', 'bank', 'async')
ATTACHMENT_MASK = (1 << 3) | (1 << 4) | (1 << 5) | (1 << 19)


def count(value, name, maximum=2**64-1):
    require(type(value) is int and 0 <= value < maximum, name)
    return value


def vector(value, size, name):
    require(type(value) is list and len(value) == size, name)
    for item in value:
        count(item, name)
    return value


def difference(a, b):
    return sum(1 << i for i, (x, y) in enumerate(zip(a, b)) if x != y)


def samples(part, kind):
    total = part['rejected']
    rows = part['samples']
    ordinals = list(range(1, min(total, 8) + 1)) + ([total] if total > 8 else [])
    require(type(rows) is list and [s['ordinal'] for s in rows] == ordinals, kind + ' bounded ordinals')
    retained_counts = [0] * (21 if kind == 'scene' else 5)
    unreadable = mismatches = 0
    last_tick = 0
    for row in rows:
        at = vector(row['at'], 11, 'depth identity')
        require(all(at[i] > 0 for i in range(9)) and at[9] <= 1 and at[10] <= 1, 'depth identity range')
        require(at[0] >= last_tick, 'depth receipt time rollback')
        last_tick = at[0]
        mask = count(row['difference'], 'depth difference', 1 << len(retained_counts))
        actual = vector(row['actual'], len(retained_counts), 'depth observed fields')
        if kind == 'scene':
            expected = vector(row['expected'], 21, 'depth expected fields')
            require(all(v[i] in (0, 1) for v in (actual, expected) for i in (19, 20)), 'depth readability fields')
            require(all(v[i] < 2**32 for v in (actual, expected) for i in range(6, 13)), 'depth owner integers')
            require(type(row['readable']) is bool and row['equal'] is False, 'scene rejection result')
            calls, failures, first, address, size = vector(row['reads'], 5, 'read observations')
            require(0 <= failures <= calls <= 64, 'bounded depth reads')
            if failures:
                require(0 < first <= calls and size in (4, 8) and address > 0, 'failed read identity')
            else:
                require((first, address, size) == (0, 0, 0), 'absent failed read')
            if row['readable']:
                require(mask != 0 and mask == difference(expected, actual), 'scene field difference')
                mismatches += 1
            else:
                require(mask == 0, 'partial reads cannot classify field differences')
                unreadable += 1
        else:
            require(actual[3] in (0, 1) and actual[4] in (0, 1), 'lease booleans')
            require(mask != 0 and mask == difference([at[5], at[6], at[1], 1, 0], actual), 'lease predicate difference')
        for i in range(len(retained_counts)):
            retained_counts[i] += bool(mask & (1 << i))
    omitted = max(0, total - len(rows))
    for measured, stored in zip(part['differences'], retained_counts):
        require(stored <= measured <= stored + omitted, kind + ' difference coverage')
    if kind == 'scene':
        require(unreadable <= part['unreadable'] <= unreadable + omitted and
                mismatches <= part['mismatches'] <= mismatches + omitted, 'scene result coverage')


def review_destination(status, previous=None):
    trace = status['depth_destination']
    require(trace['schema'] == 1 and trace['policy'] == 'depth-destination-identity-v1', 'depth destination policy')
    for key in ('checks', 'accepted', 'rejected', 'attachment_only'):
        count(trace[key], 'destination ' + key)
    require(trace['checks'] == trace['accepted'] + trace['rejected'] and
            trace['attachment_only'] <= trace['accepted'], 'destination partition')
    scene = status['depth_eligibility']['scene']
    require(trace['checks'] == scene['checks'] and trace['accepted'] == scene['passes'] + trace['attachment_only'] and
            trace['rejected'] + trace['attachment_only'] == scene['rejected'], 'destination observation agreement')
    known_accepted = sum(row['readable'] and row['difference'] != 0 and row['difference'] & ~ATTACHMENT_MASK == 0
                         for row in scene['samples'])
    known_rejected = len(scene['samples']) - known_accepted
    omitted = scene['rejected'] - len(scene['samples'])
    require(known_accepted <= trace['attachment_only'] <= known_accepted + omitted and
            known_rejected <= trace['rejected'] <= known_rejected + omitted, 'destination decision coverage')
    rows, total = trace['samples'], trace['attachment_only']
    ordinals = list(range(1, min(total, 8) + 1)) + ([total] if total > 8 else [])
    require(type(rows) is list and [s['ordinal'] for s in rows] == ordinals, 'destination bounded ordinals')
    last_tick = 0
    for row in rows:
        at = vector(row['at'], 11, 'destination identity')
        require(all(at[i] > 0 for i in range(9)) and at[9] <= 1 and at[10] <= 1, 'destination identity range')
        require(at[0] >= last_tick, 'destination time rollback')
        last_tick = at[0]
        expected, actual = [vector(row[k], 21, 'destination fields') for k in ('expected', 'actual')]
        require(all(v[i] in (0, 1) for v in (expected, actual) for i in (19, 20)), 'destination readability')
        require(all(v[i] < 2**32 for v in (expected, actual) for i in range(6, 13)), 'destination owner integers')
        require(all(v[i] > 0 for v in (expected, actual) for i in (0, 9, 10, 13, 14, 17, 18)), 'destination core identity')
        mask = count(row['difference'], 'destination difference', 1 << 21)
        require(mask != 0 and mask == difference(expected, actual) and mask & ~ATTACHMENT_MASK == 0,
                'only attachment changes accepted')
        for observation in scene['samples']:
            if observation['at'] == at:
                require(observation['readable'] is True and observation['equal'] is False and
                        all(observation[k] == row[k] for k in ('expected', 'actual', 'difference')), 'paired depth evidence')
    if previous is not None:
        require(previous['policy'] == trace['policy'], 'destination policy changed')
        require(all(trace[k] >= previous[k] for k in ('checks', 'accepted', 'rejected', 'attachment_only')), 'destination rollback')
        current = {s['ordinal']: s for s in rows}
        for row in previous['samples']:
            if row['ordinal'] in current:
                require(row == current[row['ordinal']], 'destination receipt changed')
    result = deepcopy(trace)
    result['destination_identity_verified'] = True
    result['sdk_success_per_admitted_sample_verified'] = False
    return result


def review(status, previous=None, *, scaled=False):
    trace = status['depth_eligibility']
    require(trace['schema'] == 1 and trace['policy'] == 'depth-eligibility-observation-v1' and
            trace['diagnostic_only'] is True, 'depth diagnostic policy')
    scene, lease = trace['scene'], trace['lease']
    for part, size, name in ((scene, 21, 'scene'), (lease, 5, 'lease')):
        for key in ('checks', 'passes', 'rejected'):
            count(part[key], name + ' ' + key)
        vector(part['differences'], size, name + ' difference counters')
        require(part['checks'] == part['passes'] + part['rejected'], name + ' partition')
    for key in ('unreadable', 'mismatches'):
        count(scene[key], key)
    require(scene['rejected'] == scene['unreadable'] + scene['mismatches'], 'scene rejection partition')
    destination = review_destination(status, previous.get('destination') if previous else None) if 'depth_destination' in status else None
    scene_passes = destination['accepted'] if destination else scene['passes']
    scene_rejected = destination['rejected'] if destination else scene['rejected']
    require(lease['checks'] <= scene_passes, 'depth stage ordering')
    require(count(status['frames'], 'frames') <= lease['passes'], 'SDK depth coverage')
    reasons = vector(status['dispatch_reasons'], 12, 'dispatch reasons')
    require(scene_rejected == reasons[3], 'scene depth skip coverage')
    # v1.25.0..2 also use reason 7 for stream-route readiness, after the
    # depth lease passed. Keep the original exact equality for DLAA logs.
    readiness_skips = reasons[7] - lease['rejected']
    if scaled:
        require(0 <= readiness_skips <= lease['passes'] - status['frames'], 'scaled readiness skip coverage')
        if previous is not None:
            require(readiness_skips >= previous['scaled_readiness_skips'], 'scaled readiness skip rollback')
    else:
        require(readiness_skips == 0, 'depth skip coverage')
    samples(scene, 'scene')
    samples(lease, 'lease')
    if previous is not None:
        require(previous['policy'] == trace['policy'], 'depth policy changed')
        require(('destination' in previous) == (destination is not None), 'destination channel changed')
        for key in ('scene', 'lease'):
            old, new = previous[key], trace[key]
            for field in ('checks', 'passes', 'rejected') + (('unreadable', 'mismatches') if key == 'scene' else ()):
                require(new[field] >= old[field], 'depth counter rollback')
            require(all(a >= b for a, b in zip(new['differences'], old['differences'])), 'depth field count rollback')
            current = {s['ordinal']: s for s in new['samples']}
            for row in old['samples']:
                if row['ordinal'] in current:
                    require(row == current[row['ordinal']], 'depth receipt changed')
    result = deepcopy(trace)
    result['depth_eligibility_verified'] = True
    result['scene_changed_fields'] = {k: n for k, n in zip(FIELDS, scene['differences']) if n}
    result['lease_failed_predicates'] = {k: n for k, n in zip(LEASE_FIELDS, lease['differences']) if n}
    result['independent_readback_verified'] = False
    if scaled:
        result['scaled_readiness_skips'] = readiness_skips
    if destination is not None:
        result['destination'] = destination
    return result
