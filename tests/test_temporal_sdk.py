"""Adversarial SDK receipt review using the real borrowed-device fixture."""
import copy
import json
import struct
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
from temporal_sdk import review_sdk_events


class SdkReviewTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.rows = [json.loads(s) for s in (fixture/'events.jsonl').read_text().splitlines()]
        start = cls.rows[0]
        cls.n = 9 if start['version']==5 else 3
        cls.capture = {k: start[k] for k in ('width','height','device','adapter_luid_low','adapter_luid_high')}
        # Synthetic outer metadata: real isolated GPU identities, no claim of
        # game/native producer observation in this unit fixture.
        cls.capture['owned_gpu_inputs'] = dict(sets=[
            dict(outputs=r['inputs'], inputs=[], readbacks=[], objects=[100+i*4,101+i*4,102+i*4,r['producer_fence']],
                 fence_value=r['producer_value'], completed=True, allocator_retired=True)
            for i,r in enumerate(cls.rows[1:cls.n+1])])

        if start['version'] in (4,5):
            # Explicitly synthetic frame/projection metadata from the fixture's
            # declared plan, not observed game state.
            zero=fixture.name.endswith('-zero')
            cls.capture['records']=[dict(frame=25383+i,epoch=25383+i) for i in range(cls.n)]
            cls.capture['projection_stimulus']=dict(records=[dict(frame=25383+i,epoch=25383+i,
                jitter_bits=list(struct.unpack('<II',struct.pack('<ff',.375 if 0<i<cls.n-1 and not zero else 0,-.4375 if 0<i<cls.n-1 and not zero else 0)))) for i in range(cls.n)])

    def test_real_sdk_events(self):
        review_sdk_events(self.rows, self.capture)

    def test_frame_count(self):
        for count in (None,False,0,4,8,10):
            rows=copy.deepcopy(self.rows);rows[0]['frame_count']=count
            with self.subTest(count=count),self.assertRaises(ValueError):review_sdk_events(rows,self.capture)
        if self.n==9:
            rows=copy.deepcopy(self.rows);del rows[0]['frame_count']
            with self.assertRaises(ValueError):review_sdk_events(rows,self.capture)

    def test_capability_values(self):
        rows=copy.deepcopy(self.rows)
        if rows[0]['version']<2:rows[0]['version']=2
        api=[r for r in rows if r['event']=='api']
        caps=[r for r in api if r['name'] in ('Available','NeedsUpdatedDriver','FeatureInitResult')]
        for r,v in zip(caps,(1,0,1)):r.update(value=v,value_u32=v)
        review_sdk_events(rows,self.capture)
        for name in ('Available','NeedsUpdatedDriver','FeatureInitResult'):
            for key in ('value','value_u32'):
                for bad in (None,False,2,0xbad00001):
                    changed=copy.deepcopy(rows)
                    next(r for r in changed if r.get('name')==name)[key]=bad
                    with self.subTest(name=name,key=key,bad=bad),self.assertRaises(ValueError):
                        review_sdk_events(changed,self.capture)

    def test_missing_capability_value(self):
        rows=copy.deepcopy(self.rows)
        if rows[0]['version']<2:rows[0]['version']=2
        for r in rows:
            r.pop('value',None);r.pop('value_u32',None)
        with self.assertRaises(ValueError):review_sdk_events(rows,self.capture)

    def test_changed_contract(self):
        for key,value in [('borrowed_device',False),('device',1),('width',1),('runtime_sha256','a'*64),
                          ('reset_every_frame',self.rows[0]['version'] in (4,5)),('mv_scale',[0,0]),('create_flags',0),
                          ('device_debug_settings_changed',True),('presentation_changed',True)]:
            rows=copy.deepcopy(self.rows);rows[0][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):review_sdk_events(rows,self.capture)

    def test_producer(self):
        for key,value in [('producer_fence',1),('producer_value',0),('producer_retired',False),('inputs',[1,2,3]),('input_state',64)]:
            rows=copy.deepcopy(self.rows);rows[1][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):review_sdk_events(rows,self.capture)

    def test_api_failure(self):
        for i,r in enumerate(self.rows):
            if r['event']=='api':
                rows=copy.deepcopy(self.rows);rows[i]['result']=0xbad00000
                with self.subTest(api=r['name']),self.assertRaises(ValueError):review_sdk_events(rows,self.capture)

    def test_completion_failure(self):
        for i,r in enumerate(self.rows):
            if r['event']=='completed':
                rows=copy.deepcopy(self.rows);rows[i]['recording_retired']=False
                with self.subTest(index=i),self.assertRaises(ValueError):review_sdk_events(rows,self.capture)

    def test_unexpected_shutdown(self):
        rows=copy.deepcopy(self.rows);rows.insert(-1,dict(event='api',name='Shutdown1',result=1))
        with self.assertRaises(ValueError):review_sdk_events(rows,self.capture)

    def test_incomplete_cleanup(self):
        for key in ('feature_released','parameters_destroyed','shutdown_deferred','context_retained','legacy_unchanged'):
            rows=copy.deepcopy(self.rows);rows[-1][key]=False
            with self.subTest(key=key),self.assertRaises(ValueError):review_sdk_events(rows,self.capture)

    def test_native_alias(self):
        rows=copy.deepcopy(self.rows);rows[self.n+1]['objects'][0]=rows[1]['inputs'][0]
        with self.assertRaises(ValueError):review_sdk_events(rows,self.capture)

    def test_bad_output_receipt(self):
        for i,r in enumerate(self.rows):
            if r['event']=='sdk_output':
                rows=copy.deepcopy(self.rows);rows[i]['inputs_restored']=False
                with self.subTest(index=i),self.assertRaises(ValueError):review_sdk_events(rows,self.capture)


    def test_startup_ownership(self):
        if self.rows[0]['version'] not in (3,4,5):self.skipTest('startup fixture required')
        for key,value in [('device',1),('module',1),('ownership_verified',False),('initialization_calls',1),('initialization_calls',False)]:
            rows=copy.deepcopy(self.rows);rows[self.n+2][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):review_sdk_events(rows,self.capture)

    def test_startup_cannot_reinitialize(self):
        if self.rows[0]['version'] not in (3,4,5):self.skipTest('startup fixture required')
        rows=copy.deepcopy(self.rows);rows[self.n+2]=dict(event='api',name='Init_with_ProjectID',result=1)
        with self.assertRaises(ValueError):review_sdk_events(rows,self.capture)

    def test_temporal_rejections(self):
        if self.rows[0]['version'] not in (4,5):self.skipTest('history fixture required')
        for i in (self.n+10+4*f for f in range(self.n)):
            for key,bad in [('frame',0),('epoch',0),('receipt',9),('reset',not self.rows[i]['reset']),
                            ('jitter',[float('nan'),0]),('jitter',[.501,0]),('mv_jittered',True),
                            ('frame_ms',float('nan')),('frame_ms',20),('time_policy','measured')]:
                rows=copy.deepcopy(self.rows);rows[i][key]=bad
                with self.subTest(i=i,key=key),self.assertRaises(ValueError):review_sdk_events(rows,self.capture)
        stimulus=self.n+14
        for jitter in ([0,0],[-.1875,-.21875],[.375,.4375],[.1875,-.21875]):
            if jitter==self.rows[stimulus]['jitter']:continue
            rows=copy.deepcopy(self.rows);rows[stimulus]['jitter']=jitter
            with self.subTest(jitter=jitter),self.assertRaises(ValueError):review_sdk_events(rows,self.capture)
        for offset in (stimulus-1,stimulus+1):
            rows=copy.deepcopy(self.rows);r=rows.pop(stimulus);rows.insert(offset,r)
            with self.assertRaises(ValueError):review_sdk_events(rows,self.capture)
        cap=copy.deepcopy(self.capture);cap['records'][1]['epoch']+=1
        with self.assertRaises(ValueError):review_sdk_events(self.rows,cap)

if __name__=='__main__':
    fixture=Path(sys.argv.pop(1))
    unittest.main()
