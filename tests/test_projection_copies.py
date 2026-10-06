"""Schema-4 regression and independent damage of captured copy arguments."""
import copy
import json
import sys
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import projection_provenance as pv
import test_projection_sync as legacy


class Schema4(legacy.Synchronization):
    def data(self,case='submit_batch'):
        p,t,r,m=super().data(case)
        for row in t['copy_resources']:row['first']*=100
        return p,t,r,m

    def combined(self):
        rows,blobs,t=super().combined()
        rows[0].update(version='0.7.1',experiment='projection-copy-readonly')
        return rows,blobs,t

    def test_version_requires_retention_metadata(self):
        rows,blobs,t=self.combined()
        p=legacy.review.one(rows,'provenance_ready');p.pop('copy_retention');p.pop('copy_pin_capacity')
        with self.assertRaisesRegex(ValueError,'copy retention version'):legacy.review.review(rows,blobs,t)


class Copies(unittest.TestCase):
    def data(self,case='gpu_copy_footprint'):
        f=Path(sys.argv_fixture)/(case+'.json')
        t=json.loads(f.read_text());p=json.loads(Path(str(f)+'.ready.jsonl').read_text())
        m=copy.deepcopy(t['marker'])
        r=dict(dimension=3,format=39,flags=2,samples=1,mips=1,depth_or_array=1,width=16,height=16,
               resource=m['resource'],identity=m['identity'])
        return p,t,r,m

    def result(self,change=None,case='gpu_copy_footprint'):
        p,t,r,m=self.data(case)
        if change:change(p,t,r,m)
        return pv.validate(p,t,r,m)

    def reject(self,change):
        with self.assertRaises(ValueError):self.result(change)

    def partial(self,change):
        r=self.result(change)
        self.assertTrue(r['depth_copy_calls'][0]['reasons'])
        self.assertFalse(r['depth_copy_readback_ready'])

    def test_three_actual_gpu_copies(self):
        for case in ('gpu_copy_whole','gpu_copy_texture','gpu_copy_footprint'):
            with self.subTest(case=case):
                r=self.result(case=case);self.assertTrue(r['selected_recording_submitted'])
                self.assertTrue(r['selected_queue_fence_completion_observed'])
                self.assertEqual(len(r['depth_copy_calls']),1)
                c=r['depth_copy_calls'][0]
                self.assertTrue(c['recording_submitted'] and c['full_depth_source'])
                self.assertEqual(c['local_copy_source_state'],0x800)
                self.assertEqual(c['reasons'],[])
                self.assertFalse(r['depth_copy_readback_ready'] or r['gpu_contents_verified'])

    def test_sdk_footprint(self):
        p,t,r,m=self.data();pv.structure(p,t);c=t['copies'][0]
        self.assertEqual(c['source'][1:],[0,0,0,0,0,0,0])
        self.assertEqual(c['destination'][1:],[1,512,39,16,16,1,256])

    def test_stack_box_and_uint_decoding(self):
        p,t,_,_=self.data('copy_arguments');pv.structure(p,t)
        c=t['copies'][0];self.assertTrue(c['has_box'])
        self.assertEqual(c['xyz'],[7,11,13]);self.assertEqual(c['box'],[1,2,0,4,5,1])

    def test_component_failures(self):
        for case in ('copy_bad_location','copy_null_resource','copy_exception','copy_capacity','copy_concurrent_exception',
                     'copy_merge_desc','copy_merge_identity','copy_merge_device','copy_merge_capacity'):
            with self.subTest(case=case),self.assertRaises(ValueError):self.result(case=case)

    def test_bounds(self):self.reject(lambda p,t,r,m:p.update(copy_capacity=513))
    def test_missing_payload(self):self.reject(lambda p,t,r,m:t['copies'].clear())
    def test_duplicate_slice(self):
        def change(p,t,r,m):t['copies'].append(copy.deepcopy(t['copies'][0]));t['copies_reserved']+=1
        self.reject(change)
    def test_changed_pointer(self):self.reject(lambda p,t,r,m:t['copies'][0]['source'].__setitem__(0,7))
    def test_invalid_union(self):self.reject(lambda p,t,r,m:t['copies'][0]['source'].__setitem__(1,3))
    def test_dirty_subresource_union(self):self.reject(lambda p,t,r,m:t['copies'][0]['source'].__setitem__(4,16))
    def test_footprint_alignment(self):self.reject(lambda p,t,r,m:t['copies'][0]['destination'].__setitem__(2,1))
    def test_footprint_row_pitch(self):self.reject(lambda p,t,r,m:t['copies'][0]['destination'].__setitem__(7,255))
    def test_stale_reference(self):self.reject(lambda p,t,r,m:t['copy_resources'][0].update(pin=1))
    def test_foreign_device(self):self.reject(lambda p,t,r,m:t['copy_resources'][0].update(device=1))
    def test_first_use(self):self.reject(lambda p,t,r,m:t['copy_resources'][0].update(first=1))
    def test_description_changed(self):self.partial(lambda p,t,r,m:t['copy_resources'][0]['desc'].__setitem__(2,17))
    def test_identity_changed(self):self.partial(lambda p,t,r,m:t['copy_resources'][0].update(identity=7))
    def test_offset_not_full_depth(self):self.partial(lambda p,t,r,m:t['copies'][0]['xyz'].__setitem__(0,1))
    def test_subresource_not_full_depth(self):self.partial(lambda p,t,r,m:t['copies'][0]['source'].__setitem__(2,1))
    def test_incompatible_source_state(self):
        def change(p,t,r,m):
            for b in t['barriers']:
                if b[5]==0x10 and b[6]==0x800:b[6]=0x400
        self.partial(change)
    def test_missing_copy_generation(self):
        def change(p,t,r,m):
            close=next(e for e in t['history'] if e['kind']==5 and e['object']==m['command']);close['result']=0x80004005
        self.partial(change)

    def test_concurrent_first_use_and_cleanup(self):
        p,t,_,_=self.data('copy_concurrent');pv.structure(p,t)
        a,b=sorted((e for e in t['history'] if e['kind']==16),key=lambda e:e['enter'])
        self.assertNotEqual(a['thread'],b['thread']);self.assertNotEqual(a['object'],b['object'])
        self.assertTrue(a['enter'] < b['enter'] < b['exit'] < a['exit'])
        self.assertTrue(a['valid'] and b['valid'] and a['returned'] and b['returned'])
        self.assertEqual(a['resource'],b['resource']);self.assertEqual(a['backing'],b['backing'])
        self.assertEqual(t['copy_refs_acquired'],4);self.assertEqual(t['copy_refs_released'],4)
        self.assertEqual(len(t['copy_resources']),1)
        self.assertEqual(t['copy_resources'][0]['first'],a['enter'])

    def test_reference_count_missing(self):self.reject(lambda p,t,r,m:t.pop('copy_refs_acquired'))
    def test_reference_count_mismatch(self):self.reject(lambda p,t,r,m:t.update(copy_refs_released=0))
    def test_reference_count_wrong_total(self):self.reject(lambda p,t,r,m:t.update(copy_refs_acquired=4,copy_refs_released=4))
    def test_retention_policy(self):self.reject(lambda p,t,r,m:p.update(copy_retention='shared-initializer'))
    def test_pin_capacity(self):self.reject(lambda p,t,r,m:p.update(copy_pin_capacity=1025))
    def test_concurrent_exception_releases_references(self):
        _,t,_,_=self.data('copy_concurrent_exception')
        self.assertEqual(t['copy_refs_acquired'],4);self.assertEqual(t['copy_refs_released'],4)
        self.assertTrue(t['released'] and t['disabled']);self.assertEqual(t['pending'],0)

    def test_rejected_merge_releases_every_reference(self):
        for case in ('copy_merge_desc','copy_merge_identity','copy_merge_device','copy_merge_capacity'):
            with self.subTest(case=case):
                _,t,_,_=self.data(case)
                self.assertEqual(t['copy_refs_acquired'],t['copy_refs_released'])
                self.assertEqual(t['copy_refs_acquired'],2*t['copies_reserved'])
                self.assertTrue(t['released'] and t['disabled']);self.assertEqual(t['pending'],0)


if __name__=='__main__':
    sys.argv_fixture=sys.argv.pop(1)
    unittest.main()
