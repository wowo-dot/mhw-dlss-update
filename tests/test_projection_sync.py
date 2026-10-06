"""Schema-3 fence replay, including schema-2 rejection cases on fresh traces."""
import copy
import json
import sys
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import test_projection_submission as base
import projection_provenance as pv
import projection_sync as sync
import review_projection_binding as review
import arm_projection_binding as control


class Synchronization(base.Submission):
    def data(self,case='submit_batch'):
        p,t,r,m=super().data(case)
        for key in ('completion_signal','completion_read'):t[key]*=100
        for fence in t['fences']:fence['first']*=100
        return p,t,r,m

    def combined(self):
        rows,blobs,t=super().combined()
        rows[0].update(version='0.6.0',experiment='projection-sync-readonly')
        for fence in t['fences']:fence['device']=review.one(rows,'draw')['device']
        return rows,blobs,t

    def signal(self,t):return next(e for e in t['history'] if e['enter']==t['completion_signal'])
    def completed(self,t):return next(e for e in t['history'] if e['enter']==t['completion_read'])
    def no_completion(self,change=None,case='submit_batch'):
        result=self.result(change,case)
        self.assertTrue(result['selected_recording_submitted'])
        self.assertFalse(result['selected_queue_fence_completion_observed'])
        self.assertTrue(result['synchronization_reasons'])

    def test_completion_64_bit_value(self):
        result=self.result()
        self.assertTrue(result['selected_queue_fence_completion_observed'])
        self.assertEqual(result['completion_signal_value'],0x100000001)
        self.assertTrue(result['same_queue_depth_state_declared'])
        self.assertFalse(result['gpu_completion_verified'])
        self.assertFalse(result['cross_queue_resource_safety_verified'])

    def test_two_queue_handshake(self):
        r=self.result(case='sync_chain')
        self.assertTrue(r['same_queue_depth_state_declared'] and r['selected_queue_fence_completion_observed'])
        self.assertFalse(r['submitted_depth_state_declared'])
        self.assertEqual([v['value'] for v in r['recorded_exact_value_fence_pairs']],[1,2])
        self.assertEqual(r['unmatched_queue_waits'],[])
        self.assertEqual(len(r['same_queue_generations']),2)

    def test_native_cpu_writer(self):self.no_completion(case='sync_cpu_signal')
    def test_native_shared_fence(self):self.no_completion(case='sync_shared')
    def test_native_incomplete(self):self.no_completion(case='sync_incomplete')
    def test_native_reuse(self):
        r=self.result(case='sync_reuse');self.assertTrue(r['selected_recording_submitted'] and r['selected_queue_fence_completion_observed'])
    def test_native_resubmit(self):
        r=self.result(case='sync_resubmit');self.assertFalse(r['selected_recording_submitted']);self.assertFalse(r['selected_queue_fence_completion_observed'])
    def test_failed_signal(self):self.no_completion(lambda p,t,r,m:self.signal(t).update(result=0x80004005))
    def test_no_baseline(self):
        def change(p,t,r,m):
            for e in t['history']:
                if e['kind']==13 and e['enter']<t['completion_signal']:e['handle']=0x100000001
        self.no_completion(change)
    def test_below_threshold(self):
        self.no_completion(lambda p,t,r,m:self.completed(t).update(handle=0x100000000))
    def test_unknown_read_selector(self):self.reject(lambda p,t,r,m:t.update(completion_read=77))
    def test_unknown_signal_selector(self):self.reject(lambda p,t,r,m:t.update(completion_signal=77))
    def test_device_removed(self):self.reject(lambda p,t,r,m:self.completed(t).update(handle=2**64-1))
    def test_fence_pin(self):self.reject(lambda p,t,r,m:t['fences'][0].update(pin=1))
    def test_fence_foreign_device(self):self.reject(lambda p,t,r,m:t['fences'][0].update(device=7))
    def test_fence_canonical_identity(self):self.reject(lambda p,t,r,m:self.signal(t).update(backing=7))
    def test_fence_capacity(self):self.reject(lambda p,t,r,m:p.update(fence_capacity=33))
    def test_fence_event_payload(self):self.reject(lambda p,t,r,m:self.signal(t).update(count=1))
    def test_fence_read_union(self):self.reject(lambda p,t,r,m:self.completed(t).update(resource=1))
    def test_overlapping_fence_read(self):
        def change(p,t,r,m):
            e=copy.deepcopy(self.completed(t));e.update(enter=e['enter']+1,exit=e['exit']+1);self.insert(t,e)
        self.no_completion(change)
    def test_rewound_fence_read(self):
        def change(p,t,r,m):
            e=copy.deepcopy(self.completed(t));e.update(enter=e['exit']+1,exit=e['exit']+2,handle=0);self.insert(t,e)
        self.no_completion(change)
    def test_duplicate_signal_value(self):
        def change(p,t,r,m):
            e=copy.deepcopy(self.signal(t));e.update(enter=e['exit']+1,exit=e['exit']+2);self.insert(t,e)
        self.no_completion(change)
    def test_failed_wait(self):
        def change(p,t,r,m):
            e=copy.deepcopy(self.signal(t));e.update(kind=12,enter=e['enter']-2,exit=e['enter']-1,result=0x80004005);self.insert(t,e)
        self.no_completion(change)
    def test_queue_overlap_at_signal(self):
        def change(p,t,r,m):
            e=copy.deepcopy(self.signal(t));e.update(kind=12,enter=e['enter']-1,exit=e['exit']+1);self.insert(t,e)
        self.no_completion(change)
    def test_competing_queue_writer(self):
        def change(p,t,r,m):
            e=copy.deepcopy(self.signal(t));e.update(enter=e['exit']+1,exit=e['exit']+2,object=e['object']+8,identity=e['identity']+8,handle=e['handle']+1)
            q=copy.deepcopy(t['queues'][0]);q.update(object=e['object'],identity=e['identity'],first=e['enter']);t['queues'].append(q);self.insert(t,e)
        self.no_completion(change)
    def test_missing_wait_match_not_invented(self):
        def change(p,t,r,m):next(e for e in t['history'] if e['kind']==12)['handle']=99
        result=self.result(change,'sync_chain')
        self.assertEqual(len(result['recorded_exact_value_fence_pairs']),1)
        self.assertEqual(len(result['unmatched_queue_waits']),1)
        self.assertFalse(result['cross_queue_resource_safety_verified'])
    def test_nondecaying_texture_contract(self):
        r=self.result(lambda p,t,r,m:r.update(flags=34))
        self.assertFalse(r['same_queue_depth_state_declared'])
    def test_cpu_write_between_signal_and_read(self):
        def change(p,t,r,m):
            s=self.signal(t);e=copy.deepcopy(self.completed(t))
            e.update(kind=14,enter=s['exit']+1,exit=s['exit']+2,result=0,handle=s['handle']);self.insert(t,e)
        self.no_completion(change)
    def test_newer_recording_after_submission(self):
        def change(p,t,r,m):
            sub=self.selected(t);after=sub['exit']+1
            reset=copy.deepcopy(next(e for e in t['history'] if e['kind']==8 and e['object']==m['command']))
            close=copy.deepcopy(next(e for e in t['history'] if e['kind']==5 and e['object']==m['command']))
            execute=copy.deepcopy(sub)
            reset.update(enter=after,exit=after+1);close.update(enter=after+2,exit=after+3)
            execute.update(enter=after+4,exit=after+5,first=len(t['submitted']),count=1)
            t['submitted'].append(m['command']);t['submitted_reserved']+=1
            for e in (reset,close,execute):self.insert(t,e)
        r=self.result(change)
        self.assertTrue(r['selected_recording_submitted'] and r['selected_queue_fence_completion_observed'])
    def test_combined_completion(self):
        r=review.review(*self.combined())
        self.assertTrue(r['selected_queue_fence_completion_observed'] and r['same_queue_depth_state_declared'])
        self.assertFalse(r['gpu_completion_verified'])

    def test_following_submission_nested_wait(self):
        def change(p,t,r,m):
            sub=self.selected(t);after=sub['exit']+1
            reset=copy.deepcopy(next(e for e in t['history'] if e['kind']==8 and e['object']==m['command']))
            close=copy.deepcopy(next(e for e in t['history'] if e['kind']==5 and e['object']==m['command']))
            execute=copy.deepcopy(sub);wait=copy.deepcopy(self.signal(t))
            reset.update(enter=after,exit=after+1);close.update(enter=after+2,exit=after+3)
            execute.update(enter=after+4,exit=after+8,first=len(t['submitted']),count=1)
            wait.update(kind=12,enter=after+5,exit=after+6,thread=execute['thread'])
            t['submitted'].append(m['command']);t['submitted_reserved']+=1
            for e in (reset,close,execute,wait):self.insert(t,e)
        r=self.result(change)
        self.assertTrue(r['selected_recording_submitted'] and r['selected_queue_fence_completion_observed'])
        self.assertEqual(len(r['completion_later_execute_nested_waits']),1)
        self.assertFalse(r['gpu_completion_verified'] or r['cross_queue_resource_safety_verified'])


