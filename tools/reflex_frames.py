"""Strict live frame/mode review and generation-checked file controls."""
from pathlib import Path
import argparse,ctypes as c
from ctypes import wintypes as w
import hashlib,json,re
from review_reflex_presentation import validate as presentation_validate,HOST_SHA,check

GATES={0:(0x1af2b60,4864,'9f6f5b3ac1a521ddd59351e41cc66c17dfc453ccd1b969afd4d59787bfef795d'),
       2:(0x229a080,1428,'961cf9969c28abc6b5b63ba9fb476978555eb188fdc173940d7ed81fc566a4e0'),
       11:(0x23a7f50,69,'1183e1807b99ec2ada753fa147af8c59b1f99f29bbbf6176ed7479fd48e09a46')}
VERSIONS=('0.3.0','0.3.1','0.4.0','0.4.1','0.4.2')
def parse(path):
    check(path.stat().st_size<=8*1024*1024,'bounded frame log')
    rows=[json.loads(line) for line in path.read_text().splitlines()]
    check(rows and rows[0].get('event')=='start','start record')
    start=rows[0]
    check(start.get('schema')==2 and start.get('version') in VERSIONS and start.get('experiment')=='reflex-live-frames','frame probe identity')
    check(start.get('host_sha256')==HOST_SHA and start.get('mode')==0 and start.get('upscaling') is False,'host/mode/scope')
    return rows

