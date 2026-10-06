"""Verify the frozen reviewer against real owned native/COM capture receipts."""
import copy
import json
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from temporal_final_handoff import review

class HandoffTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.capture=json.loads(Path(sys.fixture).read_text())
        cls.header=dict(base=cls.capture['final_handoff']['target']-0x23917a0)

    def reject(self,change):
        c=copy.deepcopy(self.capture);change(c)
        with self.assertRaises((ValueError,KeyError)):review(self.header,c)

    def test_owned_capture(self):
        r=review(self.header,self.capture)
        self.assertTrue(r['native_final_copy_construction_verified'])
        self.assertFalse(r['final_copy_gpu_consumption_verified'])
        self.assertFalse(r['full_output_handoff_verified'])
        self.assertEqual(r['final_copy_rectangles'],[[0,0,8,6]]*3)

    def test_frame_association(self):
        for key in ('frame','epoch','bank','async','view','context','layer','thread','observed_thread','caller','index'):
            with self.subTest(key=key):self.reject(lambda c:c['final_handoff']['records'][0].__setitem__(key,c['final_handoff']['records'][0][key]+1))

    def test_source_and_destination(self):
        for key in ('owner','resource','identity','type','kind','engine_flags','engine_width','engine_height','engine_format','gpu_format','primary','secondary','backing','width','height','format','sample_count'):
            with self.subTest(source=key):self.reject(lambda c:c['final_handoff']['records'][0]['source'].__setitem__(key,c['final_handoff']['records'][0]['source'][key]+1))
        for key in ('width','height','engine_width','engine_height','engine_format','format','dimension','array_size','mips','sample_count','sample_quality'):
            with self.subTest(destination=key):self.reject(lambda c:c['final_handoff']['records'][0]['destination'].__setitem__(key,c['final_handoff']['records'][0]['destination'][key]+1))
        self.reject(lambda c:c['final_handoff']['records'][0]['destination'].__setitem__('identity',c['final_handoff']['records'][0]['source']['identity']))

    def test_copy_packet(self):
        for key in ('opcode','source','destination','mode','source_subresource','destination_subresource','optional'):
            with self.subTest(key=key):self.reject(lambda c:c['final_handoff']['records'][0]['packet'].__setitem__(key,c['final_handoff']['records'][0]['packet'][key]+1))
        self.reject(lambda c:c['final_handoff']['records'][0]['packet'].__setitem__('rectangle',[0,0,7,6]))
        self.reject(lambda c:c['final_handoff']['records'][0]['packet'].__setitem__('xy',[1,0]))
        self.reject(lambda c:c['final_handoff']['records'][0].__setitem__('count_after',0))

    def test_qualification(self):
        for key in ('valid','returned','owners_stable','stamp_stable','rectangle_stable','copy_branch'):
            with self.subTest(key=key):self.reject(lambda c:c['final_handoff']['records'][0].__setitem__(key,False))
        for key in ('fault','pending','published','completed','mutations','pins_acquired','pins_released','target','expected_caller'):
            with self.subTest(key=key):self.reject(lambda c:c['final_handoff'].__setitem__(key,c['final_handoff'][key]+1))
        for key in ('hooks_disabled','drained_before_disable'):
            self.reject(lambda c:c['final_handoff'].__setitem__(key,False))
        self.reject(lambda c:c['final_handoff'].__setitem__('gpu_consumption_verified',True))
        self.reject(lambda c:c['final_handoff']['records'].pop())

if __name__=='__main__':
    sys.fixture=sys.argv.pop(1)
    unittest.main()
