"""Qualify observed DSV lifetimes and list-local declarations, never GPU state."""
import hashlib
import json
from pathlib import Path

MAX_BYTES = 12 * 1024 * 1024


def require(ok, why):
    if not ok:
        raise ValueError('provenance: ' + why)


def uint(x, bits=64):
    return type(x) is int and 0 <= x < 2**bits


def ready(p, version=None):
    history=p.get('history_capacity')
    require(uint(history,32) and history in (4096,8192),'history capacity contract')
    if version is not None:
        require(history==(8192 if version=='0.9.1' else 4096),'history capacity version')
    require(type(p.get('triplet_readback',False)) is bool,'triplet readiness type')
    if p.get('triplet_readback'):require(p.get('same_frame_readback') is True,'triplet frame selection missing')
    require(type(p.get('same_frame_readback',False)) is bool,'same-frame readiness type')
    if p.get('same_frame_readback'):require(p.get('frame_tagging') is True,'same-frame packet hooks missing')
    tagged=p.get('frame_tagging',False)
    require(type(tagged) is bool and (not tagged or (p.get('schema')==4 and p.get('frame_capacity')==256)), 'frame readiness')
    require(p.get('schema') in (1, 2, 3, 4) and p.get('hooks_installed') is True and p.get('code_gates') == (7 if tagged else 6 if p['schema']==4 else 5), 'watch readiness')
    if p['schema'] >= 2:
        for k, v in dict(list_capacity=128, queue_capacity=8, submission_capacity=4096, batch_capacity=128).items():
            require(p.get(k) == v, 'submission bounds: ' + k)
    for k, v in dict(watch_ms=1800000, watch_capacity=2048,
                     barrier_capacity=65536, pin_capacity=256, history_warmup_ms=50).items():
        require(p.get(k) == v, 'watch bounds: ' + k)
    require(uint(p.get('storage_bytes')) and 0 < p['storage_bytes'] <= (12 if history==8192 else 8) * 1024 * 1024, 'journal storage bound')
    require(all(uint(p.get(k)) and p[k] > 0 for k in ('started', 'backend', 'device', 'heap', 'heap_start', 'stride')), 'watch anchors')
    require(uint(p.get('heap_count'), 32) and 0 < p['heap_count'] <= 65536 and
            p['heap_start'] + p['heap_count'] * p['stride'] < 2**64, 'heap range')
    if p['schema'] >= 3:
        require(p.get('fence_capacity') == 32, 'fence capacity')
    if 'copy_retention' in p or 'copy_pin_capacity' in p:
        require(p['schema']==4 and p.get('copy_retention')=='per-call-until-drain' and
                p.get('copy_pin_capacity')==1024, 'copy retention policy')
    count = 17 if tagged else 15 if p['schema']==4 else 13 if p['schema'] == 3 else 9 if p['schema'] == 2 else 8
    require(len(p.get('targets', [])) == count and len(set(p['targets'])) == count and
            all(uint(v) and v > 0 for v in p['targets']), 'watch targets')


def load(path, record):
    require(record.get('file') == path.stem + '-provenance.json', 'companion filename')
    companion = path.with_name(record['file'])
    require(0 < companion.stat().st_size <= MAX_BYTES, 'companion size')
    raw = companion.read_bytes()
    require(raw.endswith(b'\n') and hashlib.sha256(raw).hexdigest() == record.get('sha256'), 'companion integrity')
    return json.loads(raw)


def descriptor(d, r):
    require(len(d) == 6 and all(uint(v, 32) for v in d), 'descriptor words')
    require(r['dimension'] == 3 and r['format'] == 39 and r['flags'] & 2 and
            r['samples'] == 1 and r['mips'] > 0 and r['depth_or_array'] > 0 and
            d[0] == 40 and d[2] == 0, 'unsupported writable D32 view')
    if d[1] == 3:
        require(r['depth_or_array'] == 1 and d[3] < r['mips'], '2D view bounds')
        return d[3]
    require(d[1] == 4 and d[3] < r['mips'] and d[4] < r['depth_or_array'] and d[5] == 1, 'array view bounds')
    return d[3] + d[4] * r['mips']


