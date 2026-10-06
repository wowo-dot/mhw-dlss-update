"""Submission-order and frame-selection checks against owned GPU journals."""
import copy,json,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import review_projection_binding as review
import projection_alignment as alignment
import projection_readback as readback
import test_projection_readback as fixtures
root=Path(sys.argv.pop(1)) if len(sys.argv)>1 else Path('local/projection-binding-20260923/v0.8.2/fixtures')

class Alignment(unittest.TestCase):
    def combined(self,case='pixels'):
        f=fixtures.Readback();f.fixture_root=root
        return f.combined('readback_aligned_'+case)
    def setUp(self):self.rows,self.blobs,self.t,self.pixels=self.combined()
    def valid(self):return review.review(self.rows,self.blobs,self.t,self.pixels)
    def reject(self,edit):edit();self.assertRaises((ValueError,KeyError,StopIteration),self.valid)
    @property
    def selected(self):return self.t['frame_packets']['selection']
    @property
    def depth(self):return review.one(self.rows,'depth_readback')['details']
    def test_actual_gpu_copy_recorded_first(self):
        result=self.valid()
        self.assertTrue(result['selected_projection_copy_frame_matched'])
        self.assertTrue(result['depth_copy_recorded_before_selected_draw'])
        self.assertTrue(result['selected_draw_precedes_depth_copy_in_queue_order'])
        self.assertFalse(result['depth_pixel_producer_frame_verified'] or result['raster_jitter_verified'])
        self.assertEqual(result['depth_copy_frame_delta_from_selected_draw'],0)
        self.assertEqual(result['depth_pixel_statistics']['minimum'],.375)
    def test_separate_execute_calls(self):
        result=review.review(*self.combined('separate'))
        self.assertTrue(result['selected_projection_copy_frame_matched'])
        self.assertLess(result['submission_order'],result['depth_readback_submission'])
    def test_older_readback_contract_rejects_copy_before_draw(self):
        result=self.valid()
        self.assertRaisesRegex(ValueError,'readback chronology',readback.validate,self.t,result,review.one(self.rows,'depth_readback'),self.pixels())
    def test_missing_selection(self):self.reject(lambda:self.t['frame_packets'].pop('selection'))
    def test_pending_selection(self):self.reject(lambda:self.selected.update(published=1))
    def test_core_source_order(self):self.reject(lambda:self.selected.update(core_order=999))
    def test_renderer(self):self.reject(lambda:self.selected.update(renderer=1))
    def test_context(self):self.reject(lambda:self.selected.update(context=1))
    def test_constants(self):self.reject(lambda:self.selected.update(constants=1))
    def test_thread(self):self.reject(lambda:self.selected.update(thread=1))
    def test_buffer_id(self):self.reject(lambda:self.selected.update(buffer_id=1))
    def test_frame(self):self.reject(lambda:self.selected.update(frame=self.selected['frame']+1))
    def test_epoch(self):self.reject(lambda:self.selected.update(epoch=self.selected['epoch']+1))
    def test_bank(self):self.reject(lambda:self.selected.update(bank=1-self.selected['bank']))
    def test_async(self):self.reject(lambda:self.selected.update(async_mode=1-self.selected['async_mode']))
    def test_bool_is_not_frame(self):self.reject(lambda:self.selected.update(frame=True))
    def test_site_selection(self):self.reject(lambda:self.depth['site'].update(selection=999))
    def test_selection_native_collision(self):
        def edit():
            value=self.t['history'][0]['enter'];self.selected['order']=value;self.depth['site']['selection']=value
        self.reject(edit)
    def test_packet_before_source(self):
        def edit():self.selected['order']=999999;self.depth['site']['selection']=999999
        self.reject(edit)
    def test_wrong_request_frame(self):self.reject(lambda:self.t['frame_packets']['receipts'][0].update(frame=102))
    def test_wrong_request_bank(self):self.reject(lambda:self.t['frame_packets']['receipts'][0].update(bank=1))
    def test_wrong_request_thread(self):self.reject(lambda:self.t['frame_packets']['receipts'][0].update(thread=1))
    def test_readiness_disabled(self):self.reject(lambda:review.one(self.rows,'provenance_ready').update(same_frame_readback=False))
    def test_wrong_policy(self):
        self.rows[0]['depth_frame_policy']='latest'
        self.assertRaises(ValueError,review.parse,('\n'.join(json.dumps(r) for r in self.rows)+'\n').encode())
    def test_native_skips_and_quarantine(self):
        for mode in ('wrong_frame','no_source','source_changed','view_changed'):
            d=json.loads((root/('readback_aligned_'+mode+'.json.readback.json')).read_text())
            with self.subTest(mode=mode):
                self.assertFalse(d['mapped']);self.assertEqual(d['queue_signal_attempts'],0)
                self.assertEqual(d['copy_attempted'],mode=='view_changed')
                if mode=='view_changed':self.assertTrue(d['resources_retained'])
    def test_queue_mismatch(self):
        result=self.valid();c=next(c for c in result['depth_copy_calls'] if c['order']==self.depth['site']['copy'])
        c['queue']+=1;self.assertRaises(ValueError,alignment.ordered_path,self.t,result,c)
    def test_batch_order_reversed(self):
        result=self.valid();c=next(c for c in result['depth_copy_calls'] if c['order']==self.depth['site']['copy'])
        c['batch_index']=-1;self.assertRaises(ValueError,alignment.ordered_path,self.t,result,c)
    def test_execute_order_reversed(self):
        result=self.valid();c=next(c for c in result['depth_copy_calls'] if c['order']==self.depth['site']['copy'])
        c['submission']=result['submission_order']-1;self.assertRaises(ValueError,alignment.ordered_path,self.t,result,c)
    def path_mutation(self,change):
        result=self.valid();c=next(c for c in result['depth_copy_calls'] if c['order']==self.depth['site']['copy'])
        event=next(e for e in self.t['history'] if e['enter']==self.depth['site']['barrier'])
        change(event);self.assertRaises(ValueError,alignment.ordered_path,self.t,result,c)
    def test_intervening_clear_even_when_recorded_earlier(self):
        self.path_mutation(lambda e:e.update(kind=6,handle=self.t['marker']['handle']))
    def test_intervening_bundle(self):self.path_mutation(lambda e:e.update(kind=7))
    def test_intervening_depth_write(self):
        self.path_mutation(lambda e:e.update(kind=16,backing=self.t['marker']['resource']))
    def test_intervening_alias(self):
        def change(e):self.t['barriers'][e['first']]=[1,0,0,0,0,0,0]
        self.path_mutation(change)
    def test_overlapping_execute_calls(self):
        rows,blobs,t,pixels=self.combined('separate');result=review.review(rows,blobs,t,pixels)
        c=next(c for c in result['depth_copy_calls'] if c['order']==result['depth_readback_copy_order'])
        e=next(e for e in t['history'] if e['enter']==result['submission_order']);e['exit']=c['submission']+1
        self.assertRaises(ValueError,alignment.ordered_path,t,result,c)

if __name__=='__main__':unittest.main()
