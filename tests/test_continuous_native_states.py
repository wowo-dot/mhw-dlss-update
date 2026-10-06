"""Validate native provenance bounds and association over owned memory fixtures."""
import copy,json,subprocess,sys,unittest
from pathlib import Path
REPO=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(REPO/'tools'))
from continuous_native_states import review
FIXTURES={}

def setUpModule():
    p=subprocess.run([str(REPO/'build/bin/Release/continuous_runtime_tests.exe'),'native-state-trace'],capture_output=True,text=True,check=True)
    for line in p.stdout.splitlines():
        if line.startswith('{'):
            row=json.loads(line);FIXTURES[row['case']]=row['provenance']

class NativeStateTests(unittest.TestCase):
    def setUp(self):self.p=copy.deepcopy(FIXTURES['earlier-transition'])
    def test_all_receipts(self):
        for name,p in FIXTURES.items():
            with self.subTest(name=name):self.assertTrue(review(p))
    def test_hoisted_transition_not_last_state(self):
        self.assertEqual(self.p['index'],2)
        self.assertEqual(self.p['earlier'][0]['nodes'][0][1:4],[1,0x100,4])
        self.assertEqual(self.p['trackers'][0]['final'],0x100)
        self.assertEqual(self.p['trackers'][0]['spans'][0][6],4)
    def test_packet_location_corruption(self):
        for key,value in [('index',3),('scanned',32769),('matches',4)]:
            p=copy.deepcopy(self.p);p[key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):review(p)
    def test_earlier_packet_order_corruption(self):
        self.p['earlier'][0]['index']=1
        with self.assertRaises(ValueError):review(self.p)
    def test_span_linkage_corruption(self):
        self.p['trackers'][0]['spans'][1][0]+=16
        with self.assertRaises(ValueError):review(self.p)
    def test_incomplete_and_mutating_are_evidence(self):
        self.assertFalse(FIXTURES['cyclic-spans']['trackers'][0]['complete'])
        self.assertFalse(FIXTURES['cyclic-earlier']['earlier'][0]['complete'])
        self.assertFalse(FIXTURES['changing-tracker']['trackers'][0]['stable'])
        self.assertFalse(FIXTURES['changing-schedule']['stable'])
    def test_capped_queue_not_claimed_complete(self):
        p=copy.deepcopy(FIXTURES['oversized-schedule']);p['scan_complete']=True
        with self.assertRaises(ValueError):review(p)
    def test_scalar_bounds(self):
        for value in (True,-1,2**64):
            p=copy.deepcopy(self.p);p['epoch']=value
            with self.subTest(value=value),self.assertRaises(ValueError):review(p)
    def test_earlier_node_bounds(self):
        self.p['earlier'][0]['nodes'][0][1]=8
        with self.assertRaises(ValueError):review(self.p)

if __name__=='__main__':unittest.main()