class QueueIntervals(unittest.TestCase):
    def calls(self):
        def call(kind,enter,exit):
            return dict(kind=kind,enter=enter,exit=exit,thread=7,object=8,result=0)
        return [call(10,10,20),call(10,30,60),call(12,40,50),call(11,70,80)]

    def check(self,change=None):
        calls=self.calls()
        if change:change(calls)
        return sync.completion_queue_intervals(calls,calls[0],calls[-1])

    def rejects(self,change):
        with self.assertRaisesRegex(ValueError,'failed/overlapping queue operation'):self.check(change)

    def test_later_nested_wait(self):
        self.assertEqual(self.check(),[dict(execute=30,wait=40,thread=7)])
    def test_cross_thread_overlap(self):self.rejects(lambda c:c[2].update(thread=9))
    def test_crossing_intervals(self):self.rejects(lambda c:c[2].update(exit=65))
    def test_nested_signal(self):self.rejects(lambda c:c[2].update(kind=11))
    def test_nested_execute(self):self.rejects(lambda c:c[2].update(kind=10))
    def test_failed_nested_wait(self):self.rejects(lambda c:c[2].update(result=0x80004005))
    def test_selected_execute_overlap(self):self.rejects(lambda c:c[2].update(enter=11,exit=19))
    def test_signal_overlap(self):self.rejects(lambda c:c[1].update(exit=75))
    def test_touching_intervals(self):self.rejects(lambda c:c[2].update(exit=60))
    def test_nested_waits_overlap(self):
        def change(c):
            e=copy.deepcopy(c[2]);e.update(enter=42,exit=48);c.insert(3,e)
        self.rejects(change)
    def test_two_disjoint_nested_waits(self):
        def change(c):
            e=copy.deepcopy(c[2]);e.update(enter=51,exit=55);c.insert(3,e)
        self.assertEqual(len(self.check(change)),2)


if __name__=='__main__':
    sys.argv_fixture=sys.argv.pop(1)
    unittest.main()
