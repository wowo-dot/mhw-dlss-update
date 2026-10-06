import json,sys,unittest
from copy import deepcopy
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from continuous_output_diagnostics import review
FIXTURE=None
def status(t):return dict(scaled_route=dict(diagnostics=t,readiness=dict(reasons=[0]*7+[t['order_count']]+[0]*5)))
class OutputDiagnosticsTests(unittest.TestCase):
    def setUp(self):
        if FIXTURE is None:raise RuntimeError('Supply output_diagnostics_tests JSONL')
        self.cases={x['case']:deepcopy(x['trace']) for x in FIXTURE}
    def test_actual_native_serialization(self):
        for t in self.cases.values():review(status(t))
    def test_command_repeat_and_truncation(self):
        before=self.cases['commands-truncated'];after=self.cases['commands-repeat']
        review(status(after),before)
        s=after['commands']['samples'][0]
        self.assertEqual(s['occurrences'],2)
        self.assertEqual(len(s['packets']),32)
        self.assertEqual(s['focus']['index'],39)
        self.assertEqual(s['opcodes'][13],1)
    def test_duplicate_packet_gate(self):
        from continuous_output_commands import gate
        gate([11,0x78001004,1234,7,0,1,1,1],17)
        # A duplicate may cross distinct descriptor keys, so seen can be zero.
        gate([11,0x78001004,1234,7,0,1,0,1],17)
        for index,value in ((0,12),(2,0),(3,65535),(4,13),(5,0),(7,0)):
            g=[11,0x78001004,1234,7,0,1,1,1];g[index]=value
            with self.assertRaises(ValueError):gate(g,17)
    def test_command_receipt_corruption(self):
        for field,value in [('matched',39),('scanned',41),('error',4),('stable',1),('occurrences',0)]:
            t=deepcopy(self.cases['commands-truncated']);t['commands']['samples'][0][field]=value
            with self.assertRaises(ValueError):review(status(t))
        t=deepcopy(self.cases['commands-truncated']);t['commands']['samples'][0]['focus']['index']=38
        with self.assertRaises(ValueError):review(status(t))
        t=deepcopy(self.cases['commands-unreadable']);t['commands']['samples'][0]['packets'][0]['raw']=1
        with self.assertRaises(ValueError):review(status(t))
    def test_command_immutable_sample_and_saturation(self):
        before=self.cases['commands-truncated'];t=deepcopy(self.cases['commands-repeat'])
        t['commands']['samples'][0]['at'][0]+=1
        with self.assertRaises(ValueError):review(status(t),before)
        t=deepcopy(self.cases['commands-capacity']);review(status(t))
        self.assertEqual(len(t['commands']['samples']),16)
        self.assertGreater(t['commands']['overflow'],0)
        t['commands']['samples'].pop()
        with self.assertRaises(ValueError):review(status(t))
    def test_previous_and_late_submission(self):
        before=self.cases['prior-submission'];o=before['orders'][0]
        self.assertEqual(o['submissions'][2],[12,2,500,1,1,2,102])
        self.assertEqual(self.cases['wrong-generation']['orders'][0]['submissions'][1],[12,3,0,0,0,0,0])
        final=self.cases['late-callback'];review(status(final),before)
        self.assertEqual(final['orders'][0]['submissions'][0],[11,1,500,2,0,3,105])
        self.assertEqual(final['orders'][0]['submissions'][3],[13,4,500,2,2,3,105])
    def test_history_wrap_does_not_invent_receipt(self):
        t=self.cases['wrapped'];self.assertEqual([x['ordinal'] for x in t['orders']],[1,2,3,4,11])
        self.assertEqual(t['orders'][-1]['submissions'][0],[11,1,0,0,0,0,0])
        review(status(t),self.cases['late-callback'])
    def test_wrong_generation_receipt(self):
        t=self.cases['late-callback'];t['orders'][0]['submissions'][1][1]+=1
        with self.assertRaises(ValueError):review(status(t))
    def test_bounds_and_missing_receipts(self):
        for field,value in [('count',17),('link_reads',16),('reason',19)]:
            t=deepcopy(self.cases['graph']);t['graphs'][0][field]=value
            with self.assertRaises(ValueError):review(status(t))
        t=self.cases['wrapped'];t['orders'].pop()
        with self.assertRaises(ValueError):review(status(t))
    def test_immutable_graphs_and_submissions(self):
        for name in ('graph','late-callback'):
            t=self.cases[name];previous=deepcopy(t)
            if name=='graph':t['graphs'][0]['at'][0]+=1
            else:t['orders'][0]['submissions'][0][6]+=1
            with self.assertRaises(ValueError):review(status(t),previous)
    def test_admission_not_changed_by_unknown_submission(self):
        review(status(self.cases['wrong-generation']))
    def test_counter_agreement(self):
        t=self.cases['graph'];t['graph_attempts']+=1
        with self.assertRaises(ValueError):review(status(t))
        s=status(self.cases['wrapped']);s['scaled_route']['readiness']['reasons'][7]+=1
        with self.assertRaises(ValueError):review(s)
    def test_geometry_qualification_bounds(self):
        for value in (0,(4<<16)|3,(11<<16)|11):
            t=deepcopy(self.cases['graph']);t['graphs'][0]['qualification']=value
            t['graphs'][0].pop('vertex_gate',None) # Legacy logs predate operand receipts.
            review(status(t))
        for value in (True,-1,4<<16,(12<<16)|1,14):
            t=deepcopy(self.cases['graph']);t['graphs'][0]['qualification']=value
            with self.assertRaises(ValueError):review(status(t))
        t=deepcopy(self.cases['graph']);t['graph_baseline']['qualification']=(4<<16)|3
        with self.assertRaises(ValueError):review(status(t))
    def test_dependency_qualifications_match_producer(self):
        for name,code,branch in (('commands-dependency-discovery',12,10),('commands-dependency-order',13,8)):
            t=self.cases[name];review(status(t))
            self.assertEqual(t['commands']['samples'][0]['signature'],[17,0,code,branch,0,32])
            self.assertEqual(t['graphs'][0]['command_gate'],[branch,0,0,2**32-1,0,0,0,2])
            self.assertIsNone(t['commands']['samples'][0]['focus'])
    def test_dependency_qualification_corruption(self):
        for name in ('commands-dependency-discovery','commands-dependency-order'):
            for channel in ('command','graph'):
                # Mutate both copies of a command signature/gate when needed so
                # rejection comes from the dependency contract, not disagreement.
                for field,value in [('qualification',14),('qualification',(1<<16)|12),
                        ('qualification',(1<<16)|13),('reason',6),('detail',1),
                        ('branch',9),('key',1),('packet',100),('index',0),
                        ('raw',11),('read',1),('seen',1),('total',9)]:
                    with self.subTest(name=name,channel=channel,field=field,value=value):
                        t=deepcopy(self.cases[name])
                        if channel=='graph':
                            g=t['graphs'][0];receipt=g['command_gate']
                            if field in ('qualification','reason','detail'):g[field]=value
                        else:
                            g=t['commands']['samples'][0];receipt=g['gate']
                            if field in ('qualification','reason','detail'):
                                g['signature'][{'reason':0,'detail':1,'qualification':2}[field]]=value
                        fields={'branch':0,'key':1,'packet':2,'index':3,'raw':4,'read':5,'seen':6,'total':7}
                        if field in fields:
                            receipt[fields[field]]=value
                            if channel=='command':
                                g['signature'][3:]=[receipt[0],receipt[1],receipt[4]&31 if receipt[5] else 32]
                        with self.assertRaises(ValueError):review(status(t))
            t=deepcopy(self.cases[name]);del t['graphs'][0]['command_gate']
            with self.assertRaises(ValueError):review(status(t))
    def test_vertex_gate_short_circuit_receipts(self):
        for failed,mask,stride,offset,arena in ((1,0,0,0,0),(2,1,8,0,0),(3,1,16,0,0),
                (4,3,16,16777217,0),(5,3,16,64,0),(6,7,16,64,0),(7,7,16,64,2**64-1)):
            t=deepcopy(self.cases['vertex-gate']);t['graphs'][0]['vertex_gate']=[1000,2000,10,4,0,mask,stride,offset,arena,failed]
            review(status(t))
    def test_vertex_gate_corruption(self):
        for index,value in ((0,0),(1,0),(2,11),(3,3),(4,1048577),(5,3),(6,16),(7,64),(8,1234),(9,8)):
            t=deepcopy(self.cases['vertex-gate']);t['graphs'][0]['vertex_gate'][index]=value
            with self.assertRaises(ValueError):review(status(t))
        t=deepcopy(self.cases['vertex-gate']);t['graphs'][0]['vertex_gate'].pop()
        with self.assertRaises(ValueError):review(status(t))
        t=deepcopy(self.cases['vertex-gate']);t['graphs'][0]['qualification']=(5<<16)|4
        with self.assertRaises(ValueError):review(status(t))
    def test_layout_policy_retains_historical_semantics(self):
        t=deepcopy(self.cases['vertex-gate']);g=t['graphs'][0]
        g['vertex_gate']=[1000,2000,10,4,0,1,28,0,0,2]
        t.pop('vertex_policy',None);review(status(t)) # historical packed-only check
        t['vertex_policy']='native-overlay-layout-v1'
        with self.assertRaises(ValueError):review(status(t)) # 28 no longer fails stride gate
        g['vertex_gate']=[1000,2000,10,4,0,3,28,16777217,0,4];review(status(t))
        g['qualification']=3 # non-overlay stages remain packed
        with self.assertRaises(ValueError):review(status(t))
if __name__=='__main__':
    FIXTURE=[json.loads(line) for line in Path(sys.argv.pop(1)).read_text(encoding='utf-8-sig').splitlines()]
    unittest.main()
