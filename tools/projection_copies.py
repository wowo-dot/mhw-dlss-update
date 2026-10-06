"""Native copy arguments and recording association; no GPU contents assertion."""
from projection_provenance import require, uint
from projection_submission import recording, batch


def structure(p, t):
    require(p.get('copy_capacity') == 512 and p.get('copy_resource_capacity') == 128, 'copy bounds')
    rows, copies = t.get('copy_resources'), t.get('copies')
    require(type(rows) is list and len(rows) <= 128 and type(copies) is list and
            uint(t.get('copies_reserved'), 32) and len(copies) == t['copies_reserved'] <= 512, 'copy storage')
    if p.get('copy_retention') == 'per-call-until-drain':
        require(uint(t.get('copy_refs_acquired'),32) and uint(t.get('copy_refs_released'),32) and
                t['copy_refs_acquired'] == t['copy_refs_released'] == 2*len(copies), 'per-call copy reference balance')
    require(len({r['object'] for r in rows}) == len(rows) and
            len({r['identity'] for r in rows}) == len(rows), 'copy resource identity uniqueness')
    resources = {r['object']: r for r in rows}
    for r in rows:
        require(all(uint(r.get(k)) and r[k] > 0 for k in ('object', 'identity', 'device', 'first')) and
                r['device'] == p['device'] and r.get('ready') == 1 and r.get('pin') == 0, 'copy resource pin/device')
        d = r.get('desc')
        require(type(d) is list and len(d) == 11 and all(uint(v) for v in d) and
                d[0] in (1, 2, 3, 4) and all(d[i] > 0 for i in (2, 3, 4, 5, 7)) and
                all(uint(d[i], 32) for i in (0, 3, 6, 7, 8, 9, 10)) and
                uint(d[4], 16) and uint(d[5], 16), 'copy resource description')
    coverage, uses = [], {obj: [] for obj in resources}
    for e in t['history']:
        if e['kind'] not in (15, 16):
            continue
        require(e['count'] == 1 and e['first'] < len(copies) and e['handle'] == 0 and
                e['descriptor'] == [0]*6 and e['recipe'] == [0]*5 and not e['null_desc'], 'copy event payload')
        coverage.append(e['first']); c = copies[e['first']]
        require(type(c.get('has_box')) is bool and len(c.get('box', [])) == 6 and len(c.get('xyz', [])) == 3 and
                all(uint(v, 32) for v in c['box'] + c['xyz']), 'copy coordinates')
        require(c['has_box'] or c['box'] == [0]*6, 'unused copy box')
        if c['has_box']:
            require(all(c['box'][i] < c['box'][i+3] for i in range(3)), 'empty copy box')
        for key, field in [('source', 'resource'), ('destination', 'backing')]:
            loc = c.get(key)
            require(type(loc) is list and len(loc) == 8 and all(uint(v) for v in loc) and
                    loc[0] == e[field] and loc[0] in resources and loc[1] in (0, 1, 2), 'copy location')
            uses[loc[0]].append(e['enter'])
            d = resources[loc[0]]['desc']
            if loc[1] == 0:
                require(d[0] in (2, 3, 4) and uint(loc[2], 32) and loc[3:] == [0]*5, 'copy subresource location')
            elif loc[1] == 1:
                require(d[0] == 1 and all(uint(v, 32) for v in loc[3:]) and all(v > 0 for v in loc[4:]) and
                        loc[2] % 512 == 0 and loc[7] % 256 == 0, 'copy placed footprint')
            else:
                require(loc[2:] == [0]*6, 'whole resource union')
        require((c['source'][1] == c['destination'][1] == 2 and not c['has_box'] and c['xyz'] == [0]*3)
                if e['kind'] == 16 else (c['source'][1] != 2 and c['destination'][1] != 2), 'copy API kind')
    require(sorted(coverage) == list(range(len(copies))), 'copy payload coverage')
    for obj, r in resources.items():
        require(uses[obj] and min(uses[obj]) == r['first'], 'copy resource first use')


def local_copy_state(t, calls, event, resource):
    state, split, transitions = None, None, []
    for e in calls:
        if e['enter'] >= event['enter']:
            break
        if e['kind'] != 9:
            continue
        for kind, flags, res, other, sub, before, after in t['barriers'][e['first']:e['first']+e['count']]:
            if kind == 1:
                require(res not in (0, resource) and other not in (0, resource), 'copy aliasing barrier')
            if kind != 0 or res != resource or sub not in (0, 0xffffffff):
                continue
            if flags == 2:
                require(split == (sub, before, after), 'copy unmatched split end')
                split = None; state = after
            else:
                require(split is None and state in (None, before), 'copy barrier continuity')
                if flags == 1:
                    split = (sub, before, after)
                else:
                    state = after
            transitions.append(e['enter'])
    require(split is None and state is not None and state & 0x800 and not state & ~0x8e0,
            'local COPY_SOURCE declaration missing or incompatible')
    return dict(local_copy_source_state=state, local_copy_barriers=transitions)


def validate(p, t, m, resource, base=None):
    rows = {r['object']: r for r in t['copy_resources']}
    if base is not None:
        for e in t['history']:
            if e['kind'] in (15, 16):
                require(e['caller']-base in ((0x259e0e2, 0x259e1a8) if e['kind'] == 15 else (0x259e100,)),
                        'native copy call site')
    result = dict(depth_copy_calls=[], depth_copy_readback_ready=False)
    for e in t['history']:
        if e['kind'] not in (15, 16) or m['resource'] not in (e['resource'], e['backing']):
            continue
        c = t['copies'][e['first']]; source = rows[e['resource']]; dest = rows[e['backing']]
        row = dict(order=e['enter'], caller=e['caller'], command=e['object'], kind=e['kind'],
                   source=source, destination=dest, arguments=c, recording_submitted=False,
                   full_depth_source=False, reasons=[])
        result['depth_copy_calls'].append(row)
        try:
            selected = rows[m['resource']]
            require(selected['identity'] == m['identity'], 'selected copy identity mismatch')
            require(selected['desc'][0] == 3 and selected['desc'][4:8] == [1, 1, 39, 1] and
                    selected['desc'][10] & 2, 'selected copy depth description')
            for key, index in [('dimension',0),('alignment',1),('width',2),('height',3),('depth_or_array',4),
                               ('mips',5),('format',6),('samples',7),('quality',8),('layout',9),('flags',10)]:
                if key in resource:
                    require(resource[key] == selected['desc'][index], 'core/copy description mismatch')
            generations = []
            for submit in t['history']:
                if submit['kind'] != 10 or e['object'] not in batch(t, submit) or submit['enter'] < e['exit']:
                    continue
                reset, close, calls = recording(t, e['object'], submit)
                if e in calls:
                    generations.append((submit, reset, close, calls))
            require(len(generations) == 1, 'copy generation missing or resubmitted')
            submit, reset, close, calls = generations[0]
            row.update(recording_submitted=True, reset=reset['enter'], close=close['enter'],
                       submission=submit['enter'], queue=submit['object'], batch_index=batch(t,submit).index(e['object']))
            require(e['resource'] == m['resource'] and e['backing'] != m['resource'], 'selected depth is not a distinct source')
            require(not c['has_box'] and c['xyz'] == [0]*3 and
                    (e['kind'] == 16 or c['source'][1:3] == [0, 0]), 'partial depth source copy')
            row['full_depth_source'] = True
            row.update(local_copy_state(t, calls, e, m['resource']))
        except ValueError as exc:
            row['reasons'].append(str(exc))
    return result
