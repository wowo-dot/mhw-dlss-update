import sys
import unittest
from pathlib import Path
from unittest.mock import patch
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
from frame_generation_scene import scene_review, review, raster_metadata
from reflex_frames import HOST_SHA


def valid():
    # CPU records final first; GPU executes the other list first. The result
    # must use submission order rather than callback timestamp.
    events = []
    for i, batch_index in enumerate((1, 0, 2)):
        events.append(dict(kind=0, ordinal=i+1, qpc=i+1, thread=5, stage=2 if i == 0 else 0xffffffff,
                           list=100+i, generation=1, caller=1000, packet=50+i, resource=70, rtv=1, root=2,
                           pipeline=3, api_state_matched=True, returned=True, replaced=i == 0, pipeline_read=True, rotation=0,
                           native_frame=11,native_epoch=21,
                           batch=1, queue=200, submit_qpc=5, batch_index=batch_index, batch_count=3, args=[3, 1, 0, 0, 0],
                           native_cache=[9,100+i,50+i,10,10,1,40,41,1,2,3],native_cache_read_mask=0x7ff,
                           raster_observation=[1,1,1,1,1,1,1,1,47,1,1,1,1,1,1,1,0,0,0]))
    frame = dict(request=1, identity=[10, 20, 30, 40, 50, 60, 70, 2560, 1440, 87, 0], presented=True,
                 reflex_id=10, present_thread=5, swap=9, present_qpc=6, overflow=0, duplicate_submissions=0,
                 premature_resets=0, choice=2, present_frame=11,present_epoch=21,
                 observations=events, events=3, unreadable=0, outside_scope=0, unmatched=0,
                 followed_resources=[70], resource_overflow=0, rejection_counts=[0]*16, rejection_samples=[])
    return dict(schema=4, policy='submitted-scene-raster-scope-map-v4', diagnostic_only=True, gpu_work_added=False,
                pointers_owned=False, pixel_contents_verified=False, GPU_completion_verified=False,
                capacity=4, event_capacity=512, resource_capacity=16,rejection_reasons=16,samples_per_reason=2,
                pending=0, requested=1, captures=[frame])


def copy_trace():
    t=valid();f=t['captures'][0];e=dict(f['observations'][-1])
    e.update(kind=4,ordinal=4,qpc=4,stage=0xffffffff,replaced=False,resource=70,
             native_cache=[0]*11,native_cache_read_mask=0,raster_observation=[0]*19,
             copy=[70,80,0,0,0,0,0,0,0,1,0,0,0,2560,1440,1])
    f['observations'].append(e);f['events']=4;f['followed_resources']=[70,80]
    return t


def sample(reason=0):
    return dict(reason=reason,kind=0,qpc=4,thread=5,caller=1000,list=999,generation=0,native_frame=11,
                native_epoch=21,packet=0,resource=0,rtv=123,root=0,pipeline=0,batch=0,queue=0,
                submit_qpc=0,batch_index=0,batch_count=0,reset=False,native_cache=[0]*11,
                native_cache_read_mask=0,raster_observation=[0]*19)