def validate(rows):
    def one(event):
        values=[r for r in rows if r.get('event')==event];check(len(values)==1,event);return values[0]
    start=one('start')
    check(start.get('schema')==2 and start.get('version') in VERSIONS and start.get('experiment')=='reflex-live-frames','frame version')
    mode=1 if start['version'] in ('0.4.0','0.4.1','0.4.2') else 0
    duration=600 if start['version']=='0.4.2' else 30
    ready=one('frames_ready');armed=one('frames_armed');installed=one('frame_hooks_installed');end=one('frame_finished')
    check(ready.get('duration_seconds')==armed.get('duration_seconds')==duration and ready.get('armed') is False and
          armed.get('sleep_calls_enabled') is True and armed.get('markers_enabled') is True and armed.get('mode')==mode,'arming contract')
    if mode:check(start.get('target_mode')==ready.get('target_mode')==1,'requested On experiment')
    # Reuse the independently tested 14-head native presentation lifecycle
    # checks. Normalize only the version/ready name for the common Off route;
    # real sleep/marker acceptance is checked separately below.
    common=[dict(r) for r in rows]
    common[rows.index(start)].update(schema=1,version='0.2.1',experiment='reflex-persistent-presentation')
    common[rows.index(ready)]=dict(event='persistent_ready',mode=0,sleep_calls=0,markers=0,ceiling_seconds=600)
    presentation=presentation_validate(common)
    check(all(installed.get(k,0)>0 for k in ('engine','renderer','backend','qpc_frequency')) and
          installed.get('sdk_mode')==mode and installed.get('sample_frames')==8,'native frame metadata')
    gates=installed.get('native_gates',[])
    check(len(gates)==3 and {g.get('kind') for g in gates}==set(GATES),'native frame gates')
    base=None
    for g in gates:
        expected=GATES[g['kind']]
        check((g.get('rva'),g.get('bytes'),g.get('sha256'))==expected and
              re.fullmatch('[0-9a-f]{64}',g.get('original_head_sha256','')),'qualified frame gate')
        candidate=g['target']-g['rva'];check(candidate>0 and (base is None or base==candidate),'image-relative targets');base=candidate
    if start['version'] in ('0.3.1','0.4.0','0.4.1','0.4.2'):
        pool=one('frame_pool')
        check(pool.get('ready') is True and pool.get('attempted') is True and pool.get('policy_available') is True and
              type(pool.get('policy_flags')) is int and not pool['policy_flags']&1 and pool.get('status')==0 and
              pool.get('reason')=='' and pool.get('origin')==base+GATES[0][0],'early allocation receipt')
        check(type(pool.get('slot')) is int and pool['slot']>0 and type(pool.get('page')) is int and
              pool['page']==pool['slot']&~4095 and rows.index(pool)<rows.index(one('prepared')),'pool before hook creation')
        check(all(base+g[0]-0x40000000<=pool['page']<base+g[0]+0x40000000-4095 for g in GATES.values()),'reachable allocation')
    check(end.get('faults')==end.get('pending_calls')==0 and end.get('frame_active') is False and
          end.get('native_bytes_restored') is True and end.get('reason')=='','frame completion')
    count=end.get('completed',0)
    check(type(count) is int and count>=8 and end.get('tokens')==end.get('sleeps')==count and end.get('markers')==count*6,'one token/sleep/six markers per frame')
    stages=[r for r in rows if r.get('event')=='frame_stage']
    check(len(stages)==48,'eight complete bounded samples')
    previous_qpc=0
    for n,r in enumerate(stages):
        frame=n//6+1;stage=n%6;markers=(frame-1)*6+(1,3,4,5,6,6)[stage]
        check(r.get('id')==frame and r.get('stage')==stage and r.get('thread')==end.get('owner') and r['thread']>0 and
              r.get('result')==0 and r.get('tokens')==r.get('sleeps')==frame and r.get('markers')==markers,'sampled SDK order')
        check(type(r.get('qpc')) is int and r['qpc']>=previous_qpc,'sampled timing order');previous_qpc=r['qpc']
        stamp=r.get('stamp',[]);check(len(stamp)==4 and stamp[3] is True and stamp[2] in (0,1),'sampled stamp')
        if stage:
            initial=stages[n-stage]['stamp']
            check(stamp[0]==initial[0]+1 and stamp[2]==initial[2]^1 and initial[1]<=stamp[1]<=initial[1]+1,'advanced frame stamp')
            if stage>1:check(stamp==stages[n-stage+1]['stamp'],'stable render/Present stamp')
    restored=[r for r in rows if r.get('event')=='frame_hook_restoration']
    check(len(restored)==3 and {r.get('kind') for r in restored}==set(GATES),'three restored native frame hooks')
    for r in restored:
        g=next(g for g in gates if g['kind']==r['kind'])
        check(r.get('restored') is True and r.get('target')==g['target'] and
              r.get('original_head_sha256')==g['original_head_sha256'],'native frame restoration')
    driver=one('driver_state')
    check(driver.get('mode')==mode and driver.get('low_latency_available') is True and type(driver.get('reports_available')) is bool,'Reflex driver state')
    reports=[r for r in rows if r.get('event')=='driver_frame'];check(len(reports)==driver.get('complete_reports') and len(reports)<=64,'bounded driver reports')
    for r in reports:
        times=[r.get(k) for k in ('simulation_start_us','simulation_end_us','submit_start_us','submit_end_us','present_start_us','present_end_us')]
        check(all(type(t) is int and t>0 for t in times) and times==sorted(times),'driver marker ordering')
    finish=one('finished')
    check(finish.get('frame_hooks_restored') is True and finish.get('frame_faults')==0 and
          finish.get('frame_calls')==finish.get('sleep_calls')==count and finish.get('markers')==count*6,'final frame receipt')
    order=(ready,installed,armed,end,driver,one('presentation_cleanup'),one('sdk_shutdown'),finish)
    check(all(rows.index(a)<rows.index(b) for a,b in zip(order,order[1:])),'frame shutdown order')
    check(all(rows.index(armed)<rows.index(r)<rows.index(end) for r in stages),'bounded sample lifecycle')
    if mode:
        check(driver.get('mode_source')=='requested' and driver.get('reports_available') is True and
              len(reports)==64 and count>=64,'On report availability')
        check([r.get('frame') for r in reports]==list(range(count-63,count+1)),'On driver/token frame alignment')
        check(all(rows.index(end)<rows.index(r)<rows.index(driver) for r in reports),'driver reports after frame drain')
        statuses=[r for r in rows if r.get('event')=='driver_sleep_status']
        phases=('startup_off','requested_on','running_on','stopped_on','restored_off')
        check(len(statuses)==5 and tuple(r.get('phase') for r in statuses)==phases,'driver mode readbacks')
        for r,expected in zip(statuses,(False,None,True,True,False)):
            check(r.get('result')==0 and type(r.get('low_latency')) is bool and (expected is None or r['low_latency'] is expected) and r.get('device')==presentation['native_device'] and
                  r.get('query_id')==0xaef96ca1 and r.get('structure_version')==(136 | 1<<16),'driver mode confirmation')
        options=[r for r in rows if r.get('event')=='mode_options']
        check(len(options)==2 and tuple(r.get('phase') for r in options)==('activate_on','restore_off'),'mode options lifecycle')
        for r,expected in zip(options,(1,0)):
            check(r.get('mode')==expected and r.get('result')==0 and r.get('frame_limit_us')==0 and
                  r.get('use_markers_to_optimize') is False and r.get('boost_requested') is False,'On options/defaults')
        lifecycle=(statuses[0],ready,options[0],statuses[1],installed,armed,statuses[2],end,statuses[3],driver,
                   one('presentation_cleanup'),options[1],statuses[4],one('sdk_shutdown'),finish)
        check(all(rows.index(a)<rows.index(b) for a,b in zip(lifecycle,lifecycle[1:])),'mode change after callback drain')
        check(finish.get('mode_restored_off') is True,'Off restored before SDK shutdown')
        if start['version'] in ('0.4.1','0.4.2'):
            off=statuses[-1]
            check(type(off.get('queries')) is int and 1<=off['queries']<=101 and
                  type(off.get('elapsed_ms')) is int and 0<=off['elapsed_ms']<=1500 and off.get('timeout_ms')==1000 and
                  type(off.get('initial_low_latency')) is bool,'bounded Off confirmation window')
            check(off['queries']>1 or off['initial_low_latency'] is False,'one query cannot observe an initial On then Off')
    return dict(schema=2,live_frames_passed=True,presentation_passed=True,pid=start['pid'],creation=start['creation'],
                probe_sha256=start['dll_sha256'],mode=mode,completed=count,tokens=count,sleeps=count,markers=count*6,
                faults=0,restored_heads=17,native_frame_functions=3,driver_complete_reports=len(reports),
                game_reflex_enabled=bool(mode),driver_mode_confirmed=bool(mode),mode_restored_off=bool(mode),
                game_reflex_active_after_stop=False,live_sdk_frame_calls=True,latency_improvement_measured=False,
                native_device=presentation['native_device'],native_factory=presentation['native_factory'])

