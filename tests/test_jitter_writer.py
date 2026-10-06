import copy
import json
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from review_jitter_writer import HOST_SHA,parse,review
from arm_jitter_writer import validate,event_name

def fixture():
    base=0x140000000
    before=dict(name=base+0x3344478,owner=0x90000000,type=base+0x351bda8,target=0x90000184,flags=3,index=0,value=1,failure=0)
    return [dict(event='start',schema=1,version='1.0.0',host_version='0.0.0.0',host_sha256=HOST_SHA,
                 experiment='typed-jitter-property-writer',provider_substitution=False,record_capacity=64,
                 window_ms=180000,visit_limit=1000000,pid=123,creation=456,base=base,dll_sha256='a'*64),
            dict(event='ready',hooks_installed=False),
            dict(event='armed',tick=1000,live_spans=3,d3d12_module_loaded=True),
            dict(event='write',token=1,tick=1100,exit_tick=1101,entry_order=1,exit_order=2,descriptor=0x80000000,
                 caller=base+0x221d5ed,input=0,thread=10,complete=True,before=before,after=before|{'value':0}),
            dict(event='capture_complete',records=1,reserved=1,pending=0,stop=2,visits=20,named=3,other_type=2,dropped=0,read_failures=0),
            dict(event='stopped',fatal=False,hooks_disabled=True,pending=0)]

class WriterReviewTests(unittest.TestCase):
    def test_observed_transition(self):
        r=review(parse((json.dumps(fixture()[0])+'\n'+'\n'.join(json.dumps(x) for x in fixture()[1:])+'\n').encode()))
        self.assertTrue(r['accepted_cpu_capture']); self.assertEqual(r['recorded_one_to_zero'],1)
        self.assertFalse(r['gpu_qualified'])
    def test_empty_is_not_writer_proof(self):
        r=fixture();del r[3];r[-2].update(records=0,reserved=0,named=2)
        a=review(r);self.assertFalse(a['accepted_cpu_capture']);self.assertTrue(a['valid_empty_observation'])
    def test_wrong_target_identity(self):
        for key,value in [('name',1),('type',1),('target',1),('flags',0x20003),('index',1)]:
            with self.subTest(key=key):
                r=fixture();r[3]['before'][key]=value
                with self.assertRaises(ValueError):review(r)
    def test_partial_duplicate_and_wrong_header(self):
        raw=('\n'.join(json.dumps(x) for x in fixture())+'\n').encode()
        with self.assertRaises(ValueError):parse(raw[:-1])
        with self.assertRaises(ValueError):parse(raw.replace(b'"schema": 1',b'"schema": 1, "schema": 1'))
        with self.assertRaises(ValueError):parse(raw.replace(HOST_SHA.encode(),b'0'*64))
    def test_incomplete_or_unstopped(self):
        for mode in ('pending','missing','incomplete','disable_failed'):
            r=fixture()
            if mode=='pending':r[-2]['pending']=1
            if mode=='missing':r.pop(3)
            if mode=='incomplete':r[3]['complete']=False
            if mode=='disable_failed':
                r[-1]['hooks_disabled']=False
                self.assertFalse(review(r)['accepted_cpu_capture']);continue
            with self.assertRaises(ValueError):review(r)
    def test_invalid_samples_retained_without_acceptance(self):
        r=fixture();r[3]['before']['failure']=3;r[-2]['stop']=6;r[-1]['fatal']=True
        self.assertFalse(review(r)['accepted_cpu_capture'])
    def test_after_mismatch_does_not_invent_effect(self):
        r=fixture();r[3]['after']['value']=1
        a=review(r);self.assertEqual(a['recorded_one_to_zero'],0);self.assertEqual(a['after_matches_input'],0)
    def test_overlap_and_duplicate_order(self):
        r=fixture();b=copy.deepcopy(r[3]);b.update(token=2,thread=11,entry_order=2,exit_order=3)
        r[3]['exit_order']=4;r.insert(4,b);r[-2].update(records=2,reserved=2,named=4)
        self.assertEqual(review(r)['calls_overlapping_same_target'],2)
        r[4]['entry_order']=1
        with self.assertRaises(ValueError):review(r)
    def test_generation_and_replay_rejected(self):
        r=fixture();validate(r[:2],1,123,456)
        for args in ((r[:2],1,123,999),(r[:3],1,123,456),(r,0,123,456),(r[:2],0,123,456)):
            with self.assertRaises(ValueError):validate(*args)
        validate(r[:3],0,123,456)
    def test_expired_and_unarmed(self):
        r=fixture();r[3]['tick']=181000;r[3]['exit_tick']=181001
        with self.assertRaises(ValueError):review(r)
        r=fixture();r.pop(2)
        with self.assertRaises(ValueError):review(r)