def structure(p, t):
    ready(p)
    require(t.get('schema') == p['schema'] and t.get('error') == t.get('pending') == t.get('pins') == 0 and
            t.get('released') is True and t.get('disabled') is True, 'unclean journal')
    for key, count, cap in (('watch', 'watch_reserved', 2048), ('history', 'history_reserved', p['history_capacity']),
                            ('barriers', 'barriers_reserved', 65536)):
        require(type(t.get(key)) is list and uint(t.get(count), 32) and len(t[key]) == t[count] <= cap, 'journal capacity/count')
    require(uint(t.get('frame_started')) and p['started'] <= t['frame_started'] < p['started'] + p['watch_ms'], 'frame clock')
    m = t['marker']
    require(all(uint(m.get(k)) and m[k] > 0 for k in ('order', 'command', 'view', 'resource', 'identity', 'handle')), 'draw marker')
    require(uint(m.get('bind_enter')) and uint(m.get('bind_exit')) and
            ((m['bind_enter'] == m['bind_exit'] == 0) or 0 < m['bind_enter'] < m['bind_exit'] < m['order']), 'binding marker')
    orders = [m['order']] + ([m['bind_enter'], m['bind_exit']] if m['bind_enter'] else [])
    if p['schema'] >= 2:
        require(uint(t.get('draw_return')) and t['draw_return'] > m['order'], 'draw return marker')
        orders.append(t['draw_return'])
    coverage = []
    for channel, kinds in (('watch', range(5)), ('history', range(5, 17 if p['schema']==4 else 15 if p['schema'] == 3 else 11 if p['schema'] == 2 else 10))):
        for e in t[channel]:
            require(uint(e.get('kind'), 32) and e['kind'] in kinds and e.get('valid') is True and
                    e.get('returned') is True and type(e.get('null_desc')) is bool, 'event validity/kind')
            require(all(uint(e.get(k)) for k in ('enter', 'exit', 'tick', 'object', 'caller', 'resource', 'identity', 'handle', 'backing')), 'event scalar')
            require(all(uint(e.get(k), 32) for k in ('thread', 'parent', 'count', 'first', 'result')) and
                    e['thread'] > 0 and e['object'] > 0 and 0 < e['enter'] < e['exit'], 'event interval')
            require(p['started'] <= e['tick'] < p['started'] + p['watch_ms'] and
                    (channel != 'history' or t['frame_started'] <= e['tick'] < t['frame_started'] + 2000), 'event clock')
            require(len(e['descriptor']) == 6 and len(e['recipe']) == 5 and
                    all(uint(v, 32) for v in e['descriptor'] + e['recipe']), 'event payload')
            orders += [e['enter'], e['exit']]
            if e['kind'] in (2, 3, 4):
                require(e['object'] == p['device'], 'descriptor device')
            if e['kind'] in (3, 4):
                require(len(e['ranges']) == e['count'] <= 32, 'copy range capacity')
                for start, count in e['ranges']:
                    require(uint(start) and start > 0 and uint(count, 32) and 0 < count <= 65536 and
                            start + count * p['stride'] < 2**64, 'copy range extent')
            else:
                require(e['ranges'] == [], 'unexpected copy payload')
            if e['kind'] == 9:
                require(0 < e['count'] <= 1024 and e['first'] + e['count'] <= len(t['barriers']), 'barrier slice')
                coverage.extend(range(e['first'], e['first'] + e['count']))
    require(len(set(orders)) == len(orders), 'duplicate event order')
    require(sorted(coverage) == list(range(len(t['barriers']))), 'barrier coverage')
    for b in t['barriers']:
        require(len(b) == 7 and all(uint(v) for v in b) and all(uint(b[i], 32) for i in (0, 1, 4, 5, 6)), 'barrier words')
        kind, flags, resource, other, sub, before, after = b
        require(kind in (0, 1, 2) and flags in (0, 1, 2), 'barrier kind/flags')
        require((kind == 0 and resource > 0 and other == 0) or
                (kind in (1, 2, 3, 4) and flags == sub == before == after == 0 and (kind == 1 or other == 0)), 'barrier union')
    if p['schema'] >= 2:
        import projection_submission
        projection_submission.structure(p, t)
    if p['schema']==4:
        import projection_copies
        projection_copies.structure(p, t)
    return m


