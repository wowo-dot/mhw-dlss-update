"""Actual WARP/latch receipts plus corruption tests; no live-game claims."""
import copy,json,subprocess,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from continuous_reconstruction import review
from arm_continuous_dlaa import parse
from review_continuous_dlaa import review as review_run
REPO=Path(__file__).resolve().parents[1]
FIXTURE={}

def setUpModule():
    exe=REPO/'build/bin/Release/continuous_runtime_tests.exe'
    for mode in ('unfiltered-copy','reconstruction'):
        result=subprocess.run([str(exe),mode],capture_output=True,text=True,check=True)
        for line in result.stdout.splitlines():
            if line.startswith('{'):FIXTURE.update(json.loads(line))

class ReconstructionTests(unittest.TestCase):
    def setUp(self):
        self.s=dict(copy.deepcopy(FIXTURE),activation=dict(failure_latched=True),zero_jitter_projections=1,
                    jitter_injections=2,skips=0,dispatch_reasons=[0]*12,hooks_disabled=True)
    def test_actual_gpu_and_latch_receipts(self):
        r=review(self.s,final=True)
        self.assertTrue(r['feature_failed']);self.assertFalse(r['exclusive_aa_verified'])
        self.assertEqual(r['unfiltered_lifetime']['recordings']['released'],4)
        self.assertEqual(r['unfiltered_lifetime']['signal_value'],2)
    def test_failure_cannot_resume(self):
        self.s['reconstruction'].update(waiting=False,resumptions=2)
        with self.assertRaises(ValueError):review(self.s)
    def test_failure_cannot_clear(self):
        prior=review(self.s);self.s['reconstruction']['failure']=0;self.s['activation']['failure_latched']=False
        with self.assertRaises(ValueError):review(self.s,prior)
    def test_latch_agreement(self):
        self.s['activation']['failure_latched']=False
        with self.assertRaises(ValueError):review(self.s)
    def test_lifetime_corruption(self):
        for k in self.s['unfiltered_lifetime']['recordings']:
            s=copy.deepcopy(self.s);s['unfiltered_lifetime']['recordings'][k]=None
            with self.subTest(k=k),self.assertRaises(ValueError):review(s)
    def test_device_removed_is_not_completion(self):
        self.s['unfiltered_lifetime']['completed_value']=2**64-1
        with self.assertRaises(ValueError):review(self.s)
    def test_premature_release(self):
        self.s['unfiltered_lifetime']['recordings']['retired']=3
        with self.assertRaises(ValueError):review(self.s)
    def test_counters_and_reasons(self):
        for key,value in (('zero_jitter_projections',True),('dispatch_reasons',[0]*11),('dispatch_reasons',[1]*12)):
            s=copy.deepcopy(self.s);s[key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):review(s)
    def test_full_synthetic_version(self):
        root=REPO/'local/native-aa-ownership-20260927/v1.20.2'
        rows=[json.loads(line) for line in (root/'inputs-56980-134349443685469575.jsonl').read_text().splitlines()]
        startup=[json.loads(line) for line in (root/'startup-56980-134349443685469575.jsonl').read_text().splitlines()]
        rows[0].update(version='1.21.0',reconstruction='suspend-or-latch-unfiltered-v1');startup[0]['version']='1.21.0'
        for s in rows[4:-1]:
            # Synthetic controller receipts layered on prior GPU evidence.
            s.update({k:copy.deepcopy(v) for k,v in self.s.items() if k not in ('activation','jitter_injections','skips','hooks_disabled')})
            s['reconstruction']['failure']=0;s['activation'].update(failure_latched=False,queue_unfiltered=0)
            s['zero_jitter_projections']=0
        self.assertTrue(review_run(rows,startup,rows[0]['dll_sha256'])['accepted'])
        del rows[0]['reconstruction']
        with self.assertRaises(ValueError):parse((json.dumps(rows[0])+'\n').encode())

if __name__=='__main__':unittest.main()
