"""Native memory fixtures, historical replay, and independent parser checks."""
from copy import deepcopy
import json
from pathlib import Path
import subprocess
import sys
import unittest
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from continuous_admission import decision,review
from arm_continuous_dlaa import parse
from review_continuous_dlaa import review as review_run
FIXTURES={}

def setUpModule():
    p=subprocess.run([str(ROOT/'build/bin/Release/continuous_runtime_tests.exe'),'native-state-admission'],capture_output=True,text=True,check=True)
    FIXTURES.update((v['case'],v) for v in map(json.loads,p.stdout.splitlines()))

class AdmissionTests(unittest.TestCase):
    def setUp(self):self.s=deepcopy(FIXTURES['earlier-initialization'])
    def test_native_memory_fixtures(self):
        for name,s in FIXTURES.items():
            with self.subTest(name=name):
                a=review(s);self.assertEqual(a['accepted'],int(s['accepted']))
    def test_four_captured_cases(self):
        path=ROOT/'local/native-aa-state-provenance-20260927/v1.21.2/final-19116.status.json'
        for s in json.loads(path.read_text())['state_trace']['samples']:
            self.assertEqual(decision(s),0)
    def test_current_use_not_future_state(self):
        a=self.s['state_admission']['evidence']['samples'][0]
        a['provenance']['trackers'][0]['final']=65535;a['provenance']['trackers'][1]['final']=0
        self.assertEqual(review(self.s)['accepted'],1)
    def test_cannot_forge_acceptance(self):
        for name,s in FIXTURES.items():
            if s['accepted']:continue
            s=deepcopy(s);a=s['state_admission'];a['accepted']=1;a['rejected']=0;a['results'][0].update(reason=0,stable=True)
            if name in ('aliased-native-backing','aliased-earlier-backing','schedule-changed-on-reread','wrong-current-span'):
                # Fresh-read identity/stability is a runtime receipt; retained
                # evidence cannot independently reconstruct the later read.
                continue
            with self.subTest(name=name),self.assertRaises(ValueError):review(s)
    def test_missing_reread_is_not_accepted(self):
        self.s['state_admission']['results'][0]['stable']=False
        with self.assertRaises(ValueError):review(self.s)
    def test_counters(self):
        for k in ('attempts','accepted','rejected'):
            s=deepcopy(self.s);s['state_admission'][k]+=1
            with self.subTest(k=k),self.assertRaises(ValueError):review(s)
    def test_evidence_required(self):
        self.s['state_admission']['evidence']['samples']=[]
        with self.assertRaises(ValueError):review(self.s)
    def test_immutable_first_receipts(self):
        old=review(self.s);self.s['state_admission']['evidence']['samples'][0]['tick']+=1
        with self.assertRaises(ValueError):review(self.s,old)
    def test_new_header_requires_policies(self):
        path=ROOT/'local/native-aa-state-provenance-20260927/v1.21.2/inputs-19116-134349940773007435.jsonl'
        h=json.loads(path.read_bytes().splitlines()[0]);h['version']='1.21.3'
        with self.assertRaises(ValueError):parse((json.dumps(h)+'\n').encode())
        h.update(state_admission='native-current-use-v1',copy_telemetry='single-fence-observation-v1')
        self.assertEqual(parse((json.dumps(h)+'\n').encode())[0],h)
    def test_full_new_version_requires_admission_evidence(self):
        root=ROOT/'local/native-aa-state-provenance-20260927/v1.21.2'
        rows=[json.loads(x) for x in (root/'inputs-19116-134349940773007435.jsonl').read_bytes().splitlines()]
        startup=[json.loads(x) for x in (root/'startup-19116-134349940773007435.jsonl').read_bytes().splitlines()]
        rows[0].update(version='1.21.3',state_admission='native-current-use-v1',copy_telemetry='single-fence-observation-v1');startup[0]['version']='1.21.3'
        a=deepcopy(self.s['state_admission']);a.update(attempts=0,accepted=0,rejected=0,results=[])
        a['evidence'].update(total=0,output_rejections=0,temporal_rejections=0,omitted=0,samples=[])
        for s in rows[4:-1]:s['state_admission']=deepcopy(a)
        self.assertTrue(review_run(rows,startup,rows[0]['dll_sha256'])['state_admission']['native_contract_telemetry_verified'])
        del rows[-2]['state_admission']
        with self.assertRaises(KeyError):review_run(rows,startup,rows[0]['dll_sha256'])

if __name__=='__main__':unittest.main()
