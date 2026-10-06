import copy
import json
import unittest
import test_jitter_sequence_review as old
from review_submission import ReviewError
import review_jitter_sequence as seq

class JitterSceneReviewTests(unittest.TestCase):
    def setUp(self):
        self.rows,self.state=old.fixture()
        for row in self.rows:row.update(schema=2,producer=1500)
        self.state=dict(schema=2,producer=1500,count=32,initial=16,scene=16,started=1000,armed=2600,stop=1)
    def reject(self,pattern):
        with self.assertRaisesRegex(ReviewError,pattern):seq.review_rows(self.rows,self.state)
    def test_scene_with_both_wrapper_states(self):
        for row in self.rows[16:24]:row['gates']['wrapper']=777
        r=seq.review_rows(self.rows,self.state)
        self.assertEqual(r['phases'][1]['wrapper_present'],8)
        self.assertEqual(r['phases'][1]['wrapper_absent'],8)
    def test_no_implicit_scene_arm(self):
        self.state['armed']=0;self.reject('trigger timing')
    def test_sample_before_arm_rejected(self):
        self.state['armed']=2700;self.reject('trigger timing')
    def test_incomplete_initial_burst_cannot_arm(self):
        self.rows=self.rows[:15];self.state.update(count=15,initial=15,scene=0,stop=0);self.reject('arm state')
    def test_waiting_for_trigger_is_valid_partial_trace(self):
        self.rows=self.rows[:16];self.state.update(count=16,scene=0,armed=0,stop=0)
        self.assertFalse(seq.review_rows(self.rows,self.state)['stopped'])
    def test_armed_but_no_scene_hook_keeps_missing_coverage(self):
        self.rows=self.rows[:16];self.state.update(count=16,scene=0,stop=3)
        self.assertEqual(seq.review_rows(self.rows,self.state)['phases'][1]['samples'],0)
    def test_wrong_schema_rejected(self):
        self.rows[17]['schema']=1;self.reject('sample identity')
    def test_versioned_trace(self):
        text='trace.start version=0.15.0 owned-fixture=true\n'
        text+='\n'.join('bridge.jitter-sample '+json.dumps(r) for r in self.rows)+'\n'
        text+='bridge.jitter-sequence-state '+json.dumps(self.state)
        self.assertEqual(seq.parse_trace(text)['producer'],1500)
        with self.assertRaisesRegex(ReviewError,'producer mismatch'):seq.parse_trace(text.replace('0.15.0','0.14.0'))

if __name__=='__main__':unittest.main()
