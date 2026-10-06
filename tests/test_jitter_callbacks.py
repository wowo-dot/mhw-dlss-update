import copy
import json
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from review_jitter_callbacks import HOST_SHA, parse, review
from arm_jitter_callbacks import validate

def capture():
    base=0x140000000
    s=dict(failure=0,reads=31,renderer=10,camera=20,manager=30,view=40,layer=50,
        camera_type=base+0x33f82a8,layer_type=base+0x351bda8,frame=9,frame_after=9,
        epoch=4,epoch_after=4,camera_epoch=4,camera_jitter=0,setter_input=0,layer_mask=0,
        manager_mask=0,view_flags=0,reset_mode=0,layer_state=8,layer_flags=0x13,
        blend_bits=0x3f733333,taa_bytes=0x01010000,has_layer=True,has_input=False,stable=True)
    token=1<<32
    a=dict(event='call',token=token+1,parent=0,kind=0,thread=91,tick=100,exit_tick=101,
        entry_order=1,exit_order=4,argument=50,caller=1000,complete=True,before=copy.deepcopy(s),after=copy.deepcopy(s))
    b=copy.deepcopy(a); b.update(token=token+2,parent=token+1,kind=2,entry_order=2,exit_order=3,argument=20)
    b['before']['has_input']=b['after']['has_input']=True
    return [dict(event='start',schema=1,version='1.0.0',pid=123,creation=456,base=base,host_sha256=HOST_SHA,
                 dll_sha256='a'*64,experiment='native-callbacks-dlss-off',provider_substitution=False,
                 record_capacity=256,window_ms=1500),
            dict(event='ready',next_phase=1),dict(event='installed',live_spans=7,d3d12_module_loaded=True),
            dict(event='armed',phase=1,tick=100),a,b,
            dict(event='phase_complete',phase=1,records=2,reserved=2,pending=0,dropped=0,stop=2),
            dict(event='ready',next_phase=2)]

class CallbackReviewTests(unittest.TestCase):
    def test_nested_capture_and_still_ready(self):
        r=capture(); raw=('\n'.join(map(json.dumps,r))+'\n').encode()
        result=review(parse(raw)); p=result['phases'][0]
        self.assertTrue(p['accepted_cpu_capture']); self.assertEqual(p['nested_setters'],1)
        self.assertFalse(result['gpu_qualified']); validate(r,2,123,456)

    def test_wrong_generation_or_replayed_arm(self):
        r=capture()
        for args in [(2,123,457),(1,123,456),(3,123,456)]:
            with self.assertRaises(ValueError): validate(r,*args)

    def test_parent_overlap_and_thread_mismatch_rejected(self):
        for field,value in [('thread',92),('exit_order',5),('parent',(1<<32)+9)]:
            r=capture(); r[5][field]=value
            with self.assertRaises(ValueError): review(r)

    def test_incomplete_publication_rejected(self):
        for change in ['pending','missing','duplicate','incomplete','type']:
            r=capture()
            if change=='pending': r[6]['pending']=1
            if change=='missing': del r[5]
            if change=='duplicate': r[5]['token']=r[4]['token']
            if change=='incomplete': r[5]['complete']=False
            if change=='type': r[5]['before']['camera_type']=7
            with self.assertRaises(ValueError): review(r)

    def test_invalid_sample_retained_but_not_accepted(self):
        r=capture(); r[4]['before'].update(failure=5,stable=False); r[6]['stop']=4
        p=review(r)['phases'][0]
        self.assertFalse(p['accepted_cpu_capture']); self.assertEqual(p['invalid_snapshots'],1)

    def test_loss_does_not_claim_complete_coverage(self):
        r=capture(); r[6]['dropped']=1; r[6]['stop']=3
        p=review(r)['phases'][0]
        self.assertTrue(p['accepted_cpu_capture']); self.assertFalse(p['complete_event_coverage'])

    def test_partial_line_and_duplicate_keys_rejected(self):
        with self.assertRaises(ValueError): parse(b'{"event":"start"}')
        with self.assertRaises(ValueError): parse(b'{"event":"start","event":"start"}\n')

    def test_wrong_host_rejected(self):
        r=capture();r[0]['host_sha256']='b'*64
        with self.assertRaises(ValueError): parse(('\n'.join(map(json.dumps,r))+'\n').encode())

if __name__=='__main__': unittest.main()