class SceneTests(unittest.TestCase):
    def test_order_and_scope(self):
        r=scene_review(valid(),1000,5,[70])['captures'][0]
        self.assertEqual(r['post_final_draws'],1)
        self.assertTrue(r['target_interface_seen_in_GetBuffer'])
        self.assertFalse(r['UI_semantics_verified'])
    def test_corrupted_receipts(self):
        for field, value in [('resource',71),('generation',0),('returned',False),('submit_qpc',0),('batch',0),('ordinal',2)]:
            with self.subTest(field=field):
                t=valid();t['captures'][0]['observations'][0][field]=value
                with self.assertRaises(Exception):scene_review(t,1000,5)
    def test_unknown_scope_is_explicit(self):
        t=valid();f=t['captures'][0];f['unreadable']=3;f['rejection_counts'][1]=3
        f['rejection_samples']=[sample(1),sample(1)]
        self.assertFalse(scene_review(t,1000,5)['captures'][0]['complete_direct_draw_scope'])
    def test_reject_truncation_and_unfinished(self):
        for field, value in [('overflow',1),('premature_resets',1),('presented',False),('present_thread',6),('choice',0)]:
            t=valid();t['captures'][0][field]=value
            with self.assertRaises(Exception):scene_review(t,1000,5)
    def test_no_cross_queue_order_claim(self):
        t=valid();t['captures'][0]['observations'][2].update(queue=201,batch=2,batch_index=0,batch_count=1)
        self.assertIsNone(scene_review(t,1000,5)['captures'][0]['post_final_draws'])
    def test_duplicate_final_and_wrong_scope(self):
        t=valid();t['captures'][0]['observations'][1]['stage']=2
        with self.assertRaises(Exception):scene_review(t,1000,5)
        for field in ('gpu_work_added','pixel_contents_verified','pointers_owned'):
            t=valid();t[field]=True
            with self.assertRaises(Exception):scene_review(t,1000,5)
    def test_wrapper_uses_qualified_image_and_last_capture(self):
        rows=[dict(event='start',schema=6,version='0.6.4',experiment='frame-generation-scene-boundary',
                   host_sha256=HOST_SHA,fg_enabled=False,fg_plugin_loaded=False,temporal_version='1.25.44',upscaling=True),
              dict(event='ownership_summary',image=1000),dict(event='frame_finished',owner=5),
              dict(event='scene_capture_requested',request=1,generation_checked=True)]
        inputs=[dict(event='continuous',scaled_route=dict(scene_boundary=valid()))]
        with patch('frame_generation_scene.ownership_review', return_value=dict(accepted=True)):
            result=review(rows,inputs,[], 'expected-digest')
        self.assertTrue(result['accepted'])
    def test_recording_and_present_counters_are_separate(self):
        r=scene_review(valid(),1000,5)['captures'][0]
        self.assertEqual(r['frame'],10)
        self.assertEqual(r['final_draw_native_frame'],11)
        self.assertEqual(r['present_native_frame'],11)
        for field,value in [('native_frame',0),('native_epoch',-1)]:
            t=valid();t['captures'][0]['observations'][0][field]=value
            with self.assertRaises(Exception):scene_review(t,1000,5)
        t=valid();t['captures'][0]['present_frame']=0
        with self.assertRaises(Exception):scene_review(t,1000,5)
    def test_copy_path_requires_order_and_native_return(self):
        r=scene_review(copy_trace(),1000,5,[80])['captures'][0]
        self.assertEqual(r['copies'],1)
        self.assertTrue(r['ordered_copy_reaches_sampled_GetBuffer'])
        self.assertFalse(r['current_backbuffer_verified'])
        t=copy_trace();obs=t['captures'][0]['observations']
        obs[1]['batch_index']=2;obs[2]['batch_index']=obs[3]['batch_index']=0
        self.assertFalse(scene_review(t,1000,5,[80])['captures'][0]['ordered_copy_reaches_sampled_GetBuffer'])
        for field,value in [('returned',False),('replaced',True)]:
            t=copy_trace();t['captures'][0]['observations'][-1][field]=value
            with self.assertRaises(Exception):scene_review(t,1000,5)
    def test_copy_corruption_and_unjustified_resource(self):
        for index,value in [(0,99),(1,0),(2,1),(9,2),(13,0)]:
            t=copy_trace();t['captures'][0]['observations'][-1]['copy'][index]=value
            with self.assertRaises(Exception):scene_review(t,1000,5)
        t=copy_trace();t['captures'][0]['followed_resources'].append(90)
        with self.assertRaises(Exception):scene_review(t,1000,5)
    def test_entry_snapshots_remain_incomplete_and_bounded(self):
        t=valid();f=t['captures'][0];f['outside_scope']=100;f['rejection_counts'][0]=100
        f['rejection_samples']=[sample(),sample()]
        r=scene_review(t,1000,5)['captures'][0]
        self.assertFalse(r['complete_direct_draw_scope'])
        self.assertFalse(r['rejection_snapshots_prove_draw_completion'])
        self.assertEqual(r['rejected_entry_samples'],2)
        f['rejection_samples'].append(sample())
        with self.assertRaises(Exception):scene_review(t,1000,5)
    def test_optional_entry_submission_requires_generation(self):
        t=valid();f=t['captures'][0];f['outside_scope']=1;f['rejection_counts'][0]=1
        e=sample();e.update(batch=1,queue=200,submit_qpc=5,batch_count=3)
        f['rejection_samples']=[e]
        with self.assertRaises(Exception):scene_review(t,1000,5)
        e['generation']=2
        self.assertFalse(scene_review(t,1000,5)['captures'][0]['UI_semantics_verified'])
    def test_recording_batch_locations_are_unique(self):
        t=valid();t['captures'][0]['observations'][1]['batch_index']=1
        with self.assertRaises(Exception):scene_review(t,1000,5)
    def test_copy_return_order_is_not_entry_order(self):
        t=copy_trace();f=t['captures'][0];e=dict(f['observations'][-1])
        e.update(ordinal=5,copy=[70,90,0,0,0,0,0,0,0,0,0,0,0,0,0,0])
        f['observations'].append(e);f['events']=5;f['followed_resources']=[70,90,80]
        self.assertEqual(scene_review(t,1000,5)['captures'][0]['followed_resources'],[70,90,80])

    def test_depth_attachment_and_range_flag_are_independent(self):
        e=sample(9)
        e['raster_observation']=[1,1,1,1,1,1,1,1,15,1,1,1,0,1,1,123,1,1,456]
        d=raster_metadata(e)
        self.assertTrue(d['depth_argument_present'])
        self.assertEqual(d['contiguous_range_raw'],0)
        self.assertEqual(d['missing_raster_fields'],[])
        # A range flag alone does not make the preserved renderer gate unknown.
        e['raster_observation']=[1,1,1,1,1,1,1,1,47,1,1,1,1,1,1,123,0,0,0]
        self.assertEqual(raster_metadata(e)['blockers'],[])

    def test_partial_raster_is_distinct_from_known_output(self):
        e=sample(9);e['raster_observation']=[1,1,1,1,1,1,1,1,46,0,1,1,0,1,1,123,0,0,0]
        d=raster_metadata(e)
        self.assertEqual(d['missing_raster_fields'],['vertex'])
        self.assertNotIn('single-RTV-output-unknown',d['blockers'])
        t=valid();f=t['captures'][0];f['unreadable']=1;f['rejection_counts'][9]=1;f['rejection_samples']=[e]
        r=scene_review(t,1000,5)['captures'][0]
        self.assertFalse(r['reason9_raster_observations'][0]['diagnostics_authorize_rendering'])
        self.assertFalse(r['UI_semantics_verified'])

    def test_read_bits_and_OM_consistency_reject_corruption(self):
        for field,value in [('native_cache_read_mask',0x800),('raster_observation',[0]*18)]:
            e=sample();e[field]=value
            with self.assertRaises(Exception):raster_metadata(e)
        e=sample();e['native_cache'][1]=1
        with self.assertRaises(Exception):raster_metadata(e)
        for index,value in ((16,1),(11,2),(14,0),(7,2)):
            e=valid()['captures'][0]['observations'][0];e['raster_observation'][index]=value
            with self.assertRaises(Exception):raster_metadata(e,qualified=True)

    def test_reason9_cannot_claim_a_complete_eligible_snapshot(self):
        t=valid();f=t['captures'][0];f['unreadable']=1;f['rejection_counts'][9]=1
        e=sample(9);e['raster_observation']=[1,1,1,1,1,1,1,1,47,1,1,1,0,1,1,123,0,0,0]
        f['rejection_samples']=[e]
        with self.assertRaises(Exception):scene_review(t,1000,5)


if __name__ == '__main__':
    unittest.main()
