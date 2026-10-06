"""Review real owned callback receipts and reject corrupt native retirement claims."""
import argparse
import copy
import json
from pathlib import Path
import subprocess
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from continuous_forwarded import review, COUNTERS
from continuous_requests import review as review_requests
from review_continuous_dlaa import review as review_run
from arm_continuous_dlaa import parse
import test_continuous_resolve as resolve_fixture

FIXTURES={}
MODES=('wrap','overlap','early-reuse','stop','exception','changed-before','changed-after',
       'stale-token','ambiguous','unmatched','reentrant')

class ForwardedTests(unittest.TestCase):
    def setUp(self): self.s=copy.deepcopy(FIXTURES['wrap'])
    def test_actual_native_receipts(self):
        for mode,s in FIXTURES.items():
            with self.subTest(mode=mode):
                r=review(s,final=True);review_requests(s['requests'],s['fault'])
                self.assertEqual(r['pending'],0)
                self.assertEqual(s['native_calls'],2 if mode=='reentrant' else 1)
    def test_success_and_held_request(self):
        for mode in ('wrap','overlap'):
            r=review(FIXTURES[mode],final=True)
            self.assertEqual(r['retired'],1);self.assertEqual(FIXTURES[mode]['fault'],0)
        self.assertEqual(FIXTURES['early-reuse']['fault'],6)
    def test_failed_native_return_is_not_retirement(self):
        for mode in ('exception','changed-after','stale-token'):
            r=review(FIXTURES[mode],final=True)
            self.assertEqual(r['retired'],0);self.assertEqual(r['failed'],1)
            self.assertFalse(r['native_forwarded_retirements_verified'])
    def test_unproven_identity_keeps_old_request(self):
        for mode in ('changed-before','ambiguous','unmatched'):
            r=review(FIXTURES[mode],final=True)
            self.assertEqual(r['started'],0);self.assertEqual(FIXTURES[mode]['fault'],6)
    def test_counter_types(self):
        for k in COUNTERS:
            for v in (True,-1,2**64,None):
                s=copy.deepcopy(self.s);s['forwarded_requests'][k]=v
                with self.subTest(key=k,value=v),self.assertRaises(ValueError):review(s)
    def test_counter_partitions(self):
        for k in COUNTERS:
            s=copy.deepcopy(self.s);s['forwarded_requests'][k]+=1
            with self.subTest(key=k),self.assertRaises(ValueError):review(s)
    def test_pending_only_allowed_live(self):
        m=self.s['forwarded_requests'];m['retired']=0;m['pending']=1
        m['first_retired']=m['last_retired']=None
        review(self.s)
        with self.assertRaises(ValueError):review(self.s,final=True)
    def test_fault_required_for_failed_return(self):
        s=copy.deepcopy(FIXTURES['exception']);s['fault']=0
        with self.assertRaises(ValueError):review(s)
    def test_receipt_identity_and_count(self):
        for k,v in (('frame',0),('sequence',0),('bypasses',0),('last_bypass_publishers',0)):
            s=copy.deepcopy(self.s);s['forwarded_requests']['first_retired'][k]=v
            with self.subTest(key=k),self.assertRaises(ValueError):review(s)
    def test_policy_and_coverage(self):
        self.s['forwarded_requests']['policy']='pointer-only'
        with self.assertRaises(ValueError):review(self.s)
        self.s=copy.deepcopy(FIXTURES['wrap']);self.s['requests']['taa']['blocked']+=1
        with self.assertRaises(ValueError):review(self.s)
    def test_rollback(self):
        p=copy.deepcopy(self.s['forwarded_requests']);p['retired']+=1
        with self.assertRaises(ValueError):review(self.s,p)
    def synthetic(self):
        rows,startup,dll=resolve_fixture.fixture()
        rows[0]['version']=startup[0]['version']='1.19.1'
        rows[0]['request_retirement']='validated-native-return-v1'
        for s in rows[4:-1]:
            n=s['requests']['taa']['blocked']
            s['forwarded_requests']=dict(schema=1,policy='validated-native-return-v1',candidates=n,
                unmatched=n,ambiguous=0,invalid=0,started=0,pending=0,retired=0,failed=0,
                first_retired=None,last_retired=None)
        return rows,startup,dll
    def test_versioned_full_review(self):
        r=review_run(*self.synthetic())
        self.assertTrue(r['accepted']);self.assertIn('forwarded_requests',r)
    def test_version_requires_policy_and_receipts(self):
        rows,startup,dll=self.synthetic();del rows[0]['request_retirement']
        with self.assertRaises(ValueError):parse((json.dumps(rows[0])+'\n').encode())
        rows,startup,dll=self.synthetic();del rows[-2]['forwarded_requests']
        with self.assertRaises(KeyError):review_run(rows,startup,dll)

if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('--native',type=Path,required=True)
    ap.add_argument('--output',type=Path,required=True);a,rest=ap.parse_known_args()
    a.output.mkdir(parents=True,exist_ok=True)
    for mode in MODES:
        raw=subprocess.run([str(a.native.resolve()),'forwarded-'+mode],check=True,capture_output=True).stdout
        FIXTURES[mode]=json.loads(raw)
        with (a.output/(mode+'.json')).open('xb') as out:out.write(raw)
    unittest.main(argv=[sys.argv[0],*rest])
