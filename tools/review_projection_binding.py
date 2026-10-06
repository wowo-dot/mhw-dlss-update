"""Review a bounded projection-to-command-recording trace; never qualify GPU execution."""
import argparse
import hashlib
import json
import struct
from pathlib import Path
import projection_contract

MAX_BYTES = 256 * 1024
HOST = 'c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea'

def require(ok, why):
    if not ok:
        raise ValueError(why)

def parse(raw):
    require(0 < len(raw) <= MAX_BYTES and raw.endswith(b'\n'), 'incomplete or oversized log')
    records = [json.loads(line) for line in raw.splitlines()]
    h = records[0]
    require(h.get('event') == 'start' and h.get('schema') == 1 and h.get('version') in ('0.1.0', '0.1.1', '0.1.2', '0.1.3', '0.2.0', '0.2.1', '0.3.0', '0.3.1', '0.4.0', '0.5.0', '0.6.0','0.7.0','0.7.1','0.8.0','0.8.1','0.8.2','0.8.3','0.8.4','0.9.0','0.9.1'), 'header version')
    require(h.get('host_sha256') == HOST, 'host contract')
    if h['version'] in ('0.8.2','0.8.3','0.8.4','0.9.0','0.9.1'):require(h.get('depth_frame_policy')=='selected-source-request','depth frame policy')
    if h['version']in ('0.8.0','0.8.1','0.8.2','0.8.3','0.8.4','0.9.0','0.9.1'):
        require('gpu_commands_added' not in h and h.get('gpu_copy_budget')==(5 if h['version'] in ('0.9.0','0.9.1') else 3 if h['version'] in ('0.8.3','0.8.4') else 1) and h.get('queue_signal_budget')==(5 if h['version'] in ('0.9.0','0.9.1') else 3 if h['version'] in ('0.8.3','0.8.4') else 1), 'readback GPU budget')
    else:
        require(h.get('gpu_commands_added')==0, 'host/GPU contract')
    if h['version'] in ('0.2.0','0.2.1','0.8.3','0.8.4','0.9.0','0.9.1'):
        require(h.get('experiment')==('projection-jitter-depth-sequence' if h['version'] in ('0.9.0','0.9.1') else 'projection-jitter-depth-triplet' if h['version'] in ('0.8.3','0.8.4','0.9.0','0.9.1') else 'projection-binding-stimulus') and h.get('injection_budget')==(3 if h['version'] in ('0.9.0','0.9.1') else 1) and h.get('cpu_samples')==(5 if h['version'] in ('0.9.0','0.9.1') else 3) and h.get('window_ms')==2000, 'stimulus contract')
        if h['version'] in ('0.2.1','0.8.3','0.8.4','0.9.0','0.9.1'):require(h.get('max_recovery_deferrals')==64, 'recovery deferral limit')
        if h['version'] in ('0.8.3','0.8.4','0.9.0','0.9.1'):require(h.get('jitter_writes')==(3 if h['version'] in ('0.9.0','0.9.1') else 1) and h.get('max_barriers')==64 and h.get('max_resources')==9,'triplet stimulus bounds')
        if h['version'] in ('0.9.0','0.9.1'):require(h.get('max_draw_waits')==64,'sequence draw wait bound')
    elif h['version'] in ('0.3.0','0.3.1','0.4.0','0.5.0','0.6.0','0.7.0','0.7.1','0.8.0','0.8.1','0.8.2','0.8.3','0.8.4','0.9.0','0.9.1'):
        require(h.get('experiment')==('projection-depth-readback' if h['version']in ('0.8.0','0.8.1','0.8.2','0.8.3','0.8.4','0.9.0','0.9.1') else 'projection-copy-readonly' if h['version'] in ('0.7.0','0.7.1','0.8.0','0.8.1','0.8.2','0.8.3','0.8.4','0.9.0','0.9.1') else 'projection-sync-readonly' if h['version']=='0.6.0' else 'projection-submission-readonly' if h['version']=='0.5.0' else 'projection-view-state-readonly' if h['version']=='0.4.0' else 'projection-attachments-readonly') and h.get('jitter_writes')==0 and
                h.get('window_ms')==2000 and h.get('max_barriers')==64 and h.get('max_resources')==9, 'attachment contract')
    else:
        require(h.get('experiment')=='projection-binding-readonly' and h.get('jitter_writes')==0, 'read-only contract')
    require(isinstance(h.get('pid'), int) and h['pid'] > 0 and isinstance(h.get('creation'), int) and h['creation'] > 0, 'process identity')
    return records

