"""Review bounded ordinary-TAA CPU request observations, never GPU eligibility."""
import argparse
import hashlib
import json
from pathlib import Path
from temporal_shader import review_shader
from temporal_consumer import review_consumer
from temporal_submission import review_submission
from temporal_motion import review_motion
from temporal_color import review_color
from temporal_jitter import review_jitter
from temporal_scene_depth import review_scene_depth
from temporal_depth_routes import review_depth_routes
from temporal_depth_pixels import review_depth_pixels
from temporal_owned_inputs import review_owned
from temporal_sdk import review_live_sdk
from temporal_bindings import review_bindings
from temporal_inline import review_inline
from temporal_limits import frame_count

MAX_BYTES=2*1024*1024
HOST='c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea'
ROLES=['tBaseMap','tBlendMap','tDepthMap','tGBuffer1','rwOutput','tGBuffer4','tPrevGBufferID']
def require(ok,reason):
    if not ok:raise ValueError(reason)
def parse(raw):
    require(0<len(raw)<=MAX_BYTES,'log size')
    rows=[json.loads(line) for line in raw.decode('utf-8').splitlines() if line.strip()]
    require(rows and all(isinstance(r,dict) for r in rows),'records')
    h=rows[0];n=frame_count(h)
    require(h.get('event')=='start' and h.get('schema')==1 and h.get('version') in ('1.0.0','1.1.0','1.2.0','1.3.0','1.4.0','1.5.0','1.6.0','1.6.1','1.7.0','1.8.0','1.9.0','1.10.0','1.11.0','1.13.0','1.14.0','1.15.0','1.15.1','1.15.2','1.15.3','1.16.0','1.17.0'),'header')
    require(h['version'] in ('1.3.0','1.4.0','1.5.0','1.6.0','1.6.1','1.7.0','1.8.0','1.9.0','1.10.0','1.11.0','1.13.0','1.14.0','1.15.0','1.15.1','1.15.2','1.15.3','1.16.0','1.17.0') or len(raw)<=262144,'historical log size')
    require(h.get('host_sha256')==HOST and h.get('host_version')=='0.0.0.0','host')
    require(h.get('experiment')=='ordinary-taa-input-requests','experiment')
    require([h.get(k) for k in ('capacity','window_ms','jitter_writes','gpu_commands','hooks')]==[n,2000,7 if h['version'] in ('1.15.0','1.15.1','1.15.2','1.15.3','1.16.0','1.17.0') else 1 if h['version'] in ('1.6.0','1.6.1','1.14.0','1.15.0','1.15.1','1.15.2','1.15.3','1.16.0','1.17.0') else 0,12*n if h['version'] in ('1.10.0','1.11.0','1.13.0','1.14.0','1.15.0','1.15.1','1.15.2','1.15.3','1.16.0','1.17.0') else 27 if h['version']=='1.9.0' else 18 if h['version'] in ('1.5.0','1.6.0','1.6.1','1.7.0','1.8.0','1.9.0','1.10.0','1.11.0','1.13.0','1.14.0','1.15.0','1.15.1','1.15.2','1.15.3','1.16.0','1.17.0') else 9 if h['version']=='1.4.0' else 0,29 if h['version'] in ('1.16.0','1.17.0') else 11 if h['version'] in ('1.14.0','1.15.0','1.15.1','1.15.2','1.15.3','1.16.0','1.17.0') else 10 if h['version'] in ('1.8.0','1.9.0','1.10.0','1.11.0','1.13.0','1.14.0','1.15.0','1.15.1','1.15.2','1.15.3','1.16.0','1.17.0') else 8 if h['version'] in ('1.6.0','1.6.1','1.14.0','1.15.0','1.15.1','1.15.2','1.15.3','1.16.0','1.17.0') else 7 if h['version'] in ('1.3.0','1.4.0','1.5.0','1.6.0','1.6.1','1.7.0','1.8.0','1.9.0','1.10.0','1.11.0','1.13.0','1.14.0','1.15.0','1.15.1','1.15.2','1.15.3','1.16.0','1.17.0') else 4 if h['version']=='1.2.0' else 1],'budgets')
    require(all(type(h.get(k)) is int and h[k]>0 for k in ('pid','creation','base')),'process identity')
    sha=h.get('dll_sha256','');require(len(sha)==64 and all(c in '0123456789abcdef' for c in sha),'DLL hash')
    if h['version'] in ('1.1.0','1.2.0','1.3.0','1.4.0','1.5.0','1.6.0','1.6.1','1.7.0','1.8.0','1.9.0','1.10.0','1.11.0','1.13.0','1.14.0','1.15.0','1.15.1','1.15.2','1.15.3','1.16.0','1.17.0'):
        require([h.get(k) for k in ('shader_limit','owner_limit','binding_limit')]==[262144,64,128],'shader budgets')
        require(h.get('shader_file')==f"inputs-{h['pid']}-{h['creation']}.cs.dxbc",'shader file')
    if h['version'] in ('1.2.0','1.3.0','1.4.0','1.5.0','1.6.0','1.6.1','1.7.0','1.8.0','1.9.0','1.10.0','1.11.0','1.13.0','1.14.0','1.15.0','1.15.1','1.15.2','1.15.3','1.16.0','1.17.0'):require([h.get(k) for k in ('barrier_limit','barrier_call_limit')]==[128,16],'barrier budgets')
    if h['version'] in ('1.3.0','1.4.0','1.5.0','1.6.0','1.6.1','1.7.0','1.8.0','1.9.0','1.10.0','1.11.0','1.13.0','1.14.0','1.15.0','1.15.1','1.15.2','1.15.3','1.16.0','1.17.0'):
        require([h.get(k) for k in ('warmup_ms','event_limit','list_limit','allocator_limit','queue_limit','batch_limit','submitted_limit','private_signal_limit')]==
                [150,4096,128,128,8,128,8192,n],'submission budgets')
    if h['version'] in ('1.4.0','1.5.0','1.6.0','1.6.1','1.7.0','1.8.0','1.9.0','1.10.0','1.11.0','1.13.0','1.14.0','1.15.0','1.15.1','1.15.2','1.15.3','1.16.0','1.17.0'):
        require(h.get('motion_copy_limit')==n and h.get('motion_byte_limit')==16777216,'motion budgets')
        require(h.get('motion_prefix')==f"inputs-{h['pid']}-{h['creation']}",'motion prefix')
    if h['version'] in ('1.5.0','1.6.0','1.6.1','1.7.0','1.8.0','1.9.0','1.10.0','1.11.0','1.13.0','1.14.0','1.15.0','1.15.1','1.15.2','1.15.3','1.16.0','1.17.0'):
        require([h.get(k) for k in ('color_copy_limit','color_byte_limit','total_readback_limit')]==[n,16777216,50331648*n if h['version'] in ('1.9.0','1.10.0','1.11.0','1.13.0','1.14.0','1.15.0','1.15.1','1.15.2','1.15.3','1.16.0','1.17.0') else 100663296],'color budgets')
    if h['version'] in ('1.6.0','1.6.1','1.14.0','1.15.0','1.15.1','1.15.2','1.15.3','1.16.0','1.17.0'):
        require([h.get(k) for k in ('projection_samples','projection_byte_limit','jitter_restorations')]==[n,4096,n-2],'projection budgets')
    if h['version'] in ('1.6.1','1.14.0','1.15.0','1.15.1','1.15.2','1.15.3','1.16.0','1.17.0'):
        require([h.get(k) for k in ('jitter_min_abs','jitter_selection_limit')]==[.3125,20],'jitter selection budgets')
    if h['version'] in ('1.7.0','1.8.0','1.9.0','1.10.0','1.11.0','1.13.0','1.14.0','1.15.0','1.15.1','1.15.2','1.15.3','1.16.0','1.17.0'):
        require([h.get(k) for k in ('scene_depth_samples','scene_depth_copies','descriptor_writes')]==[n,n if h['version'] in ('1.9.0','1.10.0','1.11.0','1.13.0','1.14.0','1.15.0','1.15.1','1.15.2','1.15.3','1.16.0','1.17.0') else 0,0],'scene depth budgets')
    if h['version'] in ('1.8.0','1.9.0','1.10.0','1.11.0','1.13.0','1.14.0','1.15.0','1.15.1','1.15.2','1.15.3','1.16.0','1.17.0'):
        require([h.get(k) for k in ('depth_route_limit','depth_route_barrier_limit','depth_route_call_limit')]==[64,32,8],'depth route budgets')
    if h['version'] in ('1.9.0','1.10.0','1.11.0','1.13.0','1.14.0','1.15.0','1.15.1','1.15.2','1.15.3','1.16.0','1.17.0'):
        require([h.get(k) for k in ('depth_copy_limit','depth_byte_limit','depth_selected_scope')]==[n,16777216,True],'depth pixel budgets')
    if h['version'] in ('1.10.0','1.11.0','1.13.0','1.14.0','1.15.0','1.15.1','1.15.2','1.15.3','1.16.0','1.17.0'):
        require([h.get(k) for k in ('owned_copy_limit','owned_descriptor_writes','owned_set_byte_limit',
                'owned_execute_limit','owned_signal_limit','owned_dispatch_limit','owned_readback_limit')]==
                [3*n,6*n,314572800,n,n,n,117440512*n], 'owned GPU budgets')
    if h['version'] in ('1.11.0','1.13.0','1.14.0','1.15.0','1.15.1','1.15.2','1.15.3','1.16.0','1.17.0'):
        require([h.get(k) for k in ('sdk_evaluation_limit','sdk_create_limit','sdk_image_byte_limit','sdk_output_limit','sdk_shutdown_deferred')]==([1,1,419430400,67108864,True] if h['version']=='1.17.0' else [n,1,150994944,67108864*n,True]), 'SDK budgets')
    if h['version'] in ('1.15.1','1.15.2','1.15.3','1.16.0','1.17.0'):require(h.get('packet_generations') is True,'packet generation contract')
    if h['version'] in ('1.15.2','1.15.3','1.16.0','1.17.0'):require(h.get('journal_drain') is True,'journal drain contract')
    if h['version'] in ('1.15.3','1.16.0','1.17.0'):require(h.get('pre_unhook_cutoff') is True,'pre-unhook cutoff contract')
    if h['version'] in ('1.16.0','1.17.0'):
        require(h.get('binding_observer') is True and [h.get(k) for k in ('binding_hooks','binding_lists','binding_arguments','binding_constant_words','presentation_writes')]==[18,128,64,64,1 if h['version']=='1.17.0' else 0],'native binding budgets')
    if h['version']=='1.17.0':require(h.get('inline_receipt')==4 and h.get('inline_same_list') is True,'inline budget')
    return rows
