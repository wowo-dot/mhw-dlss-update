import copy
import sys
import unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from reflex_boundaries import review


class BoundaryReviewTests(unittest.TestCase):
    def sample(self):
        events = []
        for k in range(7):
            events.append(dict(event='boundary', kind=k, thread=10+k%2,
                               enter=2*k+1, exit=2*k+2, qpc_enter=100+2*k,
                               qpc_exit=101+2*k, exception=0, before=[11,7,1,True],
                               after=[11,7,1,True], args=[0,0,0], stack=[]))
        return [dict(event='armed', duration_ms=15000, capacity=262144, targets=list(range(100,107))),
                *events, dict(event='stopped', drops=0, exceptions=0, pending=0,
                              native_bytes_restored=True, records=7, visits=[1]*7),
                dict(event='worker_finished', installed=True, cleanup_passed=True)]

    def test_complete_not_frame_alignment(self):
        r = review(self.sample())
        self.assertTrue(r['lifecycle_passed'])
        self.assertFalse(r['frame_alignment_proven'])
        self.assertFalse(r['game_reflex_implemented'])

    def test_test_present_not_real_frame(self):
        s = self.sample(); r = copy.deepcopy(s[-3])
        r.update(enter=15, exit=16, args=[0,1,0]); s.insert(-2,r)
        s[-2]['records']=8; s[-2]['visits'][6]=2
        result=review(s)
        self.assertEqual((result['present_tests'],result['actual_presents']),(1,1))
        s[-4]['args'][1]=1
        with self.assertRaisesRegex(ValueError,'no real Present'):
            review(s)

    def test_unrestored_drop_pending_or_exception_rejected(self):
        for key, value in [('native_bytes_restored',False),('drops',1),('pending',1),('exceptions',1)]:
            s=self.sample(); s[-2][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError): review(s)

    def test_duplicate_order_rejected(self):
        s=self.sample(); s[2]['enter']=s[1]['enter']
        with self.assertRaisesRegex(ValueError,'duplicate'): review(s)

    def test_overlapping_threads_are_valid(self):
        s=self.sample(); s[1]['exit']=50
        # Callback ordering differs from completion ordering in an async engine.
        self.assertTrue(review(s)['lifecycle_passed'])

    def test_unstable_counter_does_not_fake_alignment(self):
        s=self.sample(); s[1]['before'][3]=False
        r=review(s)
        self.assertEqual(r['boundaries'][0]['unstable_stamps'],1)
        self.assertFalse(r['frame_alignment_proven'])

    def test_missing_boundary_rejected(self):
        s=self.sample(); del s[1]; s[-2]['records']=6; s[-2]['visits'][0]=0
        with self.assertRaisesRegex(ValueError,'boundary'): review(s)


class InputBoundaryReviewTests(unittest.TestCase):
    def sample(self):
        s=BoundaryReviewTests().sample()
        for k in range(7,13):
            s.insert(-2,dict(event='boundary',kind=k,thread=17,enter=2*k+1,exit=2*k+2,
                            qpc_enter=100+2*k,qpc_exit=101+2*k,exception=0,
                            before=[11,7,1,True],after=[11,7,1,True],args=[0,0,0],stack=[]))
        for r in s:
            if r['event']=='boundary':
                r.update(context_before=[48,1,True],context_after=[48,0,True],qualified=True,returned=0)
        s.insert(0,dict(event='start',schema=2))
        s[1].update(targets=list(range(100,113)),input_values_recorded=False,
                    engine=1,keyboard_object=2,mouse_object=3,devices_object=4,
                    original_head_sha256=['a'*64]*13,
                    native_gates=[dict(kind=k) for k in range(13) if k not in (5,6)])
        s[-2].update(records=13,visits=[1]*13)
        s.insert(-2,dict(event='worker_plan',order=5,valid=True,jobs=[[9,10,0]]))
        return s

    def test_capture_does_not_claim_semantics(self):
        r=review(self.sample())
        self.assertEqual(len(r['boundaries']),13)
        self.assertEqual(r['worker_plan_samples'],1)
        self.assertFalse(r['frame_alignment_proven'])
        self.assertFalse(r['game_reflex_implemented'])

    def test_input_values_rejected(self):
        for key,value in [('args',[1,0,0]),('returned',1)]:
            s=self.sample();r=next(r for r in s if r.get('kind')==7);r[key]=value
            with self.subTest(key=key),self.assertRaisesRegex(ValueError,'redaction'):review(s)

    def test_invalid_context_and_worker_metadata_rejected(self):
        s=self.sample();s[2]['context_before'][1]=16
        with self.assertRaisesRegex(ValueError,'context'):review(s)
        s=self.sample();next(r for r in s if r['event']=='worker_plan')['order']=1
        with self.assertRaisesRegex(ValueError,'plan layout'):review(s)

    def test_missing_native_gate_and_head_rejected(self):
        s=self.sample();s[1]['native_gates'].pop()
        with self.assertRaisesRegex(ValueError,'gate'):review(s)
        s=self.sample();s[1]['original_head_sha256'].pop()
        with self.assertRaisesRegex(ValueError,'head'):review(s)

    def test_worker_overlap_is_observation(self):
        s=self.sample();r=next(r for r in s if r.get('kind')==10)
        r.update(thread=99,exit=999,qualified=False,context_before=[48,15,True])
        result=review(s)
        self.assertTrue(result['lifecycle_passed'])
        self.assertEqual(result['boundaries'][10]['qualified_calls'],0)
        self.assertFalse(result['frame_alignment_proven'])


if __name__ == '__main__':
    unittest.main()
