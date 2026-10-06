"""Independently check bounded request-lifetime observations; no acceptance of faults."""


def require(ok, why):
    if not ok:
        raise ValueError('requests: ' + why)


def uint(value, bits=64):
    return type(value) is int and 0 <= value < 2**bits


def identity(value):
    wide = ('frame', 'epoch', 'sequence', 'microseconds', 'packet', 'state', 'view',
            'context', 'layer', 'depth', 'bypasses', 'last_bypass_tick', 'last_bypass_caller')
    for key in wide:
        require(uint(value[key]), 'identity ' + key)
    for key in ('bank', 'async', 'last_bypass_publishers'):
        require(uint(value[key], 32), 'identity ' + key)
    require(value['bank'] <= 1 and value['async'] <= 1, 'bank/async')
    if value['bypasses']:
        require(value['last_bypass_tick'] > 0 and value['last_bypass_publishers'] > 0, 'bypass evidence')
    else:
        require(all(value[k] == 0 for k in ('last_bypass_tick', 'last_bypass_caller', 'last_bypass_publishers')), 'empty bypass')


def sample(value, calls, kind, capacity):
    for key in ('ordinal', 'tick', 'packet', 'caller'):
        require(uint(value[key]), 'sample ' + key)
    for key in ('publishers', 'live', 'matches'):
        require(uint(value[key], 32), 'sample ' + key)
    require(0 < value['ordinal'] <= calls and value['tick'] > 0, 'sample order')
    require(value['matches'] <= value['live'] <= capacity, 'sample bounded matches')
    identity(value['newest'])
    if value['matches']:
        require(value['newest']['packet'] == value['packet'] and
                all(value['newest'][k] > 0 for k in ('frame', 'epoch', 'sequence')), 'matching identity')
    else:
        require(all(v == 0 for v in value['newest'].values()), 'empty matching identity')
    if kind == 'blocked':
        require(value['publishers'] > 0, 'blocked publisher count')
    else:
        require(value['publishers'] == 0, 'unblocked publisher count')
        require(value['matches'] == 0 if kind == 'unmatched' else value['matches'] > 1, 'sample category')


def review(value, fault=None):
    require(value['schema'] == 1 and value['policy'] == 'observe-existing-request-lifetime', 'schema/policy')
    require(value['capacity'] == 32, 'capacity')
    for key in ('publication_attempts', 'published', 'collisions'):
        require(uint(value[key]), 'counter ' + key)
    require(value['publication_attempts'] == value['published'] + value['collisions'], 'publication partition')
    for name, capacity in (('taa', 32), ('depth', 64)):
        counts = value[name]
        for key in ('calls', 'blocked', 'unmatched', 'matched', 'changed', 'reentrant', 'multiple'):
            require(uint(counts[key]), 'consumer counter ' + key)
        require(counts['calls'] == sum(counts[k] for k in ('blocked', 'unmatched', 'matched', 'reentrant')), 'consumer partition')
        require(counts['changed'] <= counts['matched'] and counts['multiple'] <= counts['matched'], 'match subsets')
        for category in ('blocked', 'unmatched', 'multiple'):
            first, last = counts['first_' + category], counts['last_' + category]
            if not counts[category]:
                require(first is None and last is None, 'empty category samples')
                continue
            for entry in (first, last):
                sample(entry, counts['calls'], category, capacity)
            require(first['ordinal'] <= last['ordinal'] and first['tick'] <= last['tick'], 'first/latest order')
            require(counts[category] <= last['ordinal'] - first['ordinal'] + 1, 'sample count interval')
            if counts[category] == 1:
                require(first == last, 'single event')
    collision = value['first_collision']
    classification = None
    if value['collisions']:
        require(collision is not None, 'collision missing')
        for key in ('tick', 'publication_attempt', 'lease_list', 'lease_generation', 'lease_frame'):
            require(uint(collision[key]), 'collision ' + key)
        for key in ('slot', 'live', 'publishers', 'pending_callbacks'):
            require(uint(collision[key], 32), 'collision ' + key)
        for key in ('lease_depth', 'lease_evaluated'):
            require(type(collision[key]) is bool, 'collision ' + key)
        require(collision['tick'] > 0 and 0 < collision['publication_attempt'] <= value['publication_attempts'], 'collision timing')
        require(0 <= collision['slot'] < 32 and 0 < collision['live'] <= 32, 'collision capacity')
        old, new = collision['old_request'], collision['new_request']
        for entry in (old, new):
            identity(entry)
            require(all(entry[k] > 0 for k in ('frame', 'epoch', 'packet', 'state', 'view', 'context', 'layer')), 'collision identity')
            require(entry['frame'] % 32 == collision['slot'], 'collision slot')
        require(old['sequence'] > 0 and new['sequence'] == 0 and value['published'] > 0, 'publication identities')
        classification = ('same-frame' if new['frame'] == old['frame'] else
                          'forward-slot-reuse' if new['frame'] > old['frame'] else 'backward-frame')
    else:
        require(collision is None, 'unexpected collision')
    if fault is not None:
        require(uint(fault, 32), 'fault type')
        require(fault != 6 or value['collisions'] > 0, 'fault without collision')
        require(fault != 0 or value['collisions'] == 0, 'collision without fault')
    return dict(publication_attempts=value['publication_attempts'], published=value['published'],
                collisions=value['collisions'], collision_classification=classification,
                taa={k: value['taa'][k] for k in ('calls', 'blocked', 'matched', 'unmatched', 'changed', 'multiple', 'reentrant')},
                depth={k: value['depth'][k] for k in ('calls', 'blocked', 'matched', 'unmatched', 'changed', 'multiple', 'reentrant')})
