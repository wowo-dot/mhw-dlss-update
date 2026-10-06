"""Bind the reused native state proof to this exact scaled capture callback."""
from continuous_admission import review as replay


def require(ok, reason):
    if not ok:
        raise ValueError('color admission: '+reason)


def review(cpu, native, submission):
    a = native['color_admission']
    for key in ('attempted', 'accepted', 'packet_revalidated'):
        require(type(a[key]) is bool, key)
    require(type(a['reset']) is int and a['reset'] >= 0, 'reset')
    proof = replay(a)
    local = native['states'][0]
    if not a['attempted']:
        require(not a['accepted'] and not a['packet_revalidated'] and a['reset'] == 0 and
                proof['attempts'] == 0, 'unused proof')
        require(local == dict(known=True, conflict=False, value=64), 'missing local state')
        return 'local-transition'
    require(local == dict(known=False, conflict=False, value=0), 'local evidence conflict')
    require(a['accepted'] and a['packet_revalidated'] and proof['attempts'] == proof['accepted'] == 1 and
            proof['rejected'] == proof['evidence']['omitted'] == 0, 'proof closure')
    s = proof['evidence']['samples'][0]
    require(s['frame'] == cpu['frame'] and s['epoch'] == cpu['epoch'] and
            s['packet'] == cpu['packet'] == native['packet'] and
            s['sequence'] == native['publication'] and
            s['generation'] == a['reset'] == submission['reset'] and
            s['list'] == native['command'] == submission['command'], 'frame/recording identity')
    require(native['enter_tick'] <= s['tick'] <= native['exit_tick'], 'callback time')
    require(s['provenance']['renderer'] == cpu['context']-0x710, 'renderer identity')
    for evidence, role in zip(s['resources'], (0, 4, 5)):
        owner = cpu['inputs'][role]
        require(all(evidence[left] == owner[right] for left, right in (
            ('object', 'owner'), ('backing', 'backing'), ('resource', 'resource'),
            ('width', 'engine_width'), ('height', 'engine_height'), ('format', 'gpu_format'))), 'resource identity')
    resources = [cpu['inputs'][role]['resource'] for role in (0, 4, 5)]
    def mask(resource):
        return sum(1 << i for i, value in enumerate(resources) if value == resource) if resource else 8
    rows = []
    count = 0
    for call in native['barrier_calls']:
        if call['exit'] >= native['dispatch_enter']:
            continue
        for b in native['barriers'][call['first']:call['first']+call['count']]:
            m, other = mask(b['resource']), mask(b['other']) if b['type'] == 1 else 0
            if m or other:
                rows.append([count, b['type'], b['flags'], b['subresource'] if b['type'] == 0 else 0,
                             b['before'] if b['type'] == 0 else 0, b['after'] if b['type'] == 0 else 0, m, other])
            count += 1
    require(s['barrier_count'] == count and s['relevant_barriers'] == len(rows) and
            s['barriers'] == rows, 'callback barrier evidence')
    return 'native-current-use-v1'