def dxbc(data):
    require(32 <= len(data) <= 262144 and data[:4] == b'DXBC', 'DXBC size/magic')
    version, size, count = struct.unpack_from('<III', data, 20)
    require(version == 1 and size == len(data) and 1 <= count <= 64 and 32 + 4*count <= size, 'DXBC header')
    ranges = []
    for n in range(count):
        offset, = struct.unpack_from('<I', data, 32+4*n)
        require(32+4*count <= offset <= size-8, 'DXBC offset')
        length, = struct.unpack_from('<I', data, offset+4)
        end = offset+8+length
        require(end <= size and all(end <= a or b <= offset for a, b in ranges), 'DXBC chunk range')
        ranges.append((offset, end))

def one(records, event):
    rows = [r for r in records if r['event'] == event]
    require(len(rows) == 1, f'expected one {event}')
    return rows[0]

def reservation(records):
    if records[0]['version'] not in ('0.1.2','0.1.3','0.2.0','0.2.1','0.3.0','0.3.1','0.4.0','0.5.0','0.6.0','0.7.0','0.7.1','0.8.0','0.8.1','0.8.2','0.8.3','0.8.4','0.9.0','0.9.1'):
        return
    p, ready = one(records, 'trampoline_reservation'), one(records, 'ready')
    require(p.get('ready') is True and ready.get('pool_ready') is True and p.get('hooks_installed') is False, 'startup reservation missing')
    require(p.get('policy_available') is True and isinstance(p.get('policy_flags'), int) and p['policy_flags'] & 1 == 0 and p.get('status') == 0, 'reservation policy/status')
    require(p.get('native_targets') == 10 and p.get('block_bytes') == 4096 and p.get('free_slots') == 62, 'reservation capacity')
    page, slot, origin = (p.get(k) for k in ('page', 'slot', 'origin'))
    require(all(isinstance(v, int) and 0 < v < 2**64 for v in (page, slot, origin)), 'reservation addresses')
    require(page % 4096 == 0 and slot % 64 == 0 and page+64 <= slot <= page+4032, 'reservation layout')
    require(origin-0x40000000 <= page < origin+0x40000000-4095, 'reservation reachability')
    require(records.index(p) < records.index(ready), 'reservation chronology')
    if not records[0].get('owned_fixture'):
        base = records[0].get('base')
        require(isinstance(base, int) and base > 0 and origin == base+0x228f3b0, 'reservation host origin')
        for rva in (0x228f3b0,0x23d2a80,0x259e1e0,0x259e670,0x259d670,0x259d740,0x259d820,0x259d930,0x259da10,0x23a2b50):
            require(base+rva-0x40000000 <= page < base+rva+0x40000000-4095, 'reservation native reachability')

