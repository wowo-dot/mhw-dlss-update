"""One Performance DLSS frame and its same-recording output chain; no continuous claim."""
import hashlib
import json
import math
from pathlib import Path
from temporal_shader import require
from projection_contract import floats
from temporal_inline import pack_reference
from temporal_bindings import review_bindings
from temporal_sdk import review_startup_link


def header_contract(h):
    require(h['version'] in ('1.24.0','1.24.1','1.24.2','1.24.3','1.24.4','1.24.5') and h.get('full_output_route') is True,'full output version')
    require([h.get(k) for k in ('output_receipt','output_draw_limit','output_copy_limit','native_taa_replacement_limit',
                              'binding_hooks','inline_receipt','presentation_writes','sdk_evaluation_limit','sdk_create_limit')]
            ==[2,3,1,1,23,2,1,1,1],'full output budgets')
    if h['version'] in ('1.24.1','1.24.2','1.24.3','1.24.4','1.24.5'):
        require(h.get('hooks')==49 and h.get('viewport_hook_owner')=='binding-observer' and h.get('resolution_viewport_listener') is True,'viewport ownership budget')
    if h['version'] in ('1.24.2','1.24.3','1.24.4','1.24.5'):
        require(h.get('binding_hook_control')=='owned-targets-only','binding hook control')
    require(h.get('binding_observer') is True and h.get('inline_same_list') is True,'same-list output contract')


def route_contract(c):
    inline=c['inline_dlaa'];r=inline['full_output_route'];s=c['submission']['samples'][2]
    require(r['schema']==1 and r['receipt']==2 and r['fault']==r['pending']==0 and r['failure']=='','output route health')
    require(all(r[k] is True for k in ('claimed','resolved','retired','hooks_disabled')),'output route closure')
    require([r[k] for k in ('prepared_draws','replaced_draws','copy_count')]==[9,3,1],'output route counts')
    require(r['command']==s['command'] and r['generation']==s['reset'] and r['frame']==s['frame'],'output recording identity')
    handoff=c['final_handoff']['records'][2]
    require(r['copy_packet']==handoff['packet']['object'],'output copy packet identity')
    indices=[r['copy_event'],*r['draw_events']];events=c['submission']['events']
    require(len(indices)==4 and len(set(indices))==4 and all(type(i) is int and 0<=i<len(events) for i in indices),'output marker identities')
    previous=s['dispatch_exit']
    for i in indices:
        e=events[i]
        require(e['kind']==4 and e['object']==r['command'] and e['valid'] is True and e['returned'] is True and
                previous<e['enter']<e['exit']<s['close'],'output markers within completed recording')
        previous=e['exit']
    draws=c['final_handoff']['downstream']['postfilter']['draw_execution']['records']
    require(len(draws)==9 and len(c['native_compute']['records'])==3,'output record counts')
    for i,d in enumerate(draws):
        require(d['api_calls']==1 and d['replaced'] is (i>=6),'exact three replacement draws')
        if i>=6:require(d['command']==r['command'] and d['frame']==r['frame'],'output draw recording')
    for i,d in enumerate(c['native_compute']['records']):
        require(d['dispatch_replaced'] is (i==2) and d['native_dispatch_forwards']==(0 if i==2 else 1),'native TAA bypass count')
    return r


