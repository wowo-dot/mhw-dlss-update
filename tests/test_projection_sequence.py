"""Verify five owned GPU readbacks and damage the independently composed CPU/core review."""
import copy,json,sys,unittest
from pathlib import Path
root=Path(sys.argv.pop(1)) if len(sys.argv)>1 else Path('local/projection-binding-20260923/v0.9.0/fixtures')
import test_projection_triplet as f
import projection_stimulus as stimulus
import review_projection_binding as review
f.root=root

class Sequence(unittest.TestCase):
    def full(self):return f.Triplet().full('readback_sequence_pixels',5)
    def setUp(self):self.args=list(self.full())
    def valid(self):return review.review(*self.args)
    def reject(self,action):
        action()
        with self.assertRaises((ValueError,KeyError,StopIteration)):self.valid()
    @property
    def cpu(self):return review.one(self.args[0],'stimulus')
    @property
    def controls(self):return [r for r in self.args[0] if r['event']=='depth_control']
    def test_complete_sequence(self):
        result=self.valid()
        self.assertTrue(result['depth_sequence_readbacks_completed'])
        self.assertEqual(result['depth_sequence_frames'],[101,102,103,104,105])
        self.assertEqual(result['injections'],3);self.assertEqual(result['restorations'],3)
        self.assertEqual(result['expected_sequence_pixel_displacements'],[[.125,.125],[-.125,-.1875],[.0625,.0625]])
        self.assertFalse(result['raster_jitter_verified'] or result['depth_control_draw_bindings_verified'])
        self.assertEqual([x['depth_pixel_statistics']['minimum'] for x in result['depth_control_results']],[.25,.5,.625,.75])
    def test_native_negative_cases(self):
        for name in ('missing_baseline','wrong_recovery','view_changed','extra_copy','wrong_middle'):
            with self.subTest(case=name),self.assertRaises((ValueError,KeyError,StopIteration)):
                review.review(*f.Triplet().full('readback_sequence_'+name,5))
    def test_native_core_sequence(self):
        for name in ('chain','deferred'):
            rows=[json.loads(x) for x in (root/f'jitter_sequence_{name}.jsonl').read_text().splitlines()]
            cpu=review.one(rows,'stimulus');s=review.one(rows,'source');p=cpu['calls'][0]['before']
            result=stimulus.validate(cpu,s,p,review.one(rows,'publication'),dict(version='0.9.0',window_ms=2000))
            self.assertEqual(result['injections'],3);self.assertEqual(result['sequence_primary_draw_waits'],1)
            self.assertGreater(cpu['calls'][2]['source']['order'],review.one(rows,'draw')['exit_order'])
    def test_missing_middle_image(self):self.reject(lambda:self.args[0].remove(self.controls[2]))
    def test_middle_unmapped(self):self.reject(lambda:self.controls[2]['details'].update(mapped=False))
    def test_missing_final_image(self):self.reject(lambda:self.args[0].remove(self.controls[3]))
    def test_extra_image(self):self.reject(lambda:self.args[0].append(copy.deepcopy(self.controls[3])))
    def test_missing_middle_selection(self):self.reject(lambda:self.args[2]['frame_packets']['controls'].pop(2))
    def test_selection_wrong_frame(self):self.reject(lambda:self.args[2]['frame_packets']['controls'][2]['selection'].update(frame=900))
    def test_control_stages_bool(self):self.reject(lambda:self.controls[0].update(stage=False))
    def test_control_stages_duplicate(self):self.reject(lambda:self.controls[3].update(stage=3))
    def test_later_buffer_reused(self):self.reject(lambda:self.controls[2]['details'].update(buffer=self.controls[1]['details']['buffer']))
    def test_later_fence_reused(self):self.reject(lambda:self.controls[3]['details'].update(fence=self.controls[2]['details']['fence']))
    def test_later_queue_changed(self):self.reject(lambda:self.controls[2]['details']['site'].update(queue=1234))
    def test_later_hash(self):self.reject(lambda:self.controls[3].update(sha256='0'*64))
    def test_later_unretired(self):self.reject(lambda:self.controls[3]['details'].update(recording_retired=False))
    def test_later_restore(self):self.reject(lambda:self.cpu['calls'][2].update(restored=False))
    def test_later_nonzero_restored(self):self.reject(lambda:self.cpu['calls'][3].update(restored_bits=1))
    def test_later_injection_missing(self):self.reject(lambda:self.cpu['calls'][2].update(injected=False))
    def test_recovery_injected(self):self.reject(lambda:self.cpu['calls'][4].update(injected=True))
    def test_wrong_budget(self):self.reject(lambda:self.cpu.update(injections=4))
    def test_draw_wait_overflow(self):self.reject(lambda:self.cpu.update(draw_waits=65))
    def test_draw_wait_missing(self):self.reject(lambda:self.cpu.pop('draw_waits'))
    def test_draw_wait_bool(self):self.reject(lambda:self.cpu.update(draw_waits=True))
    def test_recovery_deferral_overflow(self):self.reject(lambda:self.cpu.update(recovery_deferrals=65))
    def test_repeat_sample(self):self.reject(lambda:self.cpu['calls'][2].update(candidate=self.cpu['calls'][1]['candidate']))
    def test_later_viewport_changed(self):self.reject(lambda:self.cpu['calls'][3]['source'].update(viewport_bits=[1,2]))
    def test_early_second_stimulus(self):self.reject(lambda:review.one(self.args[0],'draw').update(exit_order=self.cpu['calls'][2]['source']['order']))
    def test_sequence_readiness(self):self.reject(lambda:review.one(self.args[0],'provenance_ready').update(sequence_readback=False))
    def test_wrong_readback_slots(self):self.reject(lambda:review.one(self.args[0],'provenance_ready').update(readback_slots=3))
    def test_header_contract(self):
        for field,value in [('injection_budget',1),('cpu_samples',3),('jitter_writes',1),('gpu_copy_budget',3),('queue_signal_budget',3),('max_draw_waits',65)]:
            rows=copy.deepcopy(self.args[0]);rows[0][field]=value
            with self.subTest(field=field),self.assertRaises(ValueError):review.parse(('\n'.join(map(json.dumps,rows))+'\n').encode())

if __name__=='__main__':unittest.main()
