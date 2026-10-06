"""Review a stopped independent AA-Off continuous DLAA run; no visual verdict."""
import argparse
import hashlib
import json
from pathlib import Path
from arm_aa_stream import parse, require
from review_ngx_startup import review as startup_review
from aa_queue_layouts import review_layouts
from aa_input_boundaries import review_boundaries

def review(rows, startup, expected_hash):
    h=rows[0]
    require(h['dll_sha256']==expected_hash.lower(),'DLL identity')
    require(h['native_taa_forwarded'] is False,'independent route')
    require(startup_review(startup)['accepted'],'startup')
    require(all(startup[0][k]==h[k] for k in ('pid','creation','base','version','host_sha256','dll_sha256')),'startup identity')
    require(rows[-1]['event']=='stopped' and rows[-1]['armed'] is True and rows[-1]['healthy'] is True,'healthy stop')
    armed=[r for r in rows if r['event']=='armed']
    require(len(armed)==1 and armed[0]['installed'] is True and not armed[0]['reason'],'installation')
    c=rows[-2]
    if h['version'] in ('0.8.1','0.8.2','0.8.3'):require(c['queue_rejection'] is None,'no queue rejection')
    require(c['event']=='continuous' and c['active'] is False and c['fault']==c['pending']==0,'final status')
    require(c['hooks_disabled'] and c['bindings_hooks_disabled'] and c['bindings_healthy'] and c['descriptors_detached'],'hook closure')
    require(c['native_aa_required']=='off' and c['native_aa_writes']==0,'native AA contract')
    require(c['frames']>0 and c['frames']==c['published']==c['consumed']==c['signal_value']==c['completed_value']==c['released'],'frame/submission/retirement accounting')
    require(c['jitter_injections']==c['jitter_restorations'] and c['jitter_injections']>=c['frames'],'jitter restoration')
    require(not c['pending_private_recording'] and c['awaiting_gpu']==c['awaiting_reset']==c['awaiting_submit']==0,'drained requests')
    require(0<=c['live_slots']<=32 and 0<c['peak_slots']<=32 and 0<c['list_count']<=128,'bounded tracking')
    p=c['pass'];sdk=p['sdk']
    require(not p['failure'] and p['native_state']==128 and p['native_handoffs']==c['frames'],'native output handoff')
    require(not sdk['failure'] and sdk['create_result']==sdk['evaluate_result']==1 and sdk['evaluations']==c['frames'],'real SDK evaluations')
    require(sdk['continuous'] and sdk['caller_retained'] and sdk['restored'] and sdk['copied'],'SDK streaming contract')
    history=c['history']
    require(history['evaluations']==c['frames']==history['resets']+history['retained'] and history['resets']==sdk['resets'],'history accounting')
    graphics=next(r for r in startup if r['event']=='graphics')
    require(all(c[k]==graphics[k] for k in ('device','backend','width','height')) and c['format']==graphics['swapchain_format'],'graphics ownership')
    layouts=review_layouts(c,h['base']) if h['version'] in ('0.8.2','0.8.3') else {}
    if h['version']=='0.8.3':review_boundaries(c)
    return dict(accepted=True,pid=h['pid'],creation=h['creation'],frames=c['frames'],history_resets=history['resets'],
                gpu_completed=c['completed_value'],markers_released=c['released'],hooks_disabled=True,
                native_aa_off=True,independent_native_handoff_verified=True,visual_quality_verified=False,
                other_aa_suppression_verified=False,private_sdk_retained_until_process_exit=True,**layouts)

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--log',type=Path,required=True);ap.add_argument('--dll-sha256',required=True);ap.add_argument('--output',type=Path,required=True)
    a=ap.parse_args();require(not a.output.exists(),'preserve review');raw=a.log.read_bytes();rows=parse(raw);h=rows[0]
    startup=a.log.parent/f"startup-{h['pid']}-{h['creation']}.jsonl"
    try:result=review(rows,[json.loads(s) for s in startup.read_text().splitlines()],a.dll_sha256)
    except (KeyError,ValueError,TypeError,IndexError) as e:result=dict(accepted=False,reason=str(e),independent_native_handoff_verified=False)
    result['log_sha256']=hashlib.sha256(raw).hexdigest()
    with a.output.open('x') as out:json.dump(result,out,indent=2)
    print(json.dumps(result,indent=2));return 0 if result['accepted'] else 1
if __name__=='__main__':raise SystemExit(main())
