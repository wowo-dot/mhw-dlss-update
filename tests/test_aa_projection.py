import copy
import struct
import unittest
import test_aa_inputs as inputs
from test_aa_owned import raw
from arm_aa_boundary import parse
from review_aa_projection import review, review_projection, review_bindings, review_shutdown


def bits(values):return list(struct.unpack('<'+'I'*len(values),struct.pack('<'+'f'*len(values),*values)))


def fixture(images):
    rows=inputs.fixture(images)
    rows[0].update(version='0.6.0',experiment='independent-aa-projection-bindings',jitter_writes=12,
                   projection_receipts=8,jitter_stimuli=6,binding_snapshots=8)
    rows[2]['backend']=505;rows[3]['hooks']=25
    base=rows[0]['base'];g=rows[13];records=[];bindings=[]
    identity=bits([1 if i%5==0 else 0 for i in range(16)])
    for i,(sample,gpu) in enumerate(zip(rows[4:12],g['records'])):
        q=sample['before'];owned=sample['owned'];q['context']=q['renderer']+0x710
        for state in (sample['after'],owned['callback'],owned['after'],owned['restored']):state['context']=q['context']
        sample.update(tick=100+10*i+2,projection_async=0)
        pair=bits([.375+i/128,-.375-i/128]);jitter=pair if 0<i<7 else [0,0]
        jx,jy=struct.unpack('<ff',struct.pack('<II',*jitter))
        projection=identity.copy();projection[12]=bits([jx/2560])[0];projection[13]=bits([jy/1440])[0]
        record=dict(stage=i,valid=True,returned=True,acknowledged=True,restored=True,projection_valid=True,matrix_relation=True,
                    provider_stable=True,matrices_valid=True,injected=0<i<7,restored_bits=0,caller=base+0x228e834,
                    camera_type=base+0x350d188,camera=22,table=24,constants=26,thread=28,provider_use=0,provider_sizing=0,
                    backend=505,device=500,wrapper=0,enter_tick=100+10*i,exit_tick=100+10*i+1,
                    candidate=pair[0]+(pair[1]<<32),table20_bits=pair,jitter_bits=jitter,viewport_bits=bits([2560,1440]),
                    buffer_id=350,words=80,matrix_ids=[(16*k<<12)|0xf55 for k in range(3)],projection_id=(48<<12)|0xf55,
                    unjittered_id=(64<<12)|0xf55,view_bits=identity,projection_bits=projection,unjittered_bits=identity,
                    view_projection_bits=projection,unjittered_view_projection_bits=identity,async_mode=0)
        record.update({key:q[key] for key in ('frame','epoch','bank','context','view','renderer')});record['async']=0
        records.append(record)
        gpu.update(binding_seen=True,binding_ready=True,binding_unchanged=True,binding_known=True,binding_valid=True,
                   binding_generation=gpu['reset'],binding_pipeline=700,binding_compute_root=701,projection_matched=True,
                   projection_frame=q['frame'],projection_epoch=q['epoch'],jitter_bits=jitter)
        bindings.append(dict(receipt=i,frame=q['frame'],command=gpu['command'],generation=gpu['reset'],ready=True,
                             pipeline=700,expected_pipeline=700,expected_root=701,heaps=[],
                             compute=dict(signature=701,arguments=[]),graphics=dict(signature=702,arguments=[])))
    g['bindings']=dict(schema=1,fault=0,pending=0,hooks_disabled=True,drained=True,hook_count=18,list_count=1,records=bindings)
    rows.insert(-1,dict(event='projection',jitter=dict(fault=0,pending=0,completed=8,matched=8,injections=6,restorations=6,
                     hooks_disabled=True,selection_attempts=2,selection_skipped=1,selected_frame=records[1]['frame'],
                     selected_candidate=records[1]['candidate'],records=records)))
    return rows


def shutdown_fixture(images):
    rows=fixture(images);rows[0].update(version='0.6.1',shutdown_order='drain-before-disable-v1')
    g=rows[13];s=g['submission'];s.update(started_tick=100,stopped_tick=1001)
    for e in s['events']:e.update(enter_tick=100+e['enter'],exit_tick=100+e['exit'])
    for r in s['samples']:r['completed_tick']=100+r['completed_order']
    g.update(collection_closed=True,completed_before_close=True,collection_close_requested=1000,
             collection_close_tick=1002,journal_events_at_close=len(s['events']),first_disable_tick=1200)
    rows[-1]['first_disable_tick']=1100
    return rows