def location(arena, source):
    p = arena['location']
    require(p['valid'] is True and arena['mode'] in (0, 1, 2) and 0 <= arena['allocation_index'] < 3, 'arena mode/index')
    stride = arena['stride']
    require(0 < stride <= 0x10000000 and stride % 256 == 0, 'arena stride')
    require(all(len(arena[k]) == 3 for k in ('cpu', 'gpu', 'used')), 'arena dimensions')
    for i in range(3):
        require(arena['cpu'][i] == arena['cpu'][0]+i*stride and arena['gpu'][i] == arena['gpu'][0]+i*stride, 'arena continuity')
        require(0 < arena['cpu'][i] <= 2**64-1-stride and 0 < arena['gpu'][i] <= 2**64-1-stride, 'arena overflow')
        require(arena['cpu'][i] % 256 == 0 and arena['gpu'][i] % 256 == 0 and 0 <= arena['used'][i] <= stride, 'arena bounds')
    i, offset = p['arena'], p['offset']
    require(0 <= i < 3 and 0 <= offset < stride and offset % 256 == 0, 'location index/offset')
    require(p['aligned_bytes'] == (source['bytes']+255)//256*256 and offset+p['aligned_bytes'] <= arena['used'][i], 'location extent')
    require(source['constants'] == arena['cpu'][i]+offset and p['gpu'] == arena['gpu'][i]+offset, 'CPU/GPU range mapping')
    return p['gpu']

def review(records, blob_loader, provenance=None, depth_loader=None, control_loader=None):
    require(not any(r['event'] == 'failed' for r in records), 'recorder failed')
    reservation(records)
    ready, armed = one(records, 'ready'), one(records, 'armed')
    require((ready['hooks_installed'] is True and ready.get('core_hooks_installed') is False if records[0]['version'] in ('0.4.0','0.5.0','0.6.0','0.7.0','0.7.1','0.8.0','0.8.1','0.8.2','0.8.3','0.8.4','0.9.0','0.9.1') else ready['hooks_installed'] is False) and armed['graphics_api'] == 'D3D12' and
            armed['live_spans'] == (39 if records[0]['version'] in ('0.3.0','0.3.1','0.4.0','0.5.0','0.6.0','0.7.0','0.7.1','0.8.0','0.8.1','0.8.2','0.8.3','0.8.4','0.9.0','0.9.1') else 36 if records[0]['version'] in ('0.1.3','0.2.0','0.2.1') else 29), 'runtime gates')
    done, stopped = one(records, 'capture_complete'), one(records, 'stopped')
    require(stopped['fatal'] is False and stopped['hooks_disabled'] is True and stopped['pending'] == 0, 'unclean shutdown')
    require(done['drained'] is True and done['pending'] == 0 and done['drops'] == 0 and done['fault'] == 0, 'capture faults/drops')
    require(all(done[k] is False for k in ('gpu_contents_verified', 'submission_verified', 'raster_jitter_verified')), 'unjustified GPU claim')
    s, pub, con = (one(records, x) for x in ('source', 'publication', 'consumption'))
    require(s['valid'] is True and pub['valid'] is True and con['valid'] is True, 'partial source chain')
    stimulus=records[0]['version'] in ('0.2.0','0.2.1','0.8.3','0.8.4','0.9.0','0.9.1')
    require(s['projection_valid'] is True and s['matrix_relation'] is True and (stimulus or s['jitter_bits']==[0,0]), 'projection source')
    require(0 < s['bytes'] <= 4096 and s['bytes'] == s['words']*4 and s['constants'] == s['tagged_constants'], 'source layout')
    require(pub['context'] == s['context'] and pub['id'] == s['buffer_id'] and 0 <= pub['index'] < pub['count'] <= 16, 'publication slot')
    require(pub['pointer'] == con['pointer'] == s['constants'] and pub['packet'] == con['packet'] != 0, 'packet provenance')
    require(s['order'] < pub['order'] < con['order'], 'source chronology')
    location(s['arena'], s)
    gpu = location(con['arena'], s)
    require(s['constants']+con['arena']['delta'] == gpu and gpu < 2**64, 'consumed arena delta')
    require(con['arena']['backend'] == s['arena']['backend'] == armed['backend'], 'backend identity')
    require(con['arena']['cpu_resource'] == armed['cpu_resource'] and con['arena']['gpu_resource'] == armed['gpu_resource'], 'resource identity')
    shaders = [r for r in records if r['event'] == 'shader']
    require(1 <= len(shaders) <= 5 and [r['index'] for r in shaders] == list(range(1,len(shaders)+1)), 'shader bounds')
    for shader in shaders:
        data = blob_loader(shader['index'])
        require(len(data) == shader['size'] and hashlib.sha256(data).hexdigest() == shader['sha256'], 'shader hash')
        dxbc(data)
    draws = [r for r in records if r['event'] == 'draw']
    require(len(draws) == 1 and done['stop'] == 2 and done['phase'] == 4, 'one-draw completion')
    d = draws[0]
    require(d['returned'] is True and d['source_unchanged'] is True and d['state_seen'] is True and d['rejection'] == 0, 'native draw checks')
    require(con['order'] < d['order'] < d['exit_order'] and d['gpu'] == gpu and d['context'] == con['context'], 'draw provenance')
    require(0 < d['blobs'][0] <= len(shaders) and len(d['blobs']) == 5 and all(0 <= b <= len(shaders) for b in d['blobs']), 'vertex shader mapping')
    apis = sorted((r for r in records if r['event'] == 'api'), key=lambda r:r['order'])
    require(1 <= len(apis) <= 128 and len({a['order'] for a in apis}) == len(apis), 'API capacity/order')
    bindings, roots, draws_seen = {}, [], []
    for a in apis:
        require(a['returned'] is True and a['draw'] == d['index'] and a['thread'] == d['thread'], 'API scope')
        require(d['order'] < a['order'] < a['exit_order'] < d['exit_order'], 'API chronology')
        require(a['object'] == (d['device'] if a['kind'] in (17,22) else d['command']), 'API object')
        if a['kind'] in (11, 18):
            bindings.clear()
        if a['kind'] == 9:
            bindings[a['args'][0]] = (a['args'][1], a['exit_order'])
        if a['kind'] in (14, 15, 16):
            draws_seen.append(a)
            roots.extend(index for index, (value, ended) in bindings.items() if value == gpu and ended < a['order'])
    require(len(draws_seen) == 1 and d['draw_api_observed'] is True, 'draw API missing')
    result=dict(schema=1, verdict='root-cbv-call-chain-observed' if roots else 'partial-binding-evidence',
        source_frame=s['frame'], source_epoch=s['epoch'], root_parameters=sorted(set(roots)), shaders=len(shaders),
        descriptor_observed=d['descriptor_observed'],
        shader_uses_projection_verified=False, gpu_contents_verified=False, submission_verified=False, raster_jitter_verified=False)
    if records[0]['version'] in ('0.1.3','0.2.0','0.2.1','0.3.0','0.3.1','0.4.0','0.5.0','0.6.0','0.7.0','0.7.1','0.8.0','0.8.1','0.8.2','0.8.3','0.8.4','0.9.0','0.9.1'):
        result.update(projection_contract.matrices(s))
        result.update(projection_contract.root_layout(d,apis,roots,shaders,s))
    if records[0]['version'] in ('0.3.0','0.3.1','0.4.0','0.5.0','0.6.0','0.7.0','0.7.1','0.8.0','0.8.1','0.8.2','0.8.3','0.8.4','0.9.0','0.9.1'):
        import projection_attachments
        result.update(projection_attachments.validate(d,apis,shaders,s,roots,records[0]['version']))
    if stimulus:
        import projection_stimulus
        result.update(projection_stimulus.validate(one(records,'stimulus'),s,armed,pub,records[0]))
        viewports=[a for a in apis if a['kind']==13 and a['exit_order']<draws_seen[0]['order']]
        require(viewports and viewports[-1]['args'][0]==1 and
                projection_contract.floats(viewports[-1]['viewport'],6)==(0.,0.,*projection_contract.floats(s['viewport_bits'],2),0.,1.), 'stimulus draw viewport mismatch/missing')
        result['matched_viewport_observed']=True
        result['nonzero_matrix_binding_observed']=bool(roots)
        result['nonzero_known_vertex_binding_correlated']=result['native_metadata_vs_b0_binding_correlated']
    if records[0]['version'] in ('0.4.0','0.5.0','0.6.0','0.7.0','0.7.1','0.8.0','0.8.1','0.8.2','0.8.3','0.8.4','0.9.0','0.9.1'):
        import projection_provenance
        require(provenance is not None, 'missing provenance companion')
        result.update(projection_provenance.correlate(records,provenance))
    if records[0]['version']in ('0.8.0','0.8.1','0.8.2','0.8.3','0.8.4','0.9.0','0.9.1'):
        import projection_readback
        require(depth_loader is not None,'missing depth pixels')
        result.update(projection_readback.validate(provenance,result,one(records,'depth_readback'),depth_loader(),records[0]['version'] in ('0.8.2','0.8.3','0.8.4','0.9.0','0.9.1')))
    if records[0]['version'] in ('0.8.1','0.8.2','0.8.3','0.8.4','0.9.0','0.9.1'):
        import projection_frames
        result.update(projection_frames.validate(provenance,one(records,'depth_readback')['details'],s))
    if records[0]['version'] in ('0.8.2','0.8.3','0.8.4','0.9.0','0.9.1'):
        import projection_alignment
        result.update(projection_alignment.validate(provenance,result,s,one(records,'depth_readback')['details']))
    if records[0]['version'] in ('0.8.3','0.8.4','0.9.0','0.9.1'):
        import projection_triplet
        if records[0]['version'] in ('0.9.0','0.9.1'):
            require(one(records,'stimulus')['calls'][2]['source']['order']>one(records,'draw')['exit_order'],'later stimulus precedes primary draw return')
        result.update(projection_triplet.validate(records,provenance,result,control_loader,5 if records[0]['version'] in ('0.9.0','0.9.1') else 3))
    return result

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('log', type=Path)
    ap.add_argument('--output', type=Path, required=True)
    args = ap.parse_args()
    require(not args.output.exists(), 'preserve previous review')
    require(args.log.stat().st_size <= MAX_BYTES, 'log size')
    raw = args.log.read_bytes()
    records = parse(raw)
    require(not records[0].get('owned_fixture'), 'fixture is not game evidence')
    provenance = None
    if records[0]['version'] in ('0.4.0','0.5.0','0.6.0','0.7.0','0.7.1','0.8.0','0.8.1','0.8.2','0.8.3','0.8.4','0.9.0','0.9.1'):
        import projection_provenance
        provenance = projection_provenance.load(args.log,one(records,'provenance_saved'))
    def depth_loader():
        import projection_readback
        return projection_readback.load(args.log,one(records,'depth_readback'))
    def control_loader(stage,record):
        import projection_readback
        return projection_readback.load(args.log,record,stage)
    result = review(records, lambda n: args.log.with_name(args.log.stem+f'-shader-{n}.bin').read_bytes(),provenance,depth_loader,control_loader)
    result['log_sha256'] = hashlib.sha256(raw).hexdigest()
    with args.output.open('x', encoding='utf-8') as f:
        json.dump(result, f, indent=2)
    print(json.dumps(result, indent=2))

if __name__ == '__main__':
    main()
