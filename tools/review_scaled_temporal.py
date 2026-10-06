"""Verify scaled native input capture and restoration without claiming DLSS readiness."""
import argparse
import hashlib
import json
from pathlib import Path
import review_temporal_inputs as base
from review_render_resolution import review as review_scale
require=base.require
MAX_BYTES=base.MAX_BYTES


def parse(raw):
    require(0<len(raw)<=MAX_BYTES,'log size')
    rows=[json.loads(line) for line in raw.decode().splitlines() if line.strip()]
    require(rows and all(isinstance(r,dict) and 'event' in r for r in rows),'records')
    h=rows[0]
    require(h.get('version') in ('1.23.0','1.23.1','1.23.2','1.23.3','1.23.4','1.23.5','1.23.6','1.23.7','1.23.8','1.23.9','1.23.10','1.23.11','1.23.12','1.24.0','1.24.1','1.24.2','1.24.3','1.24.4','1.24.5') and h.get('scaled_temporal') is True and h.get('scale')==.5,'scaled experiment')
    projection=h['version'] in ('1.23.4','1.23.5','1.23.6','1.23.7','1.23.8','1.23.9','1.23.10','1.23.11','1.23.12','1.24.0','1.24.1','1.24.2','1.24.3','1.24.4','1.24.5')
    if h['version'] in ('1.23.6','1.23.7','1.23.8','1.23.9','1.23.10','1.23.11','1.23.12','1.24.0','1.24.1','1.24.2','1.24.3','1.24.4','1.24.5'):require(h.get('depth_projection_gate') is True,'depth projection gate')
    if h['version'] in ('1.23.5','1.23.6','1.23.7','1.23.8','1.23.9','1.23.10','1.23.11','1.23.12','1.24.0','1.24.1','1.24.2','1.24.3','1.24.4','1.24.5'):require(h.get('projection_activation_scope') is True,'projection activation scope')
    require(h.get('hooks')==(49 if h['version'] in ('1.24.1','1.24.2','1.24.3','1.24.4','1.24.5') else 50 if h['version'] in ('1.24.0','1.24.1','1.24.2','1.24.3','1.24.4','1.24.5') else 26 if h['version'] in ('1.23.11','1.23.12','1.24.0','1.24.1','1.24.2','1.24.3','1.24.4','1.24.5') else 25 if h['version'] in ('1.23.9','1.23.10','1.23.11','1.23.12','1.24.0','1.24.1','1.24.2','1.24.3','1.24.4','1.24.5') else 24 if h['version']=='1.23.8' else 23 if h['version']=='1.23.7' else 20 if h['version']=='1.23.0' else 22 if projection else 21) and h.get('capacity')==3 and h.get('jitter_writes')==(1 if projection else 0) and
            h.get('sdk_evaluation_limit')==(1 if h['version'] in ('1.24.0','1.24.1','1.24.2','1.24.3','1.24.4','1.24.5') else 0) and all(h.get(k) is True for k in ('pre_unhook_cutoff','journal_drain','packet_generations')),'scaled budgets')
    if h['version']!='1.23.0':require(h.get('depth_draw_capture') is True and h.get('depth_draw_hooks')==1,'depth draw budget')
    if h['version'] in ('1.23.3','1.23.4','1.23.5','1.23.6','1.23.7','1.23.8','1.23.9','1.23.10','1.23.11','1.23.12','1.24.0','1.24.1','1.24.2','1.24.3','1.24.4','1.24.5'):require(h.get('color_state_admission')=='native-current-use-v1' and h.get('color_state_proof_limit')==3,'color state proof budget')
    if projection:
        require(h.get('scaled_projection') is True and
                [h.get(k) for k in ('projection_samples','projection_byte_limit','jitter_restorations','jitter_min_abs','jitter_selection_limit')]==[3,4096,1,.3125,20],'scaled projection budgets')
    if h['version'] in ('1.23.7','1.23.8','1.23.9','1.23.10','1.23.11','1.23.12','1.24.0','1.24.1','1.24.2','1.24.3','1.24.4','1.24.5'):
        require(h.get('final_handoff_observer') is True and h.get('final_handoff_samples')==3 and h.get('final_handoff_mutations')==0,'final handoff budgets')
    if h['version'] in ('1.23.8','1.23.9','1.23.10','1.23.11','1.23.12','1.24.0','1.24.1','1.24.2','1.24.3','1.24.4','1.24.5'):
        require(h.get('downstream_observer') is True and [h.get(k) for k in ('downstream_samples','downstream_packet_limit','downstream_filter_limit')]==[3,8,16],'downstream budgets')
    if h['version'] in ('1.23.10','1.23.11','1.23.12','1.24.0','1.24.1','1.24.2','1.24.3','1.24.4','1.24.5'):
        require(h.get('colorgrade_observer') is True and h.get('colorgrade_scan_limit')==65535 and h.get('colorgrade_shader_limit')==262144,'colorgrade budgets')
    if h['version'] in ('1.23.9','1.23.10','1.23.11','1.23.12','1.24.0','1.24.1','1.24.2','1.24.3','1.24.4','1.24.5'):
        require(h.get('postfilter_observer') is True and [h.get(k) for k in ('postfilter_samples','postfilter_command_limit','postfilter_shader_limit','postfilter_mutations')]==[3,32,262144,0],'postfilter budgets')
    if h['version'] in ('1.23.11','1.23.12','1.24.0','1.24.1','1.24.2','1.24.3','1.24.4','1.24.5'):
        require(h.get('draw_execution_observer') is True and h.get('draw_execution_samples')==9 and h.get('draw_execution_shader_limit')==262144,'draw execution budgets')
    # Reuse the exact existing non-SDK three-input-capture header budget checks.
    # This is header validation only; retain the actual version for all evidence.
    compatible=dict(h,version='1.10.0',hooks=10,jitter_writes=0)
    base.parse(json.dumps(compatible).encode())
    if h['version'] in ('1.24.0','1.24.1','1.24.2','1.24.3','1.24.4','1.24.5'):
        from temporal_scaled_output import header_contract
        header_contract(h)
    return rows