class Cases(unittest.TestCase):
    @classmethod
    def setUpClass(cls):inputs.Cases.setUpClass()
    @classmethod
    def tearDownClass(cls):inputs.Cases.tearDownClass()

    def test_complete_without_overclaim(self):
        result=review(raw(fixture(inputs.Cases.images)),'a'*64,inputs.Cases.root)
        self.assertTrue(result['projection_gpu_association_verified'])
        self.assertEqual(result['native_binding_snapshots_verified'],8)
        self.assertEqual(result['readback_images_verified'],9)
        self.assertFalse(result['color_jitter_verified'])
        self.assertFalse(result['independent_dlaa_verified'])
        self.assertFalse(result['native_binding_restoration_tested'])

    def test_version_and_scope(self):
        rows=fixture([])[:2];parse(raw(rows),projection=True)
        with self.assertRaises(ValueError):parse(raw(rows),pixels=True)
        for key,value in (('jitter_writes',0),('jitter_stimuli',8),('binding_snapshots',9),('sdk_used',True),('aa_option_writes',1)):
            bad=copy.deepcopy(rows);bad[0][key]=value
            with self.assertRaises(ValueError,msg=key):parse(raw(bad),projection=True)

    def test_shutdown_completed_before_unhook(self):
        result=review(raw(shutdown_fixture(inputs.Cases.images)),'a'*64,inputs.Cases.root)
        self.assertTrue(result['completion_before_hook_removal_verified'])
        rows=shutdown_fixture([])[:2];rows[0]['shutdown_order']='disable-first'
        with self.assertRaises(ValueError):parse(raw(rows),projection=True)

    def test_shutdown_cannot_hide_incomplete_or_late_events(self):
        for change in ('incomplete','unclosed','early_disable','late_retire','late_completion','extra_event','late_enter','late_exit','early_gpu_disable'):
            rows=shutdown_fixture([]);g=rows[13];s=g['submission'];disable=rows[-1]['first_disable_tick']
            if change=='incomplete':g['completed_before_close']=False
            elif change=='unclosed':g['collection_closed']=False
            elif change=='early_disable':disable=999
            elif change=='late_retire':
                next(e for e in s['events'] if e['enter']==s['samples'][-1]['retire'])['exit_tick']=1001
            elif change=='late_completion':s['samples'][-1]['completed_tick']=1001
            elif change=='extra_event':g['journal_events_at_close']-=1
            elif change=='late_enter':s['events'][-1]['enter_tick']=1002
            elif change=='late_exit':s['events'][-1]['exit_tick']=1003
            elif change=='early_gpu_disable':g['first_disable_tick']=999
            with self.assertRaises(ValueError,msg=change):review_shutdown(g,disable)

    def test_projection_identity_restoration_and_matrix(self):
        for key,value in (('frame',999),('epoch',999),('bank',9),('context',2),('view',999),('async',1),('restored_bits',1),
                          ('restored',False),('acknowledged',False),('injected',False),('exit_tick',999),('device',999),('wrapper',1),('candidate',1)):
            rows=fixture([]);rows[15]['jitter']['records'][3][key]=value
            with self.assertRaises(ValueError,msg=key):review_projection(rows)
        for field in ('projection_bits','view_projection_bits'):
            rows=fixture([]);rows[15]['jitter']['records'][3][field]=bits([0]*16)
            with self.assertRaises(ValueError,msg=field):review_projection(rows)
        for key,value in (('projection_frame',999),('projection_epoch',999),('projection_matched',False),('jitter_bits',[0,0])):
            rows=fixture([]);rows[13]['records'][3][key]=value
            with self.assertRaises(ValueError,msg=key):review_projection(rows)

    def test_binding_generation_and_arguments(self):
        for key,value in (('binding_ready',False),('binding_unchanged',False),('binding_generation',999),('binding_compute_root',999)):
            g=fixture([])[13];g['records'][3][key]=value
            with self.assertRaises(ValueError,msg=key):review_bindings(g)
        for key,value in (('frame',999),('command',999),('generation',999),('ready',False),('pipeline',999)):
            g=fixture([])[13];g['bindings']['records'][3][key]=value
            with self.assertRaises(ValueError,msg=key):review_bindings(g)
        g=fixture([])[13];g['bindings']['records'][3]['compute']['arguments']=[dict(index=0,kind=1,address=900,mask=0,constants=[])]
        with self.assertRaises(ValueError):review_bindings(g)


if __name__ == '__main__':unittest.main()