def view_lifetime(p, t, m, r, base=None):
    require(m['bind_enter'] > 0, 'fresh depth binding not observed')
    handle = m['handle']
    require(p['heap_start'] <= handle < p['heap_start'] + p['heap_count'] * p['stride'] and
            (handle - p['heap_start']) % p['stride'] == 0, 'selected handle outside heap')
    candidates = [e for e in t['watch'] if e['kind'] == 2 and e['handle'] == handle and e['exit'] < m['bind_enter']]
    require(candidates, 'view predates watcher or creation missing')
    e = max(candidates, key=lambda v: v['enter'])
    require(e['parent'] > 0 and e['parent'] <= len(t['watch']), 'unqualified descriptor creation')
    parent = t['watch'][e['parent'] - 1]
    require(parent['kind'] == 0 and parent['parent'] == 0 and parent['object'] == m['view'] and
            parent['enter'] < e['enter'] < e['exit'] < parent['exit'] < m['bind_enter'] and
            parent['thread'] == e['thread'], 'constructor generation/interval')
    require(parent['resource'] == e['resource'] == m['resource'] == r['resource'] and
            e['identity'] == m['identity'] == r['identity'] and parent['handle'] == handle and
            not e['null_desc'], 'descriptor identity/default')
    if base is not None:
        require(e['caller'] == base + 0x24e6516, 'native CreateDSV call site')
    # Keep recipe as independent evidence; it never substitutes for API bytes.
    for other in t['watch']:
        if other is e or other is parent or other['enter'] >= m['order'] or other['exit'] <= parent['enter']:
            continue
        if other['kind'] in (0, 1) and other['object'] == m['view']:
            require(False, 'view destroyed, reused or lifetime overlaps')
        if other['kind'] == 2 and other['handle'] == handle:
            require(False, 'descriptor overwritten or creation overlaps')
        if other['kind'] in (3, 4):
            require(not any(start <= handle < start + count * p['stride'] for start, count in other['ranges']), 'descriptor copy invalidates association')
    return e, descriptor(e['descriptor'], r)


def local_state(t, m, created, sub):
    events = sorted((e for e in t['history'] if e['object'] == m['command'] and e['enter'] < m['order']), key=lambda e: e['enter'])
    last = 0
    for e in events:
        require(last < e['enter'] < e['exit'] < m['order'], 'overlapping/unfinished selected-list calls')
        require(not m['bind_enter'] or e['exit'] < m['bind_enter'] or e['enter'] > m['bind_exit'], 'binding overlaps list operation')
        last = e['exit']
    resets = [e for e in events if e['kind'] == 8]
    require(resets, 'selected list prefix has no actual Reset')
    reset = resets[-1]
    require(reset['result'] < 0x80000000 and reset['resource'] > 0, 'latest Reset failed')
    require(created['exit'] < reset['enter'], 'resource lifetime not pinned before recording generation')
    state, split, transitions, clears = None, None, 0, 0
    for e in events:
        if e['enter'] <= reset['enter']:
            continue
        require(e['kind'] != 5, 'selected generation closed before draw')
        require(e['kind'] != 7, 'bundle state not covered')
        if e['kind'] == 6 and e['handle'] == m['handle']:
            require(e['count'] == 0, 'partial clear not supported')
            require(split is None and state in (None, 0x10), 'clear conflicts with recorded state')
            clears += 1
        if e['kind'] != 9:
            continue
        for kind, flags, resource, other, selected, before, after in t['barriers'][e['first']:e['first'] + e['count']]:
            if kind == 1:
                require(resource not in (0, m['resource']) and other not in (0, m['resource']), 'relevant aliasing barrier')
            if kind != 0 or resource != m['resource'] or selected not in (sub, 0xffffffff):
                continue
            if flags == 2:
                require(split == (selected, before, after), 'split end without matching begin')
                split = None
                state = after
                transitions += 1
                continue
            require(split is None and (state is None or state == before), 'transition continuity/split overlap')
            if flags == 1:
                split = (selected, before, after)
            else:
                state = after
                transitions += 1
    require(split is None, 'unresolved split barrier')
    require(transitions > 0 and state == 0x10, 'DEPTH_WRITE declaration missing')
    return dict(recording_reset_order=reset['enter'], local_depth_transitions=transitions,
                local_depth_clears=clears, local_declared_state=state)


