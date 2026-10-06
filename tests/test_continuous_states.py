"""Native trace receipts, independent barrier replay and parser corruption tests."""
import copy,json,subprocess,sys,unittest
from pathlib import Path
REPO=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(REPO/'tools'))
from continuous_states import review
from arm_continuous_dlaa import parse
from review_continuous_dlaa import review as review_run
FIXTURES={}
def setUpModule():
    p=subprocess.run([str(REPO/'build/bin/Release/continuous_runtime_tests.exe'),'state-trace'],capture_output=True,text=True,check=True)
    for line in p.stdout.splitlines():
        if line.startswith('{'):
            v=json.loads(line);t=v['state_trace'];v.update(dispatch_reasons=[0]*12,resolve=dict(unclaimed_native=t['output_rejections'],unfiltered=t['temporal_rejections'],pending=0));v['dispatch_reasons'][6]=t['total'];FIXTURES[v['case']]=v

class StateTraceTests(unittest.TestCase):
    def setUp(self):self.s=copy.deepcopy(FIXTURES['missing-transition'])
    def test_all_native_receipts(self):
        for name,s in FIXTURES.items():
            with self.subTest(name=name):self.assertTrue(review(s)['state_trace_verified'])
    def test_uav_does_not_become_transition(self):
        r=review(self.s);self.assertEqual(r['replayed_samples'],9);self.assertEqual(r['omitted'],11)
        for s in r['samples']:
            self.assertEqual(s['resources'][1]['proof'],[1,0,0,2**32-1])
            self.assertEqual(s['native'][0][1:],[2,1024,1024])
    def test_incomplete_trace_not_replayed(self):
        self.assertEqual(review(FIXTURES['capped-barriers'])['replayed_samples'],0)
        self.assertFalse(review(FIXTURES['cyclic-native'])['samples'][0]['native_complete'])
    def test_corrupt_proof_rejected(self):
        for role in range(3):
            s=copy.deepcopy(self.s);s['state_trace']['samples'][0]['resources'][role]['proof'][1]+=1
            with self.subTest(role=role),self.assertRaises(ValueError):review(s)
    def test_corrupt_barrier_rejected(self):
        self.s['state_trace']['samples'][0]['barriers'][0][5]=0xc0
        with self.assertRaises(ValueError):review(self.s)
    def test_ordinal_cap_and_omission(self):
        for k,value in (('first_capacity',9),('barrier_capacity',32),('omitted',10),('total',21)):
            s=copy.deepcopy(self.s);s['state_trace'][k]=value
            with self.subTest(k=k),self.assertRaises(ValueError):review(s)
        self.s['state_trace']['samples'][-1]['ordinal']=19
        with self.assertRaises(ValueError):review(self.s)
    def test_counter_types(self):
        for value in (True,-1,2**64,None):
            s=copy.deepcopy(self.s);s['state_trace']['total']=value
            with self.subTest(value=value),self.assertRaises(ValueError):review(s)
    def test_resource_identity_and_native_bounds(self):
        self.s['state_trace']['samples'][0]['native_nodes']=129
        with self.assertRaises(ValueError):review(self.s)
        self.setUp();self.s['state_trace']['samples'][0]['resources'][0]['resource']=0
        with self.assertRaises(ValueError):review(self.s)
    def test_stage_coverage(self):
        self.s['resolve']['unclaimed_native']-=1
        with self.assertRaises(ValueError):review(self.s)
        self.setUp();self.s['state_trace']['samples'][0]['temporal']=True
        with self.assertRaises(ValueError):review(self.s)
    def test_first_samples_immutable(self):
        old=review(self.s);self.s['state_trace']['samples'][0]['tick']+=1
        with self.assertRaises(ValueError):review(self.s,old)
    def test_full_synthetic_version(self):
        # Prior v1.21.0 live evidence with synthetic trace metadata only.
        root=REPO/'local/native-aa-unfiltered-20260927/v1.21.0'
        rows=[json.loads(x) for x in (root/'inputs-49076-134349464479679036.jsonl').read_text().splitlines()]
        startup=[json.loads(x) for x in (root/'startup-49076-134349464479679036.jsonl').read_text().splitlines()]
        rows[0].update(version='1.21.2',state_trace='consumer-local-state-v1',native_state_trace='scheduled-state-evidence-v1');startup[0]['version']='1.21.2'
        for s in rows[4:-1]:
            n=s['dispatch_reasons'][6];t=copy.deepcopy(self.s['state_trace']);t.update(total=n,output_rejections=n,temporal_rejections=0,omitted=max(0,n-9));t['samples']=t['samples'][:n];s['state_trace']=t
        self.assertTrue(review_run(rows,startup,rows[0]['dll_sha256'])['state_trace']['state_trace_verified'])
        del rows[-2]['state_trace']
        with self.assertRaises(KeyError):review_run(rows,startup,rows[0]['dll_sha256'])
        del rows[0]['state_trace']
        with self.assertRaises(ValueError):parse((json.dumps(rows[0])+'\n').encode())

if __name__=='__main__':unittest.main()
