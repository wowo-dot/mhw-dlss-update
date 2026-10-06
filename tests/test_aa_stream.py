import copy
import json
import sys
import unittest
from pathlib import Path
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import arm_aa_stream as arm
import review_aa_stream as review

class StreamReview(unittest.TestCase):
    def setUp(self):
        self.h=dict(event='start',schema=1,version='0.8.0',host_sha256=arm.HOST,host_version='0.0.0.0',dll_sha256='a'*64,
                    experiment='independent-aa-continuous',graphics_api='D3D12',frame_limit=None,native_taa_forwarded=False,pid=1,creation=2,base=3)
        c=dict(event='continuous',active=False,fault=0,pending=0,hooks_disabled=True,bindings_hooks_disabled=True,bindings_healthy=True,descriptors_detached=True,
               native_aa_required='off',native_aa_writes=0,frames=600,published=600,consumed=600,signal_value=600,completed_value=600,released=600,
               jitter_injections=601,jitter_restorations=601,pending_private_recording=False,awaiting_gpu=0,awaiting_reset=0,awaiting_submit=0,
               live_slots=1,peak_slots=3,list_count=4,device=7,backend=8,width=2560,height=1440,format=87,
               history=dict(evaluations=600,resets=2,retained=598),
               **{'pass':dict(failure='',native_state=128,native_handoffs=600,sdk=dict(failure='',create_result=1,evaluate_result=1,evaluations=600,
                     continuous=True,caller_retained=True,restored=True,copied=True,resets=2))})
        self.rows=[self.h,dict(event='armed',installed=True,reason=''),c,dict(event='stopped',armed=True,healthy=True)]
        self.startup=[dict(self.h),dict(event='graphics',device=7,backend=8,width=2560,height=1440,swapchain_format=87)]
    def evaluate(self):
        with patch.object(review,'startup_review',return_value=dict(accepted=True)):
            return review.review(self.rows,self.startup,'a'*64)
    def test_accepted_scope(self):
        r=self.evaluate();self.assertTrue(r['accepted']);self.assertFalse(r['visual_quality_verified']);self.assertFalse(r['other_aa_suppression_verified'])
    def test_corrupted_closure(self):
        for key,value in dict(active=True,fault=1,pending=1,hooks_disabled=False,bindings_hooks_disabled=False,bindings_healthy=False,descriptors_detached=False,
                native_aa_required='taa',native_aa_writes=1,published=601,consumed=599,completed_value=599,released=599,jitter_restorations=600,
                pending_private_recording=True,awaiting_gpu=1,awaiting_reset=1,awaiting_submit=1,peak_slots=33,list_count=129,device=9).items():
            with self.subTest(key=key):
                original=self.rows[-2][key];self.rows[-2][key]=value
                with self.assertRaises(ValueError):self.evaluate()
                self.rows[-2][key]=original
    def test_sdk_failures(self):
        sdk=self.rows[-2]['pass']['sdk']
        for key,value in dict(failure='bad',create_result=0,evaluate_result=0,evaluations=599,continuous=False,caller_retained=False,restored=False,copied=False,resets=3).items():
            with self.subTest(key=key):
                original=sdk[key];sdk[key]=value
                with self.assertRaises(ValueError):self.evaluate()
                sdk[key]=original
    def test_wrong_startup_generation(self):
        self.startup[0]['creation']=8
        with self.assertRaises(ValueError):self.evaluate()
    def test_no_handoff(self):
        self.rows[-2]['pass']['native_handoffs']=0
        with self.assertRaises(ValueError):self.evaluate()
    def test_diagnostic_version(self):
        self.h['version']=self.startup[0]['version']='0.8.1'
        self.rows[-2]['queue_rejection']=None
        self.assertTrue(self.evaluate()['accepted'])
        self.rows[-2]['queue_rejection']=dict(schema=1)
        with self.assertRaises(ValueError):self.evaluate()
    def test_haze_version_base_only(self):
        self.h['version']=self.startup[0]['version']='0.8.2'
        self.assertEqual(arm.parse(json.dumps(self.h).encode())[0]['version'],'0.8.2')
        c=self.rows[-2];c['queue_rejection']=None
        c['scheduler_layouts']=dict(policy='base-or-haze-v1',base_admissions=600,base_returns=600,base_evaluations=600,
            haze_admissions=0,haze_returns=0,haze_evaluations=0,haze_entries=0,haze_exits=0,
            first_haze=None,first_base_after_haze=None,first_failed_return=None)
        self.assertTrue(self.evaluate()['accepted']);self.assertFalse(self.evaluate()['haze_layout_verified'])
        c['scheduler_layouts']['haze_returns']=1
        with self.assertRaises(ValueError):self.evaluate()
    def test_arm_identity(self):
        rows=arm.parse(json.dumps(self.h).encode()+b'\n')
        rows += [dict(event='reservation',ready=True,slot=1),dict(event='ready',hooks_installed=False)]
        arm.validate(rows,1,2,'a'*64)
        for field,value in [('version','0.7.0'),('experiment','continuous-dlaa-prototype')]:
            h=copy.deepcopy(self.h);h[field]=value
            with self.assertRaises(ValueError):arm.parse(json.dumps(h).encode())
        with self.assertRaises(ValueError):arm.validate(rows,1,3,'a'*64)
        rows.append(dict(event='armed'))
        with self.assertRaises(ValueError):arm.validate(rows,1,2,'a'*64)
        arm.validate(rows,1,2,'a'*64,True)
    def test_boundary_diagnostic_version(self):
        from test_aa_input_boundaries import fixture
        self.h['version']=self.startup[0]['version']='0.8.3'
        self.assertEqual(arm.parse(json.dumps(self.h).encode())[0]['version'],'0.8.3')
        c=self.rows[-2];c['queue_rejection']=None
        c['scheduler_layouts']=dict(policy='base-or-haze-v1',base_admissions=600,base_returns=600,base_evaluations=600,
            haze_admissions=0,haze_returns=0,haze_evaluations=0,haze_entries=0,haze_exits=0,
            first_haze=None,first_base_after_haze=None,first_failed_return=None)
        c['input_boundaries']=dict(policy='exact-color-motion-v1',first_base=fixture(),first_haze=None,first_rejection=None)
        self.assertTrue(self.evaluate()['accepted'])
        c['input_boundaries']['first_rejection']=fixture()
        with self.assertRaises(ValueError):self.evaluate()
if __name__=='__main__':unittest.main()