def validate(p, t, resource, expected, base=None):
    m = structure(p, t)
    require(all(m[k] == expected[k] for k in ('command', 'view', 'resource', 'identity', 'handle')), 'core/companion marker mismatch')
    result = dict(view_descriptor_provenance_verified=False, local_depth_state_declared=False,
                  resource_state_at_gpu_execution_verified=False, submission_verified=False,
                  gpu_contents_verified=False, raster_jitter_verified=False, provenance_reasons=[])
    if p['schema'] >= 2:
        result.update(selected_recording_submitted=False, submitted_depth_state_declared=False,
                      gpu_completion_verified=False, submission_reasons=[])
    if p['schema'] >= 3:
        result.update(same_queue_depth_state_declared=False, selected_queue_fence_completion_observed=False,
                      cross_queue_resource_safety_verified=False, synchronization_reasons=[])
    try:
        created, sub = view_lifetime(p, t, m, resource, base)
    except ValueError as e:
        result['provenance_reasons'].append(str(e))
        return result
    result.update(view_descriptor_provenance_verified=True, depth_view_descriptor=created['descriptor'],
                  depth_view_recipe=t['watch'][created['parent'] - 1]['recipe'], depth_view_subresource=sub)
    try:
        result.update(local_state(t, m, created, sub))
        result['local_depth_state_declared'] = True
    except ValueError as e:
        result['provenance_reasons'].append(str(e))
    if p['schema'] >= 2:
        import projection_submission
        result.update(projection_submission.validate(p, t, m, created, sub, resource, base))
    if p['schema'] >= 3:
        import projection_sync
        result.update(projection_sync.validate(p, t, m, created, sub, resource, base))
    if p['schema']==4:
        import projection_copies
        result.update(projection_copies.validate(p,t,m,resource,base))
    return result


def correlate(records, t):
    from review_projection_binding import one
    p = one(records, 'provenance_ready')
    if records[0]['version'] in ('0.8.2','0.8.3','0.8.4','0.9.0','0.9.1'):require(p.get('same_frame_readback') is True,'same-frame selection disabled')
    if records[0]['version'] in ('0.8.3','0.8.4','0.9.0','0.9.1'):require(p.get('triplet_readback') is True,'triplet readback disabled')
    if records[0]['version'] in ('0.9.0','0.9.1'):require(p.get('sequence_readback') is True and p.get('readback_slots')==5,'sequence readback disabled')
    require(p['schema'] == (4 if records[0]['version'] in ('0.7.0','0.7.1','0.8.0','0.8.1','0.8.2','0.8.3','0.8.4','0.9.0','0.9.1') else 3 if records[0]['version'] == '0.6.0' else 2 if records[0]['version'] == '0.5.0' else 1), 'version/schema mismatch')
    ready(p,records[0]['version'])
    if records[0]['version'] in ('0.8.1','0.8.2','0.8.3','0.8.4','0.9.0','0.9.1'):require(p.get('frame_tagging') is True, 'frame receipt hooks missing')
    if records[0]['version'] in ('0.7.1','0.8.0','0.8.1','0.8.2','0.8.3','0.8.4','0.9.0','0.9.1'):
        require(p.get('copy_retention')=='per-call-until-drain' and p.get('copy_pin_capacity')==1024, 'copy retention version')
    d, armed = one(records, 'draw'), one(records, 'armed')
    require(p['device'] == armed['device'] == d['device'] and p['backend'] == armed['backend'], 'core/watch anchors')
    require(records.index(p) < records.index(one(records, 'ready')) < records.index(armed), 'startup chronology')
    v = d['attachments']['ready']['views'][8]
    r = next((r for r in d['attachments']['resources'] if r['resource'] == v['resource']), None)
    require(r is not None, 'selected depth resource absent')
    expected = dict(command=d['command'], view=v['backend'], resource=v['resource'], identity=r['identity'], handle=v['handle'])
    om = [e for e in records if e['event'] == 'api' and e['kind'] == 20]
    require(len(om) == int(bool(t['marker']['bind_enter'])), 'core/binding marker count')
    return validate(p, t, r, expected, records[0]['base'])
