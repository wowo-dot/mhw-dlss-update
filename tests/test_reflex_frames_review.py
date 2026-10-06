import copy,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).parents[1]/'tools'))
from reflex_frames import validate,GATES
from test_reflex_presentation_review import valid as fixture

def frame_fixture():
    rows=fixture()
    rows[0].update(schema=2,version='0.3.0',experiment='reflex-live-frames')
    ready=next(r for r in rows if r['event']=='persistent_ready')
    ready.update(event='frames_ready',armed=False,duration_seconds=30)
    where=rows.index(ready)+1
    add=[dict(event='frame_hooks_installed',engine=20,renderer=21,backend=22,qpc_frequency=10000000,sdk_mode=0,sample_frames=8,
              native_gates=[dict(kind=k,rva=r,bytes=n,sha256=h,target=0x140000000+r,original_head_sha256='b'*64) for k,(r,n,h) in GATES.items()]),
         dict(event='frames_armed',mode=0,duration_seconds=30,sleep_calls_enabled=True,markers_enabled=True)]
    for frame in range(1,9):
        for stage in range(6):
            epoch=frame-1 if stage==0 else frame
            add.append(dict(event='frame_stage',id=frame,stage=stage,thread=7,result=0,qpc=frame*10+stage,
                            tokens=frame,sleeps=frame,markers=(frame-1)*6+(1,3,4,5,6,6)[stage],stamp=[epoch,epoch,epoch&1,True]))
    add.extend(dict(event='frame_hook_restoration',kind=k,target=0x140000000+r,original_head_sha256='b'*64,restored=True)
               for k,(r,n,h) in GATES.items())
    add.append(dict(event='frame_finished',completed=8,tokens=8,sleeps=8,markers=48,faults=0,pending_calls=0,frame_active=False,
                    native_bytes_restored=True,reason='',owner=7))
    add.append(dict(event='driver_state',mode=0,low_latency_available=True,reports_available=False,complete_reports=0))
    rows[where:where]=add
    rows[-1].update(frame_hooks_restored=True,frame_faults=0,frame_calls=8,sleep_calls=8,markers=48)
    return rows

def reserved_fixture():
    rows=frame_fixture();rows[0]['version']='0.3.1'
    page=0x140000000+0x1af2000
    rows.insert(next(i for i,r in enumerate(rows) if r['event']=='prepared'),
                dict(event='frame_pool',ready=True,attempted=True,policy_available=True,policy_flags=0,
                     status=0,reason='',origin=0x140000000+GATES[0][0],page=page,slot=page+64))
    return rows

def on_fixture():
    rows=reserved_fixture();rows[0].update(version='0.4.0',target_mode=1)
    ready=next(r for r in rows if r['event']=='frames_ready');ready['target_mode']=1
    next(r for r in rows if r['event']=='frames_armed')['mode']=1
    next(r for r in rows if r['event']=='frame_hooks_installed')['sdk_mode']=1
    end=next(r for r in rows if r['event']=='frame_finished')
    end.update(completed=64,tokens=64,sleeps=64,markers=384)
    rows[-1].update(frame_calls=64,sleep_calls=64,markers=384,mode_restored_off=True)
    driver=next(r for r in rows if r['event']=='driver_state')
    driver.update(mode=1,mode_source='requested',reports_available=True,complete_reports=64)
    for frame in range(1,65):
        rows.insert(rows.index(driver),dict(event='driver_frame',frame=frame,simulation_start_us=frame*10,
            simulation_end_us=frame*10+1,submit_start_us=frame*10+2,submit_end_us=frame*10+3,
            present_start_us=frame*10+4,present_end_us=frame*10+5))
    device=next(r for r in rows if r['event']=='device_checked')['device']
    def status(phase,on):
        return dict(event='driver_sleep_status',phase=phase,device=device,result=0,low_latency=on,
                    query_id=0xaef96ca1,structure_version=136 | 1<<16)
    def option(phase,mode):
        return dict(event='mode_options',phase=phase,mode=mode,result=0,frame_limit_us=0,
                    use_markers_to_optimize=False,boost_requested=False)
    rows.insert(rows.index(ready),status('startup_off',False))
    at=rows.index(next(r for r in rows if r['event']=='frame_hooks_installed'))
    rows[at:at]=[option('activate_on',1),status('requested_on',False)]
    rows.insert(rows.index(next(r for r in rows if r['event']=='frame_hook_restoration')),status('running_on',True))
    rows.insert(rows.index(end)+1,status('stopped_on',True))
    at=rows.index(next(r for r in rows if r['event']=='sdk_shutdown'))
    rows[at:at]=[option('restore_off',0),status('restored_off',False)]
    return rows

