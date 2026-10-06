"""Damage real owned readback journals and their GPU-written pixel companions."""
import copy
import hashlib
import json
import struct
import sys
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import projection_provenance as pv
import projection_readback as rb
import review_projection_binding as review

class Readback(unittest.TestCase):
    def data(self,case='readback_pixels'):
        f=Path(self.fixture_root if hasattr(self,'fixture_root') else sys.argv_fixture)/(case+'.json')
        t=json.loads(f.read_text());p=json.loads(Path(str(f)+'.ready.jsonl').read_text())
        d=json.loads(Path(str(f)+'.readback.json').read_text());m=t['marker']
        resource=dict(dimension=3,format=39,flags=2,samples=1,mips=1,depth_or_array=1,width=16,height=16,
                      resource=m['resource'],identity=m['identity'])
        result=pv.validate(p,t,resource,m)
        pixel=Path(str(f)+'.depth.f32');raw=pixel.read_bytes() if pixel.exists() else b''
        record=dict(event='depth_readback',saved=d['phase']==7,file='binding-depth.f32',sha256=hashlib.sha256(raw).hexdigest(),details=d)
        return t,result,record,raw

    def reject(self,mutate):
        t,r,c,b=self.data();mutate(t,r,c)
        with self.assertRaises((ValueError,KeyError,StopIteration)):rb.validate(t,r,c,b)

    def test_actual_gpu_pixels(self):
        for case in ('readback_pixels','readback_twice'):
            with self.subTest(case=case):
                result=rb.validate(*self.data(case))
                self.assertTrue(result['depth_readback_completed'])
                self.assertEqual(result['depth_pixel_count'],256)
                self.assertEqual(result['depth_pixel_statistics']['minimum'],.375)
                self.assertFalse(result['depth_has_variation'] or result['final_scene_depth_verified'] or result['raster_jitter_verified'])

    def test_skipped_native_windows(self):
        for case in ('readback_combined','readback_view_changed'):
            with self.subTest(case=case),self.assertRaises(ValueError):rb.validate(*self.data(case))

    def test_hash(self):self.reject(lambda t,r,c:c.update(sha256='0'*64))
    def test_saved(self):self.reject(lambda t,r,c:c.update(saved=False))
    def test_phase(self):self.reject(lambda t,r,c:c['details'].update(phase=5))
    def test_not_issued(self):self.reject(lambda t,r,c:c['details'].update(copy_attempted=False))
    def test_copy_exception(self):self.reject(lambda t,r,c:c['details'].update(copy_returned=False))
    def test_released_buffer(self):self.reject(lambda t,r,c:c['details'].update(resources_retained=False))
    def test_unretired_recording(self):self.reject(lambda t,r,c:c['details'].update(recording_retired=False))
    def test_retirement_selector(self):self.reject(lambda t,r,c:c['details'].update(retire_reset=1))
    def test_failed_retirement(self):
        def change(t,r,c):next(e for e in t['history'] if e['enter']==c['details']['retire_reset']).update(result=0x80004005)
        self.reject(change)
    def test_premature_mapping(self):self.reject(lambda t,r,c:c['details'].update(completed_value=0))
    def test_fence_device_removed(self):self.reject(lambda t,r,c:c['details'].update(completed_value=2**64-1))
    def test_external_fence_value(self):self.reject(lambda t,r,c:c['details'].update(completed_value=2))
    def test_shared_fence(self):self.reject(lambda t,r,c:c['details'].update(fence_flags=1))
    def test_signal_failure(self):self.reject(lambda t,r,c:c['details'].update(signal_result=0x80004005))
    def test_multiple_signals(self):self.reject(lambda t,r,c:c['details'].update(queue_signal_attempts=2))
    def test_map_failure(self):self.reject(lambda t,r,c:c['details'].update(map_result=0x80004005))
    def test_extent(self):self.reject(lambda t,r,c:c['details'].update(width=17))
    def test_footprint_pitch(self):self.reject(lambda t,r,c:c['details']['footprint'].__setitem__(5,255))
    def test_partial_depth(self):self.reject(lambda t,r,c:c['details'].update(has_box=True))
    def test_offset(self):self.reject(lambda t,r,c:c['details'].update(xyz=[1,0,0]))
    def test_subresource(self):self.reject(lambda t,r,c:c['details'].update(subresource=1))
    def test_allocation(self):self.reject(lambda t,r,c:c['details'].update(allocation_bytes=1))
    def test_native_buffer(self):self.reject(lambda t,r,c:c['details'].update(buffer=t['marker']['resource']))
    def test_native_fence(self):self.reject(lambda t,r,c:c['details'].update(fence=t['fences'][0]['object']))
    def test_view_unqualified(self):self.reject(lambda t,r,c:r.update(view_descriptor_provenance_verified=False))
    def test_identity(self):self.reject(lambda t,r,c:c['details']['site'].update(identity=1))
    def test_reset(self):self.reject(lambda t,r,c:c['details']['site'].update(reset=1))
    def test_copy_selector(self):self.reject(lambda t,r,c:c['details']['site'].update(copy=1))
    def test_barrier_selector(self):self.reject(lambda t,r,c:c['details']['site'].update(barrier=1))
    def test_allocator(self):self.reject(lambda t,r,c:c['details']['site'].update(allocator=1))
    def test_execute_return(self):self.reject(lambda t,r,c:c['details']['site'].update(execute_exit=1))
    def test_signal_before_execute(self):self.reject(lambda t,r,c:c['details']['site'].update(signal_begin=c['details']['site']['execute']))
    def test_copy_overlaps_close(self):self.reject(lambda t,r,c:c['details']['site'].update(end=c['details']['site']['close']))
    def test_stats(self):self.reject(lambda t,r,c:c['details']['pixels'].update(zero=1))
    def test_extrema(self):self.reject(lambda t,r,c:c['details']['pixels'].update(minimum=.5))
    def test_nonfinite_and_outside(self):
        for value in (float('nan'),float('inf'),-.01,1.01):
            t,r,c,b=self.data();raw=struct.pack('<f',value)+b[4:];c['sha256']=hashlib.sha256(raw).hexdigest()
            with self.subTest(value=value),self.assertRaises(ValueError):rb.validate(t,r,c,raw)

    def test_header_budget(self):
        h=dict(event='start',schema=1,version='0.8.0',host_sha256=review.HOST,experiment='projection-depth-readback',
               jitter_writes=0,gpu_copy_budget=1,queue_signal_budget=1,window_ms=2000,max_barriers=64,max_resources=9,pid=1,creation=1)
        def parse(obj):return review.parse((json.dumps(obj)+'\n').encode())
        self.assertEqual(parse(h)[0],h)
        for change in ({'gpu_copy_budget':2},{'queue_signal_budget':2},{'gpu_commands_added':0},{'experiment':'projection-copy-readonly'}):
            with self.subTest(change=change),self.assertRaises(ValueError):parse(dict(h,**change))

    def combined(self,case='readback_pixels'):
        # Synthetic composition of independent owned journals, not game evidence.
        directory=Path(self.fixture_root if hasattr(self,'fixture_root') else sys.argv_fixture)
        path=directory.parents[1]/'v0.4.0/core-final/attach_depth.jsonl'
        rows=review.parse(path.read_bytes());h=rows[0]
        h.update(version='0.8.0',experiment='projection-depth-readback',base=0x140000000,gpu_copy_budget=1,queue_signal_budget=1)
        h.pop('gpu_commands_added')
        t,_,record,raw=self.data(case);p=json.loads((directory/(case+'.json.ready.jsonl')).read_text())
        d=review.one(rows,'draw');v=d['attachments']['ready']['views'][8];m=t['marker']
        resource=next(r for r in d['attachments']['resources'] if r['resource']==v['resource'])
        remap={m['command']:d['command'],m['view']:v['backend'],m['resource']:resource['resource'],
               m['identity']:resource['identity'],m['handle']:v['handle'],p['device']:d['device']}
        for item in [m,*t['watch'],*t['history'],*t['lists'],*t['queues'],*t['fences'],*t['copy_resources'],record['details']['site']]:
            for key in ('object','command','view','resource','identity','handle','device','backing'):
                if key in item:item[key]=remap.get(item[key],item[key])
        for b in t['barriers']:b[2]=remap.get(b[2],b[2]);b[3]=remap.get(b[3],b[3])
        for c in t['copies']:
            for side in ('source','destination'):c[side][0]=remap.get(c[side][0],c[side][0])
        t['submitted']=[remap.get(value,value) for value in t['submitted']]
        if 'frame_packets' in t:
            h['version']='0.8.1';f=t['frame_packets'];s=review.one(rows,'source')
            f['renderer']=s['renderer']
            for receipt in f['receipts']:receipt['context']=s['renderer']+0x710
            for consumer in f['consumers']:
                for key in ('command','source_resource','destination_resource'):
                    consumer[key]=remap.get(consumer[key],consumer[key])
            if 'selection' in f:
                h.update(version='0.8.2',depth_frame_policy='selected-source-request');selection=f['selection']
                # Synthetic composition remaps independent owned identities,
                # while retaining the native GPU submission and copy chronology.
                for key in ('frame','epoch','bank','async_mode','thread'):s[key]=selection[key]
                for key,original in [('core_order','order'),('renderer','renderer'),('context','context'),('constants','constants'),('buffer_id','buffer_id')]:
                    selection[key]=s[original]
        for e in t['watch']:
            if e['kind']==2:e['caller']=h['base']+0x24e6516
        for e in t['history']:
            if e['kind']==16:e['caller']=h['base']+0x259e100
        desc=next(c['desc'] for c in t['copy_resources'] if c['object']==resource['resource'])
        for key,i in [('alignment',1),('width',2),('height',3)]:resource[key]=desc[i]
        p.update(device=d['device'],backend=review.one(rows,'armed')['backend'],heap_start=v['handle'])
        ready=review.one(rows,'ready');ready.update(hooks_installed=True,core_hooks_installed=False)
        rows.insert(rows.index(ready),p);rows.append(record)
        rows=review.parse(('\n'.join(json.dumps(row) for row in rows)+'\n').encode())
        return rows,lambda n:path.with_name(path.stem+f'-shader-{n}.bin').read_bytes(),t,lambda:raw

    def test_full_review(self):
        r=review.review(*self.combined())
        self.assertTrue(r['depth_readback_completed'] and r['resource_identity_roundtrip_verified'])
        self.assertFalse(r['gpu_contents_verified'] or r['raster_jitter_verified'])

    def test_full_review_requires_pixels(self):
        with self.assertRaisesRegex(ValueError,'missing depth pixels'):review.review(*self.combined()[:3])

    def test_full_review_rejects_pixel_corruption(self):
        rows,blobs,t,_=self.combined()
        with self.assertRaisesRegex(ValueError,'depth bytes/hash'):review.review(rows,blobs,t,lambda:b'no pixels')

    def test_new_version_requires_all_resource_query_phases(self):
        rows,blobs,t,pixels=self.combined()
        review.one(rows,'draw')['attachments']['resource_checks'][2]['queries'][0]['valid']=False
        with self.assertRaisesRegex(ValueError,'resource query incomplete'):review.review(rows,blobs,t,pixels)

if __name__=='__main__':
    sys.argv_fixture=sys.argv.pop(1)
    unittest.main()