def review(h,c,folder):
    import numpy as np
    header_contract(h);route_contract(c);review_bindings(c,sample_count=3,hook_count=23)
    folder=Path(folder);inline=c['inline_dlaa'];p=inline['pass'];s=c['submission']['samples'][2];r=c['records'][2]
    require(inline['receipt']==2 and inline['resolve_outcome']==1 and inline['failure']=='' and
            all(inline[k] is True for k in ('attempted','bindings_equal','saved')),'reconstructed inline completion')
    require(p['failure']=='' and all(p[k] is True for k in ('creation_completed','attempted','issued','restored','copied','collected','released','shutdown_deferred')),'SDK closure')
    require([p[k] for k in ('create_result','evaluate_result','release_result','destroy_result','evaluations','resets','captures')]==[1]*7,'one successful SDK frame')
    require(all(p['last_attempt'][k] is True for k in ('commands','bindings_restored','native_output_written','success')),'SDK transaction')
    w,hgt=c['width'],c['height'];rw,rh=w//2,hgt//2
    require(p['geometry_schema']==1 and p['allocation']==p['output_extent']==[w,hgt] and p['render']==[rw,rh] and
            [p['width'],p['height']]==[rw,rh] and p['input_origin']==[0,0],'Performance geometry')
    require([p[k] for k in ('create_flags','mode','preset','input_formats','converted_formats','mv_scale','pre_exposure')]==
            [75,0,11,[26,41,42],[2,41,16],[1,1],1] and p['mv_jittered'] is False and p['reset'] is True,'Performance SDK contract')
    require(p['motion_units']==p['jitter_units']=='render_pixels' and p['continuous'] is True,'scaled input units')
    require(0<p['allocation_bytes']<=h['sdk_image_byte_limit'] and p['device']==c['device'],'SDK device/budget')
    require(p['frame']==r['frame']==s['frame'] and p['epoch']==r['epoch'] and p['generation']==s['reset'] and p['command']==s['command'],'SDK recording identity')
    require(p['completion_fence']==s['fence'] and p['completion_value']==s['completed_value']==1 and s['inline_ok'] is True,'SDK completion fence')
    require(s['dispatch_forward']<s['inline_begin']<s['inline_end']<s['dispatch_exit'],'SDK recording interval')
    require(all(x['inline_begin']==x['inline_end']==0 and x['inline_ok'] is False for x in c['submission']['samples'][:2]),'one inline receipt')
    require(p['native_output']==r['inputs'][4]['resource'] and p['raw']==c['owned_gpu_inputs']['sets'][2]['inputs'],'SDK resource identity')
    x,y=floats(c['projection_stimulus']['records'][2]['jitter_bits'],2)
    require(len(p['jitter'])==2 and all(math.isfinite(a) and abs(a-b)<1e-8 for a,b in zip(p['jitter'],(x/2,-y/2))),'frame jitter mapping')
    require(math.isfinite(p['frame_ms']) and abs(p['frame_ms']-16.666667)<1e-5,'diagnostic frame time')
    sizes=[rw*rh*16,rw*rh*4,rw*rh*8,w*hgt*16,w*hgt*4]
    suffixes=('color.f32','depth.f32','motion.f32','dlaa.rgba32f','native.u32');arrays=[]
    require(len(inline['files'])==5,'SDK artifact count')
    for index,(a,size,suffix) in enumerate(zip(inline['files'],sizes,suffixes)):
        require(a['file']==h['motion_prefix']+'.inline-'+suffix,'SDK artifact name');path=folder/a['file']
        require(path.stat().st_size==a['bytes']==size,'SDK artifact extent');raw=path.read_bytes()
        require(hashlib.sha256(raw).hexdigest()==a['sha256'],'SDK artifact hash')
        arrays.append(np.frombuffer(raw,dtype='<u4' if index==4 else '<f4'))
    for i,channels in enumerate((4,1,2)):
        reference=c['owned_gpu_inputs']['sets'][2]['files'][i];raw=(folder/reference['file']).read_bytes()
        expected=np.frombuffer(raw,dtype='<f4').reshape(hgt,w,channels)[:rh,:rw,:].copy()
        if i==2:expected*=.5  # Independent converter reports allocation-pixel units.
        require(np.array_equal(arrays[i].reshape(rh,rw,channels),expected),'render-sized converted input pixels')
    rgb=arrays[3].reshape(-1,4)[:,:3]
    require(np.isfinite(rgb).all() and not np.any(rgb==65504),'finite full DLSS image')
    require(np.array_equal(arrays[4],pack_reference(rgb)),'full packed DLSS handoff')
    startup=folder/f"startup-{h['pid']}-{h['creation']}.jsonl"
    review_startup_link(h,c,[json.loads(line) for line in startup.read_text().splitlines() if line.strip()])
    return dict(dlss_verified=True,dlss_frames=1,native_taa_replacements=1,full_output_handoff_verified=True,
                output_draw_gpu_submission_verified=True,full_output_draws=3,continuous_dlss_verified=False,
                meaning='One Performance DLSS frame, expanded native copy and three same-recording full-size effects; continuous quality not tested')