def on_wait_fixture():
    rows=on_fixture();rows[0]['version']='0.4.1'
    next(r for r in rows if r.get('phase')=='restored_off').update(queries=2,elapsed_ms=10,timeout_ms=1000,initial_low_latency=True)
    return rows

def on_transition_fixture():
    rows=on_wait_fixture();rows[0]['version']='0.4.2'
    for r in rows:
        if r['event'] in ('frames_ready','frames_armed'):r['duration_seconds']=600
    return rows

class LiveFramesReviewTests(unittest.TestCase):
    def test_transition_capture_preserves_on_and_shutdown_requirements(self):
        value=validate(on_transition_fixture())
        self.assertTrue(value['driver_mode_confirmed'] and value['mode_restored_off'])
        for phase,key,bad in [('running_on','low_latency',False),('restored_off','queries',None),
                              ('restored_off','timeout_ms',60000)]:
            rows=on_transition_fixture();next(r for r in rows if r.get('phase')==phase)[key]=bad
            with self.subTest(phase=phase,key=key),self.assertRaises(ValueError):validate(rows)
    def test_transition_capture_rejects_mismatched_or_unbounded_duration(self):
        for ready,armed in ((30,30),(600,30),(30,600),(601,601),(None,None)):
            rows=on_transition_fixture()
            next(r for r in rows if r['event']=='frames_ready')['duration_seconds']=ready
            next(r for r in rows if r['event']=='frames_armed')['duration_seconds']=armed
            with self.subTest(ready=ready,armed=armed),self.assertRaises(ValueError):validate(rows)
    def test_bounded_off_confirmation_accepts_immediate_and_delayed(self):
        rows=on_wait_fixture();self.assertTrue(validate(rows)['mode_restored_off'])
        next(r for r in rows if r.get('phase')=='restored_off').update(queries=1,elapsed_ms=0,initial_low_latency=False)
        self.assertTrue(validate(rows)['mode_restored_off'])
    def test_off_wait_missing_unbounded_or_impossible_receipt_rejected(self):
        for key,value in [('queries',None),('queries',0),('queries',102),('queries',1),('elapsed_ms',1501),
                          ('elapsed_ms',-1),('timeout_ms',60000),('initial_low_latency',None)]:
            rows=on_wait_fixture();next(r for r in rows if r.get('phase')=='restored_off')[key]=value
            with self.subTest(key=key,value=value),self.assertRaises(ValueError):validate(rows)
    def test_on_requires_driver_confirmation_without_latency_claim(self):
        value=validate(on_fixture())
        self.assertTrue(value['game_reflex_enabled'] and value['driver_mode_confirmed'] and value['mode_restored_off'])
        self.assertFalse(value['latency_improvement_measured'] or value['game_reflex_active_after_stop'])
    def test_on_rejects_failed_mismatched_and_fabricated_driver_mode(self):
        for phase,key,value in [('startup_off','low_latency',True),('running_on','low_latency',False),
            ('stopped_on','low_latency',False),('restored_off','low_latency',True),('requested_on','result',-1),
            ('requested_on','device',987),('requested_on','query_id',0),('requested_on','structure_version',0)]:
            rows=on_fixture();next(r for r in rows if r.get('phase')==phase)[key]=value
            with self.subTest(phase=phase,key=key),self.assertRaises(ValueError):validate(rows)
    def test_on_rejects_missing_duplicate_or_early_restoration(self):
        for variant in ('missing','duplicate','early','unrestored'):
            rows=on_fixture();s=next(r for r in rows if r.get('phase')=='restored_off')
            if variant=='missing':rows.remove(s)
            elif variant=='duplicate':rows.insert(rows.index(s),dict(s))
            elif variant=='early':rows.remove(s);rows.insert(rows.index(next(r for r in rows if r['event']=='frame_finished')),s)
            else:rows[-1]['mode_restored_off']=False
            with self.subTest(variant=variant),self.assertRaises(ValueError):validate(rows)
    def test_on_rejects_unmatched_driver_tokens_or_changed_options(self):
        for variant in ('frame_id','reports','sdk_result','boost','frame_limit'):
            rows=on_fixture()
            if variant=='frame_id':next(r for r in rows if r['event']=='driver_frame')['frame']=100
            elif variant=='reports':next(r for r in rows if r['event']=='driver_state')['reports_available']=False
            else:
                option=next(r for r in rows if r.get('phase')=='activate_on')
                key,value={'sdk_result':('result',1),'boost':('boost_requested',True),'frame_limit':('frame_limit_us',1000)}[variant]
                option[key]=value
            with self.subTest(variant=variant),self.assertRaises(ValueError):validate(rows)
    def test_accepts_off_sdk_calls_without_latency_claim(self):
        value=validate(frame_fixture())
        self.assertEqual(value['restored_heads'],17)
        self.assertTrue(value['live_sdk_frame_calls'])
        self.assertFalse(value['game_reflex_enabled'])
        self.assertFalse(value['latency_improvement_measured'])
    def reject(self,event,key,value):
        rows=frame_fixture();next(r for r in rows if r['event']==event)[key]=value
        with self.assertRaises(ValueError):validate(rows)
    def test_partial_or_false_cleanup_rejected(self):
        for key,value in [('completed',0),('tokens',9),('sleeps',7),('markers',47),('faults',1),('pending_calls',1),('frame_active',True),('native_bytes_restored',False)]:
            with self.subTest(key=key):self.reject('frame_finished',key,value)
    def test_bad_sdk_order_and_result_rejected(self):
        for key,value in [('id',2),('stage',1),('thread',8),('result',1),('tokens',0),('sleeps',0),('markers',2),('stamp',[0,0,0,False])]:
            with self.subTest(key=key):self.reject('frame_stage',key,value)
    def test_different_stamp_on_present_rejected(self):
        rows=frame_fixture();sample=next(r for r in rows if r['event']=='frame_stage' and r['stage']==3);sample['stamp']=[2,2,0,True]
        with self.assertRaises(ValueError):validate(rows)
    def test_missing_duplicate_and_out_of_order_rejected(self):
        for variant in ('missing','duplicate','order'):
            rows=frame_fixture();at=next(i for i,r in enumerate(rows) if r['event']=='frame_stage')
            if variant=='missing':rows.pop(at)
            elif variant=='duplicate':rows.insert(at,copy.deepcopy(rows[at]))
            else:rows[at],rows[at+1]=rows[at+1],rows[at]
            with self.subTest(variant=variant),self.assertRaises(ValueError):validate(rows)
    def test_gate_and_restoration_identity_rejected(self):
        rows=frame_fixture();next(r for r in rows if r['event']=='frame_hooks_installed')['native_gates'][0]['sha256']='0'*64
        with self.assertRaises(ValueError):validate(rows)
        self.reject('frame_hook_restoration','original_head_sha256','0'*64)
    def test_driver_timestamp_order_rejected(self):
        rows=frame_fixture();d=next(r for r in rows if r['event']=='driver_state');d['complete_reports']=1
        rows.insert(rows.index(d),dict(event='driver_frame',frame=8,simulation_start_us=2,simulation_end_us=1,
            submit_start_us=3,submit_end_us=4,present_start_us=5,present_end_us=6))
        with self.assertRaises(ValueError):validate(rows)
    def test_mode_and_ceiling_rejected(self):
        self.reject('frames_armed','mode',1)
        self.reject('frames_ready','duration_seconds',600)
    def test_corrected_startup_allocation_receipt_accepted(self):
        self.assertTrue(validate(reserved_fixture())['live_frames_passed'])
    def test_missing_failed_unreachable_or_late_reservation_rejected(self):
        for variant in ('missing','failed','policy','unreachable','late'):
            rows=reserved_fixture();p=next(r for r in rows if r['event']=='frame_pool')
            if variant=='missing':rows.remove(p)
            elif variant=='failed':p['ready']=False
            elif variant=='policy':p['policy_flags']=1
            elif variant=='unreachable':p.update(page=0x10000,slot=0x10040)
            else:rows.remove(p);rows.insert(rows.index(next(r for r in rows if r['event']=='frames_ready')),p)
            with self.subTest(variant=variant),self.assertRaises(ValueError):validate(rows)

if __name__=='__main__':unittest.main()
