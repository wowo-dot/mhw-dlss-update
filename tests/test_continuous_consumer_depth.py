from copy import deepcopy
from pathlib import Path
import json, sys, unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
from continuous_consumer_depth import review
FIXTURE = None

def status(t):
    return dict(consumer_depth=t, depth_copies=t['copies'], depth_eligibility=dict(lease=dict(checks=t['attempts'], rejected=t['rejected'])))

class ConsumerDepthTests(unittest.TestCase):
    def setUp(self):
        if FIXTURE is None: raise RuntimeError('Supply depth_consumer_tests JSONL')
        self.cases = {r['case']: deepcopy(r['trace']) for r in FIXTURE}
    def test_native_serialization(self):
        for t in self.cases.values(): review(status(t))
    def test_native_copy_branch_receipts(self):
        for name,index in [('direct-copy-opaque-child',1),('region-copy-opaque-child',2),('skipped-copy-opaque-child',3)]:
            t=self.cases[name];review(status(t));sample=t['samples'][0]
            self.assertEqual(sample['copy_paths'][index],1)
            self.assertEqual(sample['copy_paths'][4],1)
        t=self.cases['copy-child-draw-transition'];review(status(t))
        self.assertEqual(t['samples'][0]['copy_paths'][4],2)
    def test_copy_branch_corruptions(self):
        for mutate in (lambda s:s.update(copy_paths=[1,0,0,0,1]),lambda s:s.update(copy_paths=[0,1,0,0,0]),lambda s:s.update(copy_paths=[0,257,0,0,1])):
            t=deepcopy(self.cases['direct-copy-opaque-child']);mutate(t['samples'][0])
            with self.assertRaises(ValueError):review(status(t))
    def test_both_state_intervals(self):
        for name, state in [('render-target', 4), ('hoisted-outside-prior-32', 128), ('hoisted-at-consumer', 128)]:
            t = self.cases[name]; review(status(t)); self.assertEqual(t['samples'][0]['state'], state)
            self.assertEqual(t['copies'], 1)
    def test_rejected_proofs_never_copy(self):
        for t in self.cases.values():
            if t['rejected']:
                self.assertEqual(t['copies'], 0); self.assertGreaterEqual(t['rejected'], 1)
    def test_exact_failure_sites(self):
        for name,code in [('unreadable-node',21),('unqualified-node-opcode',31)]:
            t=self.cases[name];review(status(t));d=t['failure_diagnostics']
            self.assertEqual(d['samples'][0]['detail'][0],code)
            self.assertGreater(d['samples'][0]['detail'][3],0)
    def test_bounded_failures_and_corruption(self):
        t=self.cases['bounded-failure-telemetry'];review(status(t))
        self.assertEqual(t['failure_diagnostics']['count'],12)
        self.assertEqual([s['ordinal'] for s in t['failure_diagnostics']['samples']],[1,2,3,4,5,6,7,8,12])
        for kind in ('count','site','drop'):
            bad=deepcopy(t);d=bad['failure_diagnostics']
            if kind=='count':d['count']-=1
            elif kind=='site':d['samples'][0]['detail'][0]=999
            else:d['samples'].pop()
            with self.subTest(kind=kind),self.assertRaises(ValueError):review(status(bad))
    def test_general_read_states(self):
        for name, state in [('combined-read-after-consumer', 192), ('compute-read-after-consumer', 64),
                            ('inside-existing-read-span', 128), ('inside-combined-read-span', 192),
                            ('multiple-read-transitions', 192)]:
            t = self.cases[name]; review(status(t)); self.assertEqual(t['samples'][0]['state'], state)
            self.assertEqual(t['copies'], 1)
    def test_corrupt_transition_chain(self):
        for index, value in [(0, 29), (1, 34), (2, 30), (3, 12), (4, 256)]:
            t = deepcopy(self.cases['multiple-read-transitions']); t['samples'][0]['transitions'][2][index] = value
            with self.assertRaises(ValueError): review(status(t))
    def test_missing_transition(self):
        t = self.cases['multiple-read-transitions']; t['samples'][0]['transitions'].pop(1)
        with self.assertRaises(ValueError): review(status(t))
    def test_legacy_policy_remains_readable(self):
        t = self.cases['render-target']; t.update(schema=1, policy='scheduled-consumer-depth-copy-v1')
        t['hoisted'] = t.pop('read_state_copies')
        for row in t['samples']: row.pop('transitions')
        review(status(t))
    def test_counter_corruption(self):
        for k in ('attempts', 'accepted', 'copies', 'rejected', 'read_state_copies'):
            s = status(deepcopy(self.cases['render-target'])); s['consumer_depth'][k] += 1
            with self.assertRaises(ValueError): review(s)
    def test_bad_state_or_order(self):
        for k, v in [('indices', [2, 1, 59]), ('indices', [2, 560, 561]), ('state', 8), ('nodes', 0), ('stable', False)]:
            t = deepcopy(self.cases['render-target']); t['samples'][0][k] = v
            with self.assertRaises(ValueError): review(status(t))
    def test_old_writer_cannot_remain_enabled(self):
        t = self.cases['render-target']; t['writer_copy_disabled'] = False
        with self.assertRaises(ValueError): review(status(t))
    def test_bounded_and_immutable_evidence(self):
        t = self.cases['bounded-telemetry']; self.assertEqual(len(t['samples']), 9); previous = review(status(t))
        review(status(t), previous); t['samples'][0]['at'][0] += 1
        with self.assertRaises(ValueError): review(status(t), previous)
    def test_empty_before_first_attempt(self):
        t = self.cases['render-target']; t.update(attempts=0, accepted=0, rejected=0, copies=0, read_state_copies=0, reasons=[0]*12, samples=[])
        review(status(t))

if __name__ == '__main__':
    FIXTURE = [json.loads(l) for l in Path(sys.argv.pop(1)).read_text(encoding='utf-8-sig').splitlines()]
    unittest.main()
