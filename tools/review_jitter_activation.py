"""Review one scoped camera-jitter experiment; CPU projection evidence only."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import struct

HOST_SHA='c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea'
MAX_BYTES=65536

def require(ok, why):
    if not ok: raise ValueError(why)

def unique(pairs):
    result={}
    for k,v in pairs:
        require(k not in result,'duplicate JSON key'); result[k]=v
    return result

def integer(value, low=0, high=0xffffffffffffffff):
    require(type(value) is int and low<=value<=high,'integer range/type')
    return value

def parse(raw):
    require(0<len(raw)<=MAX_BYTES and raw.endswith(b'\n'),'capture size or partial last row')
    def invalid_constant(value): raise ValueError('nonfinite JSON number: '+value)
    rows=[json.loads(s,object_pairs_hook=unique,parse_constant=invalid_constant) for s in raw.decode('utf-8').splitlines()]
    require(1<=len(rows)<=12 and all(type(r) is dict for r in rows),'row cap/type')
    h=rows[0]
    require(h.get('event')=='start' and sum(r.get('event')=='start' for r in rows)==1,'header')
    require(type(h.get('schema')) is int and h['schema']==1 and h.get('version') in ('1.0.0','1.0.1','1.0.2') and h.get('host_version')=='0.0.0.0','version')
    require(h.get('host_sha256')==HOST_SHA and h.get('experiment')=='scoped-camera-jitter' and h.get('provider_substitution') is False,'identity/experiment')
    for field,want in (('record_capacity',3),('window_ms',1500),('visit_limit',256)):
        require(integer(h.get(field))==want,'capture bounds')
    for field in ('pid','creation','base'): integer(h.get(field),1)
    require(re.fullmatch('[0-9a-f]{64}',h.get('dll_sha256','')),'DLL identity')
    allowed={'start','ready','adapter_inventory','armed','sample','capture_complete','stopped','failed'}
    require(all(r.get('event') in allowed for r in rows),'unknown event')
    return rows

def floats(words, count):
    require(type(words) is list and len(words)==count,'float array length')
    values=[struct.unpack('<f',struct.pack('<I',integer(w,0,0xffffffff)))[0] for w in words]
    require(all(math.isfinite(v) for v in values),'nonfinite float bits')
    return values

def pair(bits):
    integer(bits)
    return [bits&0xffffffff,bits>>32]

def validate_capture(rows):
    h=rows[0]
    require([r['event'] for r in rows]==['start','ready','adapter_inventory','armed','sample','sample','sample','capture_complete','stopped'],'complete event sequence required')
    ready,inventory,arm=rows[1:4]; samples=rows[4:7]; end,stop=rows[7:]
    require(ready.get('hooks_installed') is False,'ready before hooks')
    require(inventory.get('gpu_instrumented') is False and inventory.get('active_adapter_identified') is False and inventory.get('swap_chain_format') is None,'GPU qualification unsupported')
    require(arm.get('live_spans')==(9 if h['version']=='1.0.2' else 8) and arm.get('d3d12_module_loaded') is True and arm.get('graphics_api_confirmed') is False,'live verification')
    if h['version']!='1.0.0':
        installation=arm['installation']
        require(installation.get('stage')==3 and installation.get('status')==0 and installation.get('status_name')=='MH_OK' and installation.get('hook_created') is True and installation.get('hook_enabled') is True,'installation diagnostics')
    started=integer(arm['tick'],1)
    for key,value in dict(records=3,reserved=3,pending=0,stop=2,injections=1,restorations=1,dropped=0).items():
        require(integer(end.get(key))==value,'incomplete/invalid capture: '+key)
    require(end.get('drained') is True and 3<=integer(end.get('visits'))<=256,'admission/drain bound')
    require(stop.get('fatal') is False and stop.get('hooks_disabled') is True and integer(stop.get('pending'))==0 and stop.get('dll_pinned') is True and stop.get('trampolines_retained') is True,'clean stop required')
    previous=None; delta_max=[]
    for stage,r in enumerate(samples):
        require(integer(r['stage'])==stage and integer(r['rejection'])==0,'stage or rejection')
        require(all(r.get(k) is True for k in ('complete','returned','result','restored')) and r.get('injected') is (stage==1),'call/restoration result')
        require(integer(r['original_jitter'])==integer(r['restored_jitter'])==0,'camera not restored to zero')
        require(integer(r['projection_active'],1,255)>0 and integer(r['display_index'],0,255)==0 and (integer(r['view_flags'],0,0xffffffff)&0x840000)==0,'camera/render eligibility')
        if h['version']=='1.0.2':
            require(integer(r['admission_checkpoint'])==100 and integer(r['camera_stamp'])==(r['anchor']['epoch']&0xffffffff),'admission checkpoint/stamp')
            for key in ('provider_before','provider_after'):
                s=r[key]
                require(s.get('valid') is True and integer(s['backend'],1)<=0xffffffffffffffff-0x42708 and integer(s['wrapper'])==0 and integer(s['use_dlss'],0,1)<=1 and (integer(s['sizing_flags'],0,0xffffffff)&1)==0,'provider exclusion')
            require(r['provider_before']==r['provider_after'],'provider state changed')
        else:
            require(integer(r['use_dlss'],0,255)==0,'legacy raw-switch gate')
        require(integer(r['projection_object'],1)==r['camera']+0x50 and r['caller']==h['base']+0x228e834,'embedded view/caller')
        for key in ('camera','camera_type','caller','thread'): integer(r[key],1)
        require(r['camera_type']-h['base'] in (0x350d188,0x33f82a8),'camera type')
        tick,exit_tick=integer(r['tick']),integer(r['exit_tick'])
        require(started<=tick<=exit_tick and tick-started<1500,'call time bound')
        a,p=r['anchor'],r['projection']
        for key in ('renderer','context','view','table'): integer(a[key],1)
        for key in ('frame','epoch'): integer(a[key])
        require(a['context']==a['renderer']+0x710 and integer(a['bank'],0,1)<=1 and integer(a['async_mode'],0,1)<=1,'recording context')
        if previous:
            require(all(r[k]==samples[0][k] for k in ('camera','camera_type','thread')) and all(a[k]==samples[0]['anchor'][k] for k in ('renderer','context','view','table')),'changed anchors')
            require(a['frame']>previous['anchor']['frame'] and a['epoch']>previous['anchor']['epoch'] and tick>=previous['exit_tick'],'phase order')
            require(p['viewport_bits']==samples[0]['projection']['viewport_bits'],'viewport changed')
        require(p.get('valid') is True and p.get('matrix_relation') is True and integer(p['rejection'])==0 and integer(p['checkpoint'])==100 and 1<=integer(p['reads'])<=192,'native snapshot invalid')
        require(p['camera']==r['camera'] and p['vtable_rva']==r['camera_type']-h['base'] and p['epoch']==(a['epoch']&0xffffffff),'projection source changed')
        integer(p['buffer_id'],0,0xfffffffe)
        words=integer(p['words'],16,4096); constants=integer(p['constants'],1)
        require(constants%16==0,'constant alignment')
        offsets=[]
        for key in ('projection_id','unjittered_id'):
            v=integer(p[key],0,0xfffffffe); offset=(v>>12)&0x3ff
            require(((v>>8)&15)==15 and offset+16<=min(words,1024),'matrix layout'); offsets.append(offset)
        require(abs(offsets[0]-offsets[1])>=16,'overlapping matrices')
        candidate=pair(r['candidate']); x,y=floats(candidate,2)
        require(max(abs(x),abs(y))<=0.5 and (x!=0 or y!=0),'native sample range')
        require(p['table20_bits']==candidate and p['jitter_bits']==(candidate if stage==1 else [0,0]),'wrong sample or jitter')
        jx,jy=floats(p['jitter_bits'],2); width,height=floats(p['viewport_bits'],2)
        require(width>0 and height>0,'viewport')
        proj,plain=floats(p['projection_bits'],16),floats(p['unjittered_bits'],16)
        require(any(plain[4*row+3]!=0 for row in range(4)),'degenerate projection')
        for row in range(4):
            factor=plain[4*row+3]
            for col in range(4):
                idx=4*row+col; delta=factor*jx/width if col==0 else factor*jy/height if col==1 else 0
                tolerance=8*2**-23*max(1e-6,abs(proj[idx]),abs(plain[idx]),abs(delta))
                require(abs((proj[idx]-plain[idx])-delta)<=tolerance,'independent matrix relation failed')
        difference=max(abs(a-b) for a,b in zip(proj,plain)); delta_max.append(difference)
        require(stage!=1 or difference>0,'stimulus produced no measured matrix change')
        previous=r
    return dict(frames=[r['anchor']['frame'] for r in samples],epochs=[r['anchor']['epoch'] for r in samples],
                projection_delta_max=delta_max,camera_restored=True)

def review(rows):
    h=rows[0]
    out=dict(pid=h['pid'],creation=h['creation'],accepted_cpu_experiment=False,gpu_qualified=False,
             limitation='CPU constants and scoped camera restoration only. No proof of raster use, motion-vector correctness, native TAA activation or reversal of rendered history.')
    try:
        out.update(validate_capture(rows)); out['accepted_cpu_experiment']=True
    except (ValueError,KeyError,TypeError,OverflowError) as error:
        out['rejection']=str(error)
    out['failed_events']=[r for r in rows if r['event']=='failed']
    if any(r['event']=='capture_complete' for r in rows):
        out['capture_terminal']=[r for r in rows if r['event']=='capture_complete'][-1]
    out['rejected_samples']=[dict(stage=r.get('stage'),rejection=r.get('rejection'),admission_checkpoint=r.get('admission_checkpoint')) for r in rows if r['event']=='sample' and r.get('rejection')]
    return out

def main():
    ap=argparse.ArgumentParser(description=__doc__); ap.add_argument('log',type=Path); ap.add_argument('--output',type=Path); a=ap.parse_args()
    require(a.log.stat().st_size<=MAX_BYTES,'capture size'); raw=a.log.read_bytes(); result=review(parse(raw))
    result['capture_sha256']=hashlib.sha256(raw).hexdigest()
    if a.output:
        with a.output.open('x',encoding='utf-8') as f: json.dump(result,f,indent=2)
    print(json.dumps(result,indent=2))

if __name__=='__main__': main()