def review(rows,shader_path=None,*,cpu_only=False):
    require([r['event'] for r in rows]==['start','reservation','ready','armed','capture','stopped'],'event sequence')
    h,res,ready,armed,c,end=rows;n=frame_count(h)
    require(res.get('ready') is True and res.get('slot',0)>0 and res.get('hooks_installed') is False,'reservation')
    require(ready.get('hooks_installed') is False and armed.get('installed') is True and armed.get('reason')=='','arming')
    require(c.get('fault')==0 and c.get('pending')==0 and c.get('hooks_disabled') is True,'observer closure')
    if h['version'] in ('1.15.2','1.15.3','1.16.0','1.17.0'):
        require(all(c[k].get('drained_before_disable') is True for k in ('native_compute','depth_routes')),'dependent callbacks drained before disable')
    if h['version'] in ('1.15.3','1.16.0','1.17.0'):
        shutdown=c['shutdown']
        require(shutdown.get('callbacks_drained') is True and shutdown.get('journal_stopped_before_unhook') is True,'shutdown cutoff result')
        times=[shutdown[k] for k in ('drain_started_tick','drain_finished_tick','journal_stopped_tick','unhook_started_tick','finish_tick')]
        require(all(type(t) is int and t>0 for t in times) and c['started_tick']<=times[0] and times==sorted(times) and times[1]-times[0]<=2000,'shutdown cutoff ordering')
        require(c['submission'].get('admission_closed') is True and c['submission'].get('stopped_tick')==times[2],'submission cutoff binding')
    pins=8*n if h['version'] in ('1.7.0','1.8.0','1.9.0','1.10.0','1.11.0','1.13.0','1.14.0','1.15.0','1.15.1','1.15.2','1.15.3','1.16.0','1.17.0','1.23.0','1.23.1','1.23.2','1.23.3','1.23.4','1.23.5','1.23.6','1.23.7','1.23.8','1.23.9','1.23.10','1.23.11','1.23.12','1.24.0','1.24.1','1.24.2','1.24.3','1.24.4','1.24.5') else 7*n
    require(c.get('pins_acquired')==pins and c.get('pins_released')==pins,'resource references')
    require(c.get('graphics_api')=='D3D12' and c.get('swapchain_format',0)>0 and c.get('width',0)>0 and c.get('height',0)>0,'graphics identity')
    require(all(type(c.get(k)) is int and c[k]>0 for k in ('renderer','backend','device')),'renderer identity')
    require(c.get('gpu_consumption_verified') is False and c.get('motion_contents_verified') is False,'coverage')
    require(end.get('armed') is True and end.get('accepted') is True and end.get('settled') is True and
            end.get('dll_pinned') is True and end.get('trampolines_retained') is True,'stopped')
    records=c.get('records',[]);require(len(records)==n,'request count')
    previous=0;last_tick=c['started_tick'];require(type(last_tick) is int and last_tick>0,'start tick')
    for r in records:
        require(r.get('valid') is True and r.get('returned') is True and r.get('snapshots_equal') is True,'source stability')
        require(r['caller']==h['base']+0x2392f31 and r['context']==c['renderer']+0x710,'call site')
        require(type(r['frame']) is int and r['frame']>previous,'frame order');previous=r['frame']
        require(r['bank'] in (0,1) and r['async'] in (0,1) and r['epoch']>=0 and r['thread']>0,'frame stamp')
        require(last_tick<=r['enter_tick']<=r['exit_tick']<c['started_tick']+2000,'capture interval');last_tick=r['exit_tick']
        require(0<=r['count_before']<65535 and r['count_after']==r['count_before']+1,'queue count')
        require(all(r[k]>0 for k in ('array_before','array_after','packet','state','layer','view','gbuffer')),'request identities')
        require(r['state']==r['packet_state'] and r['opcode']==6 and r['mode']==0 and r['indirect']==0,'packet')
        x0,y0,x1,y1=r['rectangle'];require(0<=x0<x1<=16384 and 0<=y0<y1<=16384,'rectangle')
        require(r['groups']==r['packet_groups']==[(x1-x0+15)//16,(y1-y0+15)//16,1],'dispatch size')
        inputs=r['inputs'];require([v['name'] for v in inputs]==ROLES,'named inputs')
        require(len({v['id']&4095 for v in inputs})==7 and all(v['id']!=0xffffffff for v in inputs),'binding slots')
        require(inputs[4]['owner'] not in (inputs[0]['owner'],inputs[1]['owner']),'output owner alias')
        for v in inputs:
            require(all(v[k]>0 for k in ('owner','type','backing','resource','identity','width','height','engine_width','engine_height','mips','array_size','sample_count')),'resource identity')
            require(v['backing']==(v['secondary'] or v['primary']) and v['dimension']==3 and v['width']<=16384 and v['height']<=16384,'resource description')
    result=dict(accepted=True,meaning=f'{n} CPU TAA input requests observed',frames=[r['frame'] for r in records],
                formats={v['name']:v['format'] for v in records[0]['inputs']},gpu_consumption_verified=False,motion_contents_verified=False,dlaa_ready=False)
    if cpu_only:return result
    if h['version'] in ('1.1.0','1.2.0','1.3.0','1.4.0','1.5.0','1.6.0','1.6.1','1.7.0','1.8.0','1.9.0','1.10.0','1.11.0','1.13.0','1.14.0','1.15.0','1.15.1','1.15.2','1.15.3','1.16.0','1.17.0'):result.update(review_shader(c,shader_path))
    if h['version'] in ('1.2.0','1.3.0','1.4.0','1.5.0','1.6.0','1.6.1','1.7.0','1.8.0','1.9.0','1.10.0','1.11.0','1.13.0','1.14.0','1.15.0','1.15.1','1.15.2','1.15.3','1.16.0','1.17.0'):result.update(review_consumer(h,c))
    if h['version'] in ('1.3.0','1.4.0','1.5.0','1.6.0','1.6.1','1.7.0','1.8.0','1.9.0','1.10.0','1.11.0','1.13.0','1.14.0','1.15.0','1.15.1','1.15.2','1.15.3','1.16.0','1.17.0'):result.update(review_submission(c,sample_count=n))
    if h['version'] in ('1.4.0','1.5.0','1.6.0','1.6.1','1.7.0','1.8.0','1.9.0','1.10.0','1.11.0','1.13.0','1.14.0','1.15.0','1.15.1','1.15.2','1.15.3','1.16.0','1.17.0'):result.update(review_motion(h,c,Path(shader_path).parent if shader_path else None))
    if h['version'] in ('1.5.0','1.6.0','1.6.1','1.7.0','1.8.0','1.9.0','1.10.0','1.11.0','1.13.0','1.14.0','1.15.0','1.15.1','1.15.2','1.15.3','1.16.0','1.17.0'):result.update(review_color(h,c,Path(shader_path).parent if shader_path else None))
    if h['version'] in ('1.6.0','1.6.1','1.14.0','1.15.0','1.15.1','1.15.2','1.15.3','1.16.0','1.17.0'):result.update(review_jitter(h,c))
    if h['version'] in ('1.7.0','1.8.0','1.9.0','1.10.0','1.11.0','1.13.0','1.14.0','1.15.0','1.15.1','1.15.2','1.15.3','1.16.0','1.17.0'):result.update(review_scene_depth(c,paired_jitter=h['version'] in ('1.14.0','1.15.0','1.15.1','1.15.2','1.15.3','1.16.0','1.17.0'),sample_count=n))
    if h['version'] in ('1.8.0','1.9.0','1.10.0','1.11.0','1.13.0','1.14.0','1.15.0','1.15.1','1.15.2','1.15.3','1.16.0','1.17.0'):result.update(review_depth_routes(h,c,selected_scope=h['version'] in ('1.9.0','1.10.0','1.11.0','1.13.0','1.14.0','1.15.0','1.15.1','1.15.2','1.15.3','1.16.0','1.17.0')))
    if h['version'] in ('1.9.0','1.10.0','1.11.0','1.13.0','1.14.0','1.15.0','1.15.1','1.15.2','1.15.3','1.16.0','1.17.0'):result.update(review_depth_pixels(h,c,Path(shader_path).parent if shader_path else None))
    if h['version'] in ('1.10.0','1.11.0','1.13.0','1.14.0','1.15.0','1.15.1','1.15.2','1.15.3','1.16.0','1.17.0'):result.update(review_owned(h,c,Path(shader_path).parent if shader_path else None))
    if h['version'] in ('1.11.0','1.13.0','1.14.0','1.15.0','1.15.1','1.15.2','1.15.3','1.16.0'):result.update(review_live_sdk(h,c,Path(shader_path).parent if shader_path else None))
    if h['version'] in ('1.16.0','1.17.0'):result.update(review_bindings(c,sample_count=n))
    if h['version']=='1.17.0':result.update(review_inline(h,c,Path(shader_path).parent if shader_path else None))
    return result
def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('log',type=Path);ap.add_argument('--output',type=Path,required=True);a=ap.parse_args()
    require(not a.output.exists(),'preserve review');require(a.log.stat().st_size<=MAX_BYTES,'log size');raw=a.log.read_bytes()
    try:
        rows=parse(raw);result=review(rows,a.log.parent/rows[0]['shader_file'] if rows[0]['version'] in ('1.1.0','1.2.0','1.3.0','1.4.0','1.5.0','1.6.0','1.6.1','1.7.0','1.8.0','1.9.0','1.10.0','1.11.0','1.13.0','1.14.0','1.15.0','1.15.1','1.15.2','1.15.3','1.16.0','1.17.0') else None)
    except (ValueError,KeyError,TypeError,OSError,IndexError) as e:result=dict(accepted=False,reason=str(e),gpu_consumption_verified=False,motion_contents_verified=False,dlaa_ready=False)
    result['log_sha256']=hashlib.sha256(raw).hexdigest()
    with a.output.open('x') as out:json.dump(result,out,indent=2)
    print(json.dumps(result,indent=2))
    return 0 if result['accepted'] else 1
if __name__=='__main__':raise SystemExit(main())
