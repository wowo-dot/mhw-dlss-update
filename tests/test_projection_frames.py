"""Mutation tests against a real owned GPU packet/readback journal."""
import copy
import json
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import projection_frames as frames
import projection_provenance as pv
import review_projection_binding as review
import test_projection_readback as rb_tests

root=Path(sys.argv.pop(1)) if len(sys.argv)>1 else Path('local/projection-binding-20260923/v0.8.1/fixtures-final')
def load(mode):
    p=root/('readback_frame_'+mode+'.json')
    t=json.loads(p.read_text());d=json.loads(Path(str(p)+'.readback.json').read_text())
    return t,d,dict(renderer=t['frame_packets']['renderer'],frame=100,epoch=90)

class Frames(unittest.TestCase):
    def setUp(self):self.t,self.d,self.s=load('pixels')
    def valid(self):return frames.validate(self.t,self.d,self.s)
    def reject(self,edit):
        edit();self.assertRaises(ValueError,self.valid)
    def test_gpu_receipt(self):
        r=self.valid();self.assertEqual(r['depth_copy_request_frame'],101)
        self.assertEqual(r['depth_copy_frame_delta_from_selected_draw'],1)
        self.assertFalse(r['depth_pixel_producer_frame_verified'])
    def test_reused_address_uses_new_generation(self):
        self.t,self.d,self.s=load('reuse');self.assertEqual(self.valid()['depth_copy_request_frame'],102)
    def test_stamp_gap_is_reported_not_assumed(self):
        self.s['frame']=7;self.assertEqual(self.valid()['depth_copy_frame_delta_from_selected_draw'],94)
    def test_stamp_regression_delta(self):
        self.s['frame']=102;self.assertEqual(self.valid()['depth_copy_frame_delta_from_selected_draw'],-1)
    def test_consumer_epoch_does_not_replace_original(self):
        self.t['frame_packets']['consumers'][0]['later_global_frame']=999
        self.assertEqual(self.valid()['depth_copy_request_frame'],101)
    def test_missing_receipt(self):self.reject(lambda:self.c.update(receipt=0))
    def test_bad_receipt_index(self):self.reject(lambda:self.c.update(receipt=257))
    def test_missing_native_copy(self):self.reject(lambda:self.c.update(copy=999999))
    def test_changed_packet(self):self.reject(lambda:self.c['data'].update(nested=123))
    def test_source_identity(self):self.reject(lambda:self.c.update(source_resource=42))
    def test_command_identity(self):self.reject(lambda:self.c.update(command=42))
    def test_destination_identity(self):self.reject(lambda:self.c.update(destination_resource=42))
    def test_thread_identity(self):self.reject(lambda:self.c.update(thread=42))
    def test_native_calls(self):self.reject(lambda:self.c.update(native_calls=2))
    def test_exception(self):self.reject(lambda:self.c.update(returned=False))
    def test_pending(self):self.reject(lambda:self.p.update(published=1))
    def test_producer_changed(self):self.reject(lambda:self.p.update(valid=False))
    def test_frontend_context(self):self.reject(lambda:self.p.update(context=42))
    def test_renderer(self):self.reject(lambda:self.s.update(renderer=42))
    def test_bad_bank(self):self.reject(lambda:self.p.update(bank=2))
    def test_bad_epoch(self):self.reject(lambda:self.p.update(epoch=True))
    def test_array_overflow(self):self.reject(lambda:self.p.update(after_array=2**64-1))
    def test_count_no_append(self):self.reject(lambda:self.p.update(after_count=self.p['before_count']))
    def test_count_bound(self):self.reject(lambda:self.p.update(after_count=65537))
    def test_count_jump(self):self.reject(lambda:self.p.update(after_count=self.p['before_count']+65))
    def test_payload_owner(self):self.reject(lambda:self.p.update(source=42))
    def test_capacity(self):self.reject(lambda:self.t['frame_packets'].update(receipts_reserved=257))
    def test_native_order_collision(self):self.reject(lambda:self.p.update(enter=self.d['site']['copy']))
    def test_copy_not_nested(self):self.reject(lambda:self.c.update(enter=self.c['copy']+1))
    def test_packet_rebuilt_during_consumer(self):
        other=copy.deepcopy(self.p);other.update(enter=self.c['enter']+1,exit=self.c['exit']-1)
        def edit():self.t['frame_packets']['receipts'].append(other);self.t['frame_packets']['receipts_reserved']=2
        self.reject(edit)
    def test_double_consumption(self):
        other=copy.deepcopy(self.c);other.update(enter=100000,exit=100001,copy=0)
        def edit():self.t['frame_packets']['consumers'].append(other);self.t['frame_packets']['consumers_reserved']=2
        self.reject(edit)
    def test_owned_skips_have_no_linked_readback(self):
        for mode in ['changed','orphan','stamp']:
            with self.subTest(mode=mode):
                self.t,self.d,self.s=load(mode);self.assertFalse(self.d['copy_attempted']);self.assertRaises(ValueError,self.valid)
    def test_ready_tag_requires_two_hooks(self):
        p=json.loads((root/'readback_frame_pixels.json.ready.jsonl').read_text())
        pv.ready(p);p['targets'].pop();self.assertRaises(ValueError,pv.ready,p)
    def test_native_noops_skip_receipt(self):
        for mode in ('empty','self'):
            with self.subTest(mode=mode):
                self.t,self.d,self.s=load(mode)
                self.assertEqual(len(self.t['frame_packets']['receipts']),1)
                self.assertEqual(self.valid()['depth_copy_request_frame'],101)
    def combined(self):
        fixture=rb_tests.Readback();fixture.fixture_root=root
        return fixture.combined('readback_frame_pixels')
    def test_full_review(self):
        result=review.review(*self.combined())
        self.assertTrue(result['depth_readback_completed'] and result['depth_copy_request_frame_verified'])
        self.assertFalse(result['depth_pixel_producer_frame_verified'] or result['raster_jitter_verified'])
    def test_full_review_requires_packets(self):
        rows,blobs,t,pixels=self.combined();t.pop('frame_packets')
        self.assertRaisesRegex(ValueError,'frame packet header',review.review,rows,blobs,t,pixels)
    def test_full_review_requires_tag_hooks(self):
        rows,blobs,t,pixels=self.combined();review.one(rows,'provenance_ready')['frame_tagging']=False
        self.assertRaises(ValueError,review.review,rows,blobs,t,pixels)
    def test_full_review_rejects_changed_packet(self):
        rows,blobs,t,pixels=self.combined();t['frame_packets']['consumers'][0]['data']['nested']=42
        self.assertRaisesRegex(ValueError,'packet publication/consumption',review.review,rows,blobs,t,pixels)
    @property
    def p(self):return self.t['frame_packets']['receipts'][0]
    @property
    def c(self):return self.t['frame_packets']['consumers'][0]

if __name__=='__main__':unittest.main()
