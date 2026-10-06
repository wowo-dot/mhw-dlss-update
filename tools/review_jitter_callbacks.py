"""Review bounded native callback captures. Acceptance is CPU evidence only."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path

HOST_SHA = 'c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea'
MAX_BYTES = 4 * 1024 * 1024

def require(ok, why):
    if not ok:
        raise ValueError(why)

def unique(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, 'duplicate JSON key')
        result[key] = value
    return result

def parse(raw):
    require(0 < len(raw) <= MAX_BYTES, 'capture size')
    # A writer may be between rows. Never accept a partial JSON row as evidence.
    require(raw.endswith(b'\n'), 'capture has an incomplete last line; retry after flush')
    records = [json.loads(line, object_pairs_hook=unique) for line in raw.decode('utf-8').splitlines()]
    require(len(records) <= 800, 'capture row cap')
    require(records and records[0].get('event') == 'start', 'start record missing')
    require(sum(r.get('event') == 'start' for r in records) == 1, 'multiple process headers')
    header = records[0]
    require(header.get('schema') == 1 and header.get('version') == '1.0.0', 'unsupported capture')
    require(header.get('host_sha256') == HOST_SHA and header.get('provider_substitution') is False, 'host/experiment mismatch')
    require(header.get('experiment') == 'native-callbacks-dlss-off' and header.get('record_capacity') == 256 and
            header.get('window_ms') == 1500, 'capture contract mismatch')
    require(type(header.get('pid')) is int and header['pid'] > 0 and type(header.get('creation')) is int and
            header['creation'] > 0 and type(header.get('base')) is int and header['base'] > 0, 'process identity')
    require(len(header.get('dll_sha256', '')) == 64, 'DLL identity absent')
    return records

def review(records):
    header = records[0]
    starts = [r for r in records if r['event'] == 'armed']
    finishes = [r for r in records if r['event'] == 'phase_complete']
    installed = [r for r in records if r['event'] == 'installed']
    require(len({s['phase'] for s in starts}) == len(starts) and
            all(s['phase'] in (1,2,3) for s in starts), 'duplicate/unknown armed phase')
    require(all(r['token'] >> 32 in {s['phase'] for s in starts} for r in records if r['event']=='call'), 'unarmed call record')
    require(len(installed) <= 1, 'reinstallation not supported')
    if finishes:
        require(len(installed) == 1 and installed[0].get('live_spans') == 7 and
                installed[0].get('d3d12_module_loaded') is True, 'verified install record required')
    phases = []
    finished = set()
    for end in finishes:
        p = end['phase']
        require(p in (1, 2, 3) and p not in finished and p == len(finished) + 1, 'phase order/duplication')
        finished.add(p)
        require(sum(s['phase'] == p for s in starts) == 1, 'unique arm record required')
        rows = [r for r in records if r['event'] == 'call' and r['token'] >> 32 == p]
        require(len(rows) == end['records'] == end['reserved'] <= 256 and not end['pending'], 'incomplete record publication')
        require(end['stop'] in (2, 3, 4, 5), 'invalid terminal capture state')
        by_token = {r['token']: r for r in rows}
        require(len(by_token) == len(rows), 'duplicate call token')
        require(sorted(t & 0xffffffff for t in by_token) == list(range(1,len(rows)+1)), 'missing call token')
        orders = []
        failures = 0
        for r in rows:
            require(r['complete'] is True and r['kind'] in (0,1,2) and r['thread'] > 0, 'incomplete/unknown callback')
            require(r['entry_order'] < r['exit_order'] and r['tick'] <= r['exit_tick'], 'invalid call interval')
            orders.extend((r['entry_order'],r['exit_order']))
            if r['parent']:
                require(r['parent'] in by_token, 'missing parent')
                parent = by_token[r['parent']]
                require(parent['thread'] == r['thread'] and parent['entry_order'] < r['entry_order'] <
                        r['exit_order'] < parent['exit_order'], 'parent/thread ordering mismatch')
            for s in (r['before'], r['after']):
                require(0 < s['reads'] <= 40, 'scalar read budget')
                failures += bool(s['failure'])
                if not s['failure']:
                    require(s['camera_type'] in (header['base']+0x350d188,header['base']+0x33f82a8), 'camera type')
                    if s['has_layer']:
                        require(s['layer_type'] == header['base']+0x351bda8, 'TAA type')
                    if r['kind'] < 2:
                        require(s['has_layer'] and s['layer'] == r['argument'], 'callback object identity')
                    if r['kind'] == 2:
                        require(s['has_input'] and r['argument'] == s['camera'], 'setter object/input identity')
                if s['stable']:
                    require(not s['failure'] and s['frame'] == s['frame_after'] and s['epoch'] == s['epoch_after'], 'invalid stability claim')
        require(len(orders) == len(set(orders)), 'duplicate order marker')
        require(sorted(orders) == list(range(1,2*len(rows)+1)), 'missing order marker')
        kinds = Counter(r['kind'] for r in rows)
        enabled = Counter(r['before']['taa_bytes'] & 255 for r in rows if r['kind'] == 0 and not r['before']['failure'])
        setters = [r for r in rows if r['kind'] == 2]
        phases.append(dict(phase=p,records=len(rows),kinds=dict(kinds),dropped=end['dropped'],stop=end['stop'],
            invalid_snapshots=failures,stable_pairs=sum(r['before']['stable'] and r['after']['stable'] for r in rows),
            jitter_enable_before_update=dict(enabled),setter_calls=len(setters),
            nested_setters=sum(bool(r['parent']) for r in setters),
            nonzero_setter_inputs=sum(r['before']['setter_input'] != 0 for r in setters if not r['before']['failure']),
            accepted_cpu_capture=bool(rows) and not failures and end['stop'] != 4,
            complete_event_coverage=end['dropped'] == 0 and end['stop'] == 2))
    return dict(schema=1,pid=header['pid'],creation=header['creation'],phases=phases,
        stopped=[r for r in records if r['event']=='stopped'],gpu_qualified=False,
        limitation='Recorded scalar callback intervals only; no GPU/raster proof, continuous coverage, or motion-vector qualification.')

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('log',type=Path)
    ap.add_argument('--output',type=Path)
    a=ap.parse_args()
    require(a.log.stat().st_size <= MAX_BYTES,'capture size')
    raw=a.log.read_bytes(); result=review(parse(raw)); result['capture_sha256']=hashlib.sha256(raw).hexdigest()
    if a.output:
        with a.output.open('x',encoding='utf-8') as f: json.dump(result,f,indent=2)
    print(json.dumps(result,indent=2))

if __name__=='__main__': main()
