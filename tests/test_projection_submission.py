"""Queue/generation replay using owned SDK captures plus explicitly damaged copies."""
import copy
import json
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import projection_provenance as pv
import projection_submission as ps
import review_projection_binding as review
import arm_projection_binding as control


class Submission(unittest.TestCase):
    def data(self, case='submit_batch'):
        f = Path(sys.argv_fixture) / (case + '.json')
        t = json.loads(f.read_text()); p = json.loads(Path(str(f) + '.ready.jsonl').read_text())
        # Leave explicit unused order values for independently inserted events.
        for e in t['watch'] + t['history']:
            e['enter'] *= 100; e['exit'] *= 100
        for k in ('order', 'bind_enter', 'bind_exit'):
            t['marker'][k] *= 100
        for k in ('draw_return', 'selected_submission'):
            t[k] *= 100
        for v in t['lists'] + t['queues']:
            v['first'] *= 100
        m = copy.deepcopy(t['marker'])
        r = dict(dimension=3, format=39, flags=2, samples=1, mips=1, depth_or_array=1,
                 resource=m['resource'], identity=m['identity'])
        return p, t, r, m

    def result(self, mutate=None, case='submit_batch'):
        p, t, r, m = self.data(case)
        if mutate: mutate(p, t, r, m)
        return pv.validate(p, t, r, m)

    def partial(self, mutate=None, association=True, case='submit_batch'):
        r = self.result(mutate, case)
        self.assertTrue(r['view_descriptor_provenance_verified'])
        self.assertEqual(r['selected_recording_submitted'], association)
        self.assertFalse(r['submitted_depth_state_declared'])
        self.assertTrue(r['submission_reasons'])

    def reject(self, mutate):
        with self.assertRaises((ValueError, KeyError)): self.result(mutate)

    def selected(self, t):
        return next(e for e in t['history'] if e['enter'] == t['selected_submission'])

    def insert(self, t, e):
        t['history'].append(e); t['history_reserved'] += 1

    def test_batch_with_later_cpu_recording(self):
        r = self.result()
        self.assertTrue(r['selected_recording_submitted'] and r['submitted_depth_state_declared'])
        self.assertFalse(r['local_depth_state_declared'])
        for k in ('resource_state_at_gpu_execution_verified', 'submission_verified', 'gpu_contents_verified', 'gpu_completion_verified'):
            self.assertFalse(r[k])
        _, t, _, m = self.data()
        self.assertGreater(next(e['enter'] for e in t['history'] if e['kind'] == 9), m['order'])
        self.assertEqual(r['submission_batch_index'], 1)

    def test_reversed_batch(self): self.partial(case='submit_reversed')
    def test_reset_after_draw(self): self.partial(association=False, case='submit_reset')
    def test_timeout(self):
        with self.assertRaises(ValueError): self.result(case='submit_timeout')
    def test_missing_submission(self):
        self.partial(lambda p,t,r,m:t.update(selected_submission=0), association=False)
    def test_unknown_submission(self):
        self.reject(lambda p,t,r,m:t.update(selected_submission=77))
    def test_duplicate_lists(self):
        self.reject(lambda p,t,r,m:t['submitted'].__setitem__(0,t['submitted'][1]))
    def test_submission_slice(self):
        self.reject(lambda p,t,r,m:self.selected(t).update(first=4096))
    def test_submission_count(self):
        self.reject(lambda p,t,r,m:t.update(submitted_reserved=4097))
    def test_submission_payload_uncovered(self):
        self.reject(lambda p,t,r,m:(t['submitted'].append(t['submitted'][0]),t.update(submitted_reserved=3)))
    def test_foreign_device(self): self.reject(lambda p,t,r,m:t['queues'][0].update(device=p['device']+16))
    def test_queue_identity(self): self.reject(lambda p,t,r,m:t['queues'][0].update(identity=7))
    def test_list_identity(self): self.reject(lambda p,t,r,m:t['lists'][0].update(identity=7))
    def test_retained_pin(self): self.reject(lambda p,t,r,m:t['lists'][0].update(pin=17))
    def test_initializing_pin(self): self.reject(lambda p,t,r,m:t['queues'][0].update(ready=0))
    def test_first_use(self): self.reject(lambda p,t,r,m:t['lists'][0].update(first=1))
    def test_non_direct_queue(self): self.partial(lambda p,t,r,m:t['queues'][0]['desc'].__setitem__(0,2), association=False)
    def test_non_direct_list(self):
        def change(p,t,r,m): next(v for v in t['lists'] if v['object']==m['command'])['desc'][0]=2
        self.partial(change, association=False)
    def test_failed_selected_close(self):
        def change(p,t,r,m): next(e for e in t['history'] if e['kind']==5 and e['object']==m['command'])['result']=0x80004005
        self.partial(change, association=False)
    def test_failed_prefix_reset(self):
        def change(p,t,r,m): next(e for e in t['history'] if e['kind']==8 and e['object']!=m['command'])['result']=0x80004005
        self.partial(change)
    def test_missing_prefix_reset(self):
        def change(p,t,r,m):
            e=next(e for e in t['history'] if e['kind']==8 and e['object']!=m['command']);t['history'].remove(e);t['history_reserved']-=1
        self.partial(change)
    def test_close_overlap_submission(self):
        def change(p,t,r,m): next(e for e in t['history'] if e['kind']==5 and e['object']==m['command'])['exit']=t['selected_submission']+1
        self.partial(change, association=False)
    def test_bind_overlap_reset(self):
        def change(p,t,r,m): next(e for e in t['history'] if e['kind']==8 and e['object']==m['command'])['exit']=m['bind_enter']+1
        self.partial(change, association=False)
    def test_concurrent_submission(self):
        def change(p,t,r,m):
            e=copy.deepcopy(self.selected(t));e.update(enter=e['enter']-1,exit=e['exit']+1,first=2)
            t['queues'][0]['first']=e['enter']
            t['submitted']+=t['submitted'][:];t['submitted_reserved']=4;self.insert(t,e)
        self.partial(change, association=False)
    def test_other_queue(self):
        def change(p,t,r,m):
            e=copy.deepcopy(self.selected(t));e.update(enter=e['enter']-20,exit=e['enter']-10,object=e['object']+8,identity=e['identity']+8,first=2,count=1)
            q=copy.deepcopy(t['queues'][0]);q.update(object=e['object'],identity=e['identity'],first=e['enter']);t['queues'].append(q)
            t['submitted'].append(t['submitted'][0]);t['submitted_reserved']=3;self.insert(t,e)
        self.partial(change)
    def test_bundle(self):
        def change(p,t,r,m):
            e=copy.deepcopy(next(e for e in t['history'] if e['kind']==9));e.update(kind=7,enter=e['exit']+1,exit=e['exit']+2,count=0,first=0);self.insert(t,e)
        self.partial(change)
    def test_early_resource_lifetime(self):
        def change(p,t,r,m):
            e=next(e for e in t['history'] if e['kind']==8 and e['object']!=m['command']);e.update(enter=150,exit=175)
            q=next(v for v in t['lists'] if v['object']==e['object']);q['first']=150
        self.partial(change)
    def test_descriptor_destroyed_after_draw(self):
        def change(p,t,r,m):
            e=copy.deepcopy(t['watch'][0]);e.update(kind=1,enter=t['draw_return']+1,exit=t['draw_return']+2)
            t['watch'].append(e);t['watch_reserved']+=1
        self.partial(change)
    def test_wrong_subresource(self): self.partial(lambda p,t,r,m:t['barriers'][0].__setitem__(4,3))
    def test_wrong_final_state(self): self.partial(lambda p,t,r,m:t['barriers'][0].__setitem__(6,0x80))
    def test_unresolved_split(self): self.partial(lambda p,t,r,m:t['barriers'][0].__setitem__(1,1))
    def test_unmatched_split_end(self): self.partial(lambda p,t,r,m:t['barriers'][0].__setitem__(1,2))
    def test_aliasing(self):
        self.partial(lambda p,t,r,m:t['barriers'].__setitem__(0,[1,0,m['resource'],0,0,0,0]))
    def test_draw_return_order_collision(self): self.reject(lambda p,t,r,m:t.update(draw_return=t['selected_submission']))
    def test_schema_mismatch(self): self.reject(lambda p,t,r,m:t.update(schema=1))
    def test_hook_count(self): self.reject(lambda p,t,r,m:p['targets'].pop())
    def test_cap_change(self): self.reject(lambda p,t,r,m:p.update(list_capacity=129))

    def combined(self):
        # Synthetic composition tests the reviewer boundary. These independent
        # owned native captures are not an end-to-end game capture.
        path=Path(sys.argv_fixture).parents[1]/'v0.4.0/core-final/attach_depth.jsonl'
        rows=review.parse(path.read_bytes());rows[0].update(version='0.5.0',experiment='projection-submission-readonly',base=0x140000000)
        d=review.one(rows,'draw');v=d['attachments']['ready']['views'][8]
        r=next(r for r in d['attachments']['resources'] if r['resource']==v['resource'])
        p,t,_,m=self.data()
        remap={m['command']:d['command'],m['view']:v['backend'],m['resource']:r['resource'],
               m['identity']:r['identity'],m['handle']:v['handle'],p['device']:d['device']}
        for row in [t['marker'],*t['watch'],*t['history'],*t['lists'],*t['queues']]:
            for key in ('object','command','view','resource','identity','handle','device'):
                if key in row:row[key]=remap.get(row[key],row[key])
        t['submitted']=[remap.get(v,v) for v in t['submitted']]
        for b in t['barriers']:
            b[2]=remap.get(b[2],b[2]);b[3]=remap.get(b[3],b[3])
        t['watch'][1]['caller']=rows[0]['base']+0x24e6516
        p.update(device=d['device'],backend=review.one(rows,'armed')['backend'],heap_start=v['handle'])
        review.one(rows,'armed')['device']=d['device']
        ready=review.one(rows,'ready');ready.update(hooks_installed=True,core_hooks_installed=False)
        rows.insert(rows.index(ready),p)
        return rows,lambda n:path.with_name(path.stem+f'-shader-{n}.bin').read_bytes(),t

    def test_combined_review(self):
        rows,blobs,t=self.combined()
        rows=review.parse(('\n'.join(json.dumps(r) for r in rows)+'\n').encode())
        result=review.review(rows,blobs,t)
        self.assertTrue(result['view_descriptor_provenance_verified'] and result['selected_recording_submitted'] and result['submitted_depth_state_declared'])
        self.assertFalse(result['submission_verified'])

    def test_combined_missing_companion(self):
        rows,blobs,_=self.combined()
        with self.assertRaisesRegex(ValueError,'missing provenance companion'):review.review(rows,blobs)

    def test_version_schema_mismatch(self):
        rows,blobs,t=self.combined();review.one(rows,'provenance_ready')['schema']=1
        with self.assertRaisesRegex(ValueError,'version/schema mismatch'):review.review(rows,blobs,t)

    def test_controller(self):
        from unittest.mock import patch
        rows,_,_=self.combined();rows=rows[:rows.index(review.one(rows,'ready'))+1];rows[0]['owned_fixture']=False
        with patch.object(control,'reservation'):
            control.validate(rows,1,rows[0]['pid'],rows[0]['creation'])
            review.one(rows,'provenance_ready')['schema']=1
            with self.assertRaises(ValueError):control.validate(rows,1,rows[0]['pid'],rows[0]['creation'])


if __name__ == '__main__':
    sys.argv_fixture = sys.argv.pop(1)
    unittest.main()
