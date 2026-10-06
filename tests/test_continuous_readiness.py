from copy import deepcopy
from pathlib import Path
import json,sys,unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from continuous_readiness import review
FIXTURE=None
def status(t):
    return dict(scaled_route=dict(readiness=t,claimed=t['accepted']),scaled_depth_capture=dict(readiness_rejected=t['rejected']))
class ReadinessTests(unittest.TestCase):
    def setUp(self):
        if FIXTURE is None:raise RuntimeError('Supply stream_readiness_tests JSONL')
        self.cases={r['case']:deepcopy(r['trace']) for r in FIXTURE}
    def test_native_serialization(self):
        for t in self.cases.values():review(status(t))
    def test_counters(self):
        for k in ('attempts','accepted','rejected'):
            t=deepcopy(self.cases['bounded']);t[k]+=1
            with self.assertRaises(ValueError):review(status(t))
    def test_cross_channel_agreement(self):
        s=status(self.cases['bounded']);s['scaled_route']['claimed']+=1
        with self.assertRaises(ValueError):review(s)
        s=status(self.cases['bounded']);s['scaled_depth_capture']['readiness_rejected']+=1
        with self.assertRaises(ValueError):review(s)
    def test_executed_receipt(self):
        t=self.cases['reason-7'];t['samples'][0]['plan'][6]=0
        with self.assertRaises(ValueError):review(status(t))
    def test_active_receipt(self):
        t=self.cases['reason-3'];t['samples'][0]['active'][1]=0
        with self.assertRaises(ValueError):review(status(t))
    def test_pipeline_receipts(self):
        for reason,key in ((9,'pipeline_missing_mask'),(10,'pipeline_signature_mask')):
            t=deepcopy(self.cases['reason-'+str(reason)]);t['samples'][0][key]=0
            with self.assertRaises(ValueError):review(status(t))
    def test_immutable_first_samples(self):
        t=self.cases['bounded'];previous=deepcopy(review(status(t)));review(status(t),previous)
        t['samples'][0]['at'][0]+=1
        with self.assertRaises(ValueError):review(status(t),previous)
    def test_immutable_baseline(self):
        t=self.cases['success-counts'];previous=deepcopy(review(status(t)));t['baseline']['at'][0]+=1
        with self.assertRaises(ValueError):review(status(t),previous)
    def test_bounds(self):
        for key,value in (('stage',5),('reason',13),('plan',[0]*11)):
            t=deepcopy(self.cases['bounded']);t['samples'][-1][key]=value
            with self.assertRaises(ValueError):review(status(t))
    def test_empty_and_success_no_rejections(self):
        for name in ('empty','success-counts'):
            t=self.cases[name];review(status(t));self.assertEqual(t['samples'],[])
if __name__=='__main__':
    FIXTURE=[json.loads(line) for line in Path(sys.argv.pop(1)).read_text(encoding='utf-8-sig').splitlines()]
    unittest.main()