def control(rows,path,action):
    check(not any(r.get('event') in ('failed','worker_failed','finished','frame_finished') for r in rows),'active recorder')
    check(any(r.get('event')=='frames_ready' for r in rows),'SDK/presentation ready')
    check(action=='stop' or not any(r.get('event')=='frames_armed' for r in rows),'not already armed')
    start=rows[0];k=c.WinDLL('kernel32',use_last_error=True)
    k.OpenProcess.argtypes=[w.DWORD,w.BOOL,w.DWORD];k.OpenProcess.restype=w.HANDLE
    k.GetProcessTimes.argtypes=[w.HANDLE]+[c.POINTER(w.FILETIME)]*4
    k.QueryFullProcessImageNameW.argtypes=[w.HANDLE,w.DWORD,w.LPWSTR,c.POINTER(w.DWORD)]
    k.CloseHandle.argtypes=[w.HANDLE];h=k.OpenProcess(0x1000,False,start['pid']);check(h,'process query')
    try:
        times=[w.FILETIME() for _ in range(4)];check(k.GetProcessTimes(h,*map(c.byref,times)),'process identity')
        check((times[0].dwHighDateTime<<32|times[0].dwLowDateTime)==start['creation'] and
              not(times[1].dwHighDateTime or times[1].dwLowDateTime),'live process generation')
        text=c.create_unicode_buffer(32768);size=w.DWORD(len(text));check(k.QueryFullProcessImageNameW(h,0,text,c.byref(size)),'image path')
        check(hashlib.sha256(Path(text.value).read_bytes()).hexdigest()==HOST_SHA,'retail image identity')
    finally:k.CloseHandle(h)
    target=Path(str(path)+'.'+action)
    with target.open('x',encoding='ascii') as f:f.write(str(start['creation'])+'\n')
    return dict(action=action,pid=start['pid'],creation=start['creation'],request_sent=True,acknowledged=False,
                process_access='PROCESS_QUERY_LIMITED_INFORMATION',process_memory_writes=0)

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('action',choices=('arm','stop','review'))
    parser.add_argument('--log',required=True,type=Path);parser.add_argument('--output',required=True,type=Path)
    args=parser.parse_args();check(not args.output.exists(),'preserve receipt');rows=parse(args.log)
    result=validate(rows) if args.action=='review' else control(rows,args.log,args.action)
    result['log_sha256']=hashlib.sha256(args.log.read_bytes()).hexdigest()
    with args.output.open('x',encoding='utf-8') as f:json.dump(result,f,indent=2)
    print(json.dumps(result,indent=2))
if __name__=='__main__':main()