def scales(c,s):
    require(len(c['records'])==3,'three scaled input records')
    for r in c['records']:
        stamp=r.get('scale_state',{})
        require(stamp.get('stable') is True and stamp.get('frame')==r['frame'] and stamp.get('view')==r['view'],'frame-aligned scale')
        require(stamp.get('active')==.5 and stamp.get('content')==1 and
                stamp.get('display')==s['display']==[c['width'],c['height']] and stamp.get('render')==s['render'],'scale dimensions')
    return dict(active_region=s['render'],allocation_extent=s['display'],native_rectangles=[r['rectangle'] for r in c['records']],
                dispatch_groups=[r['groups'] for r in c['records']])


def review(rows,folder,expected_hash):
    require([r['event'] for r in rows]==['start','reservation','ready','armed','capture','resolution','scaled_temporal','stopped'],'event sequence')
    h,res,ready,armed,c,s,context,end=rows
    require(h['dll_sha256']==expected_hash,'DLL identity')
    result=base.review([h,res,ready,armed,c,end],cpu_only=True)
    shutdown=c['shutdown'];times=[shutdown[k] for k in ('drain_started_tick','drain_finished_tick','journal_stopped_tick','unhook_started_tick','finish_tick')]
    require(shutdown['callbacks_drained'] and shutdown['journal_stopped_before_unhook'] and times==sorted(times) and times[0]>0 and times[1]-times[0]<=2000,'capture drain')
    require(c['submission']['admission_closed'] and c['submission']['stopped_tick']==times[2],'journal cutoff')
    require(all(c[k]['drained_before_disable'] for k in ('native_compute','depth_routes')),'consumer drain')
    for function,args,kwargs in (
        (base.review_shader,(c,folder/h['shader_file']),{}),
        (base.review_consumer,(h,c),{}),
        (base.review_submission,(c,),dict(sample_count=3)),
        (base.review_motion,(h,c,folder),{}),
        (base.review_color,(h,c,folder),{}),
        (base.review_scene_depth,(c,),dict(sample_count=3,paired_jitter=h['version'] in ('1.23.4','1.23.5','1.23.6','1.23.7','1.23.8','1.23.9','1.23.10','1.23.11','1.23.12','1.24.0','1.24.1','1.24.2','1.24.3','1.24.4','1.24.5'))),
        (base.review_depth_routes,(h,c),dict(selected_scope=True)),
        (base.review_depth_pixels,(h,c,folder),{}),
        (base.review_owned,(h,c,folder),{}),
    ):result.update(function(*args,**kwargs))
    # The scale evaluator accepts an explicit synthetic scale-only envelope;
    # its evidence record remains the real record from this combined capture.
    scale_header=dict(h,requested_scale=.5)
    scale_end=dict(event='stopped',armed=True,healthy=True,cleaned=True,dll_pinned=True,reason='requested')
    scale_result=review_scale([scale_header,res,ready,dict(event='armed',installed=True),s,scale_end],expected_hash)
    require(context.get('schema')==1 and context.get('activation_disabled') is True and context.get('scale_finished') is True,'combined cleanup')
    if h['version']!='1.23.0':require(context.get('activation_unhooked') is True,'activation patch removal')
    a=context['activation']
    require(a['policy']=='native-postqueue-preparation-v2' and a['fault']==a['pending']==a['active_context']==0 and
            not a['requested'] and a['hooks_disabled'] and a['settings_writes']==a['layer_flag_writes']==0 and a['taa_admitted']>=3,'native activation cleanup')
    if h['version'] in ('1.24.0','1.24.1','1.24.2','1.24.3','1.24.4','1.24.5'):
        require(context.get('dlss_evaluation_limit')==1 and context.get('motion_conversion_basis')=='render-dimensions','scaled SDK conversion scope')
    else:
        require(context.get('dlss_evaluations')==0 and context.get('motion_conversion_basis')=='allocation-dimensions-diagnostic-only','conversion scope')
    if h['version'] in ('1.24.1','1.24.2','1.24.3','1.24.4','1.24.5'):
        require(s.get('shared_viewport') is True and s.get('owned_hooks')==1,'single viewport hook owner')
        b=c['native_bindings']
        require(b.get('viewport_listener') is True and type(b.get('viewport_notifications')) is int and
                0<sum(s['viewport_counts'])<=b['viewport_notifications'],'shared viewport observations')
        if h['version'] in ('1.24.2','1.24.3','1.24.4','1.24.5'):require(b.get('hook_control')=='owned-targets-only','binding hook control')
    result.update(scales(c,s))
    if h['version'] in ('1.23.7','1.23.8','1.23.9','1.23.10','1.23.11','1.23.12','1.24.0','1.24.1','1.24.2','1.24.3','1.24.4','1.24.5'):
        from temporal_final_handoff import review as review_handoff
        result.update(review_handoff(h,c))
        if h['version'] in ('1.23.8','1.23.9','1.23.10','1.23.11','1.23.12','1.24.0','1.24.1','1.24.2','1.24.3','1.24.4','1.24.5'):
            from temporal_downstream import review as review_downstream
            result.update(review_downstream(h,c['final_handoff']))
        if h['version'] in ('1.23.9','1.23.10','1.23.11','1.23.12','1.24.0','1.24.1','1.24.2','1.24.3','1.24.4','1.24.5'):
            from temporal_postfilter import review as review_postfilter
            result.update(review_postfilter(h,c['final_handoff']['downstream'],folder))
    if h['version'] in ('1.23.11','1.23.12','1.24.0','1.24.1','1.24.2','1.24.3','1.24.4','1.24.5'):
        from temporal_draw_execution import review as review_draw_execution
        result.update(review_draw_execution(h,c['final_handoff']['downstream'],folder))
    if h['version'] in ('1.23.4','1.23.5','1.23.6','1.23.7','1.23.8','1.23.9','1.23.10','1.23.11','1.23.12','1.24.0','1.24.1','1.24.2','1.24.3','1.24.4','1.24.5'):
        from temporal_jitter import review_jitter
        result.update(review_jitter(h,c))
    result.update(scale_restoration=scale_result,dlss_verified=False,
        scaled_motion_convention_verified=False,meaning='Three native temporal inputs captured at half active scale; full-size restoration verified')
    if h['version'] in ('1.24.0','1.24.1','1.24.2','1.24.3','1.24.4','1.24.5'):
        from temporal_scaled_output import review
        result.update(review(h,c,folder))
    return result


def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--log',type=Path,required=True)
    ap.add_argument('--dll-sha256',required=True);ap.add_argument('--output',type=Path,required=True);a=ap.parse_args()
    require(not a.output.exists(),'preserve review');require(a.log.stat().st_size<=MAX_BYTES,'log size');raw=a.log.read_bytes()
    try:result=review(parse(raw),a.log.parent,a.dll_sha256)
    except (ValueError,KeyError,TypeError,OSError,IndexError) as e:result=dict(accepted=False,reason=str(e),dlss_verified=False)
    result['log_sha256']=hashlib.sha256(raw).hexdigest()
    with a.output.open('x') as out:json.dump(result,out,indent=2)
    print(json.dumps(result,indent=2));return 0 if result['accepted'] else 1
if __name__=='__main__':raise SystemExit(main())
