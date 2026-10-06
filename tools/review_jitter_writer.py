"""Validate bounded, typed boolean-property setter observations; CPU evidence only."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re

HOST_SHA='c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea'
MAX_BYTES=1024*1024
def require(ok, why):
    if not ok: raise ValueError(why)
def unique(pairs):
    out={}
    for k,v in pairs:
        require(k not in out,'duplicate JSON key'); out[k]=v
    return out
def parse(raw):
    require(0<len(raw)<=MAX_BYTES and raw.endswith(b'\n'),'capture size or partial last row')
    rows=[json.loads(s,object_pairs_hook=unique) for s in raw.decode('utf-8').splitlines()]
    require(len(rows)<=80 and rows[0].get('event')=='start','header/row cap')
    require(sum(r.get('event')=='start' for r in rows)==1,'duplicate start')
    h=rows[0]
    require(type(h.get('schema')) is int and (h['schema'],h.get('version')) in ((1,'1.0.0'),(2,'1.1.0')) and h.get('host_version')=='0.0.0.0','version')
    require(h.get('host_sha256')==HOST_SHA and h.get('experiment')=='typed-jitter-property-writer' and h.get('provider_substitution') is False,'identity/experiment')
    require(h.get('record_capacity')==64 and h.get('window_ms')==180000 and h.get('visit_limit')==1000000,'capture bounds')
    require(all(type(h.get(k)) is int and h[k]>0 for k in ('pid','creation','base')),'process identity')
    require(re.fullmatch('[0-9a-f]{64}',h.get('dll_sha256','')),'DLL identity')
    require(all(r.get('event') in {'start','ready','adapter_inventory','armed','write','capture_complete','stopped','failed'} for r in rows),'unknown event')
    return rows
def review(rows):
    h=rows[0]
    arms=[r for r in rows if r['event']=='armed']; ends=[r for r in rows if r['event']=='capture_complete']; stops=[r for r in rows if r['event']=='stopped']
    require(len(arms)==len(ends)==len(stops)==1,'finished armed capture required')
    arm,end,stop=arms[0],ends[0],stops[0]
    require(arm.get('live_spans')==(5 if h['schema']==2 else 3) and arm.get('d3d12_module_loaded') is True,'live verification')
    require(rows.index(arm)<rows.index(end)<rows.index(stop),'capture event order')
    writes=[r for r in rows if r['event']=='write']
    require(len(writes)==end['records']==end['reserved']<=64 and end['pending']==0,'incomplete publication')
    require(end['stop'] in (2,3,4,5,6),'terminal state')
    require(0<=end['named']<=end['visits']<=1000001 and 0<=end['other_type']<=end['named'],'visit counts')
    require(end['dropped']>=0 and end['read_failures']>=0,'failure counts')
    require(sorted(r['token'] for r in writes)==list(range(1,len(writes)+1)),'missing/duplicate token')
    orders=[]; failures=0; owner_changes=0
    track_calls=track_failures=track_changes=track_mismatches=track_pairs=0
    for r in writes:
        require(rows.index(arm)<rows.index(r)<rows.index(end),'write outside capture')
        require(r['complete'] is True and r['thread']>0 and 0<=r['input']<=255 and r['descriptor']>0,'call record')
        require(arm['tick']<=r['tick']<=r['exit_tick'] and r['tick']-arm['tick']<180000,'call time')
        require(r['entry_order']<r['exit_order'],'call order'); orders.extend((r['entry_order'],r['exit_order']))
        for s in (r['before'],r['after']):
            require(s['failure'] in (0,1,2,3,4),'unknown failure'); failures+=bool(s['failure'])
            if not s['failure']:
                require(s['name']==h['base']+0x3344478 and s['type']==h['base']+0x351bda8,'named TAA2 identity')
                require(s['owner']>0 and s['target']==s['owner']+0x184 and s['flags']==3 and s['index']==0 and 0<=s['value']<=255,'direct scalar destination')
        owner_changes+=any(r['before'][k]!=r['after'][k] for k in ('name','owner','type','target','flags','index'))
        if h['schema']==2:
            applicable=r['caller']==h['base']+0x221d5ed
            track_calls+=applicable
            for key,destination in (('track_before','before'),('track_after','after')):
                s=r[key]
                require(type(s.get('applicable')) is bool and s['applicable']==applicable,'track caller identity')
                for field in ('record','header','owner','keys','values','kind','count','index','key_word','raw_value','failure'):
                    require(type(s.get(field)) is int and 0<=s[field]<=0xffffffffffffffff,'track integer field')
                require(s['failure'] in (0,1,2),'track failure code')
                if not applicable:
                    require(not any(v for k,v in s.items() if k!='applicable'),'foreign caller followed as track')
                    continue
                track_failures+=bool(s['failure'])
                if not s['failure']:
                    require(not r[destination]['failure'],'track without valid destination')
                    require(s['record']==r['descriptor']-0x10 and 0<s['record']<=0xffffffffffffffff-0x70,'track record bounds')
                    require(s['owner']==r[destination]['owner'] and s['kind']==11,'track owner/kind')
                    require(0<s['header']<=0xffffffffffffffff-0x30 and 0<s['count']<=65535 and 0<=s['index']<s['count'],'track count/index')
                    require(0<s['keys']<=0xffffffffffffffff-4*s['index']-4 and 0<s['values']<=0xffffffffffffffff-s['index']-1,'track selected address bounds')
                    require(s['key_word']<=0xffffffff and s['raw_value']<=255,'track scalar width')
            if applicable:
                a,b=r['track_before'],r['track_after']
                valid=not a['failure'] and not b['failure']
                changed=a!=b
                mismatch=any(not s['failure'] and (s['raw_value']&1)!=r['input'] for s in (a,b))
                track_changes+=valid and changed
                track_mismatches+=mismatch
                track_pairs+=valid and not changed and not mismatch
    require(sorted(orders)==list(range(1,len(writes)*2+1)),'duplicate/missing order marker')
    bad=failures or owner_changes or track_failures or end['read_failures'] or end['stop']==6 or any(r['event']=='failed' for r in rows)
    clean=stop.get('fatal') is False and stop.get('hooks_disabled') is True and stop.get('pending')==0
    overlap=sum(any(a is not b and a['before']['target']==b['before']['target'] and a['entry_order']<b['exit_order'] and b['entry_order']<a['exit_order'] for b in writes) for a in writes)
    out=dict(schema=h['schema'],pid=h['pid'],creation=h['creation'],records=len(writes),visits=end['visits'],named=end['named'],
        other_type=end['other_type'],dropped=end['dropped'],stop=end['stop'],invalid_snapshots=failures,owner_changes=owner_changes,
        clean_stop=clean,accepted_cpu_capture=bool(writes) and not bad and clean,
        valid_empty_observation=not writes and not bad and clean,
        inputs=dict(Counter(r['input'] for r in writes)),caller_rvas=dict(Counter(hex(r['caller']-h['base']) for r in writes)),
        recorded_one_to_zero=sum(not r['before']['failure'] and not r['after']['failure'] and r['before']['value']==1 and r['input']==r['after']['value']==0 for r in writes),
        after_matches_input=sum(not r['after']['failure'] and r['after']['value']==r['input'] for r in writes),
        calls_overlapping_same_target=overlap,gpu_qualified=False,
        limitation='Selected generic setter only; empty output excludes no other writer, startup or inline path. No GPU or raster qualification.')
    if h['schema']==2:
        selected=[r for r in writes if r['track_before']['applicable'] and not r['track_before']['failure']]
        out.update(track_calls=track_calls,invalid_track_snapshots=track_failures,
            track_context_changes=track_changes,track_input_mismatches=track_mismatches,
            matching_track_pairs=track_pairs,
            accepted_source_context=bool(out['accepted_cpu_capture'] and track_calls and track_pairs==track_calls and not overlap),
            source_values=dict(Counter(r['track_before']['raw_value'] for r in selected)),
            selected_keys=[dict(token=r['token'],index=r['track_before']['index'],count=r['track_before']['count'],
                key_word=r['track_before']['key_word'],raw_value=r['track_before']['raw_value']) for r in selected],
            source_limitation='Only selected key/value copied during call; sequential snapshots do not prove atomicity, asset identity or active raster ownership.')
    return out
def main():
    ap=argparse.ArgumentParser(description=__doc__); ap.add_argument('log',type=Path); ap.add_argument('--output',type=Path); a=ap.parse_args()
    require(a.log.stat().st_size<=MAX_BYTES,'capture size'); raw=a.log.read_bytes(); out=review(parse(raw)); out['capture_sha256']=hashlib.sha256(raw).hexdigest()
    if a.output:
        with a.output.open('x',encoding='utf-8') as f: json.dump(out,f,indent=2)
    print(json.dumps(out,indent=2))
if __name__=='__main__': main()