def track_fixture():
    r=fixture();r[0].update(schema=2,version='1.1.0');r[2]['live_spans']=5
    source=dict(applicable=True,record=r[3]['descriptor']-0x10,header=0xa0000000,
                owner=r[3]['before']['owner'],keys=0xb0000000,values=0xb0000100,
                kind=11,count=3,index=1,key_word=20,raw_value=2,failure=0)
    r[3].update(track_before=copy.deepcopy(source),track_after=copy.deepcopy(source))
    return r

class TrackReviewTests(unittest.TestCase):
    def test_selected_nonzero_even_source(self):
        rows=track_fixture()
        a=review(parse(('\n'.join(json.dumps(x) for x in rows)+'\n').encode()))
        self.assertTrue(a['accepted_source_context']);self.assertEqual(a['source_values'],{2:1})
        self.assertEqual(a['matching_track_pairs'],1);self.assertEqual(a['selected_keys'][0]['index'],1)
        self.assertFalse(a['gpu_qualified'])
    def test_new_code_gates_and_version_required(self):
        r=track_fixture();r[2]['live_spans']=3
        with self.assertRaises(ValueError):review(r)
        r=track_fixture();r[0]['version']='1.0.0'
        with self.assertRaises(ValueError):parse(('\n'.join(json.dumps(x) for x in r)+'\n').encode())
    def test_track_structure_and_bounds(self):
        for key,value in [('record',1),('header',0),('owner',1),('kind',3),('count',0),('count',65536),
                          ('index',3),('keys',2**64-1),('values',2**64-1),('raw_value',256),('key_word',2**32)]:
            with self.subTest(key=key,value=value):
                r=track_fixture();r[3]['track_before'][key]=value
                with self.assertRaises(ValueError):review(r)
    def test_failed_source_kept_without_acceptance(self):
        r=track_fixture();r[3]['track_before']['failure']=1;r[-2]['stop']=6;r[-1]['fatal']=True
        a=review(r);self.assertFalse(a['accepted_source_context']);self.assertFalse(a['accepted_cpu_capture'])
        self.assertEqual(a['invalid_track_snapshots'],1)
    def test_changed_source_is_not_stable_context(self):
        r=track_fixture();r[3]['track_after']['raw_value']=3
        a=review(r);self.assertTrue(a['accepted_cpu_capture']);self.assertFalse(a['accepted_source_context'])
        self.assertEqual(a['track_context_changes'],1);self.assertEqual(a['track_input_mismatches'],1)
    def test_consistent_source_not_matching_input(self):
        r=track_fixture();r[3]['input']=1;r[3]['after']['value']=1
        a=review(r);self.assertTrue(a['accepted_cpu_capture']);self.assertFalse(a['accepted_source_context'])
        self.assertEqual(a['track_context_changes'],0);self.assertEqual(a['track_input_mismatches'],1)
    def test_foreign_route_cannot_claim_source_context(self):
        r=track_fixture();r[3]['caller']+=1
        with self.assertRaises(ValueError):review(r)
        for key in ('track_before','track_after'):
            r[3][key]={k:(False if k=='applicable' else 0) for k in r[3][key]}
        a=review(r);self.assertTrue(a['accepted_cpu_capture']);self.assertFalse(a['accepted_source_context'])
        self.assertEqual(a['track_calls'],0)
    def test_overlap_prevents_source_acceptance(self):
        r=track_fixture();b=copy.deepcopy(r[3]);b.update(token=2,thread=11,entry_order=2,exit_order=3)
        r[3]['exit_order']=4;r.insert(4,b);r[-2].update(records=2,reserved=2,named=4)
        a=review(r);self.assertEqual(a['matching_track_pairs'],2);self.assertFalse(a['accepted_source_context'])
    def test_distinct_versioned_control_events(self):
        self.assertEqual(event_name(fixture()[0],1),'Local\\MhwJitterWriter-v1-123-456-1')
        self.assertEqual(event_name(track_fixture()[0],0),'Local\\MhwJitterWriter-v2-123-456-0')

if __name__=='__main__':unittest.main()
