"""Activation receipt checks against the actual owned native-hook fixture."""
import copy,json,subprocess,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from continuous_activation import review,COUNTERS
from arm_continuous_dlaa import parse
from review_continuous_dlaa import review as review_run

REPO=Path(__file__).resolve().parents[1]
FIXTURES={}

def setUpModule():
    r=subprocess.run([str(REPO/'build/bin/Release/native_activation_tests.exe')],capture_output=True,text=True,check=True)
    FIXTURES.update({j['case']:j for line in r.stdout.splitlines() if line.startswith('{') for j in [json.loads(line)]})

class ActivationTests(unittest.TestCase):
    def setUp(self):self.s=copy.deepcopy(FIXTURES['healthy'])
    def test_native_receipt(self):
        r=review(self.s,final=True)
        self.assertTrue(r['saved_aa_off_observed']);self.assertFalse(r['exclusive_aa_verified'])
        self.assertEqual(r['taa_admitted'],13);self.assertEqual(r['competing_suppressed'],3)
    def test_latched_unfiltered_queue(self):
        r=review(FIXTURES['latched'],final=True)
        self.assertEqual(r['queue_unfiltered'],13)
        self.assertEqual(r['taa_admitted'],13)
        self.assertTrue(r['failure_latched'])
        s=copy.deepcopy(FIXTURES['latched']);s['activation']['failure_latched']=False
        with self.assertRaises(ValueError):review(s)
    def test_native_exception_rejected(self):
        with self.assertRaises(ValueError):review(FIXTURES['exception'],final=True)
    def test_native_taa_exception_rejected(self):
        with self.assertRaises(ValueError):review(FIXTURES['taa-exception'],final=True)
    def test_native_reentrant_rejected(self):
        with self.assertRaises(ValueError):review(FIXTURES['reentrant'],final=True)
    def test_queue_exception_rejected(self):
        with self.assertRaises(ValueError):review(FIXTURES['queue-exception'],final=True)
    def test_queue_partition(self):
        self.s['activation']['queue_inserted']+=1
        with self.assertRaises(ValueError):review(self.s,final=True)
    def test_queue_reason_coverage(self):
        self.s['activation']['queue_reasons'][0]+=1
        with self.assertRaises(ValueError):review(self.s)
    def test_queue_counter_types(self):
        for k in ('layer_flag_writes','queue_visits','queue_inserted','queue_existing','queue_rejected','execute_unowned'):
            for value in (True,-1,None,2**64):
                s=copy.deepcopy(self.s);s['activation'][k]=value
                with self.subTest(key=k,value=value),self.assertRaises(ValueError):review(s)
    def test_types(self):
        for k in COUNTERS:
            for value in (True,-1,None,2**64):
                s=copy.deepcopy(self.s);s['activation'][k]=value
                with self.subTest(key=k,value=value),self.assertRaises(ValueError):review(s)
    def test_boolean_types(self):
        for k in ('requested','hooks_disabled'):
            self.s['activation'][k]=0
            with self.assertRaises(ValueError):review(self.s)
            self.s=copy.deepcopy(FIXTURES['healthy'])
    def test_partitions(self):
        for k in ('taa_visits','taa_admitted','taa_rejected','frames'):
            s=copy.deepcopy(self.s);s['activation'][k]+=1
            with self.subTest(key=k),self.assertRaises(ValueError):review(s)
    def test_no_setting_or_layer_mutations(self):
        for k in ('settings_writes','layer_flag_writes'):
            s=copy.deepcopy(self.s);s['activation'][k]=1
            with self.assertRaises(ValueError):review(s)
    def test_choice_coverage(self):
        for value in ([],[True,1,1,11],[1,-1,1,13],[1,1,1,99]):
            s=copy.deepcopy(self.s);s['activation']['native_choices']=value
            with self.assertRaises(ValueError):review(s)
    def test_closure(self):
        for k,v in (('requested',True),('pending',1),('active_context',123),('hooks_disabled',False)):
            s=copy.deepcopy(self.s);s['activation'][k]=v
            review(s)
            with self.assertRaises(ValueError):review(s,final=True)
    def test_activation_must_exercise(self):
        for k in ('table_forced','sharpen_suppressed'):
            s=copy.deepcopy(self.s);s['activation'][k]=0
            with self.assertRaises(ValueError):review(s,final=True)
    def test_rollback(self):
        prior=review(self.s)
        self.s['activation']['table_forced']-=1
        with self.assertRaises(ValueError):review(self.s,prior)
    def test_policy(self):
        self.s['activation']['policy']='shared-table-write'
        with self.assertRaises(ValueError):review(self.s)
    def test_full_new_version_review_and_header(self):
        # Private prior live receipts supply unrelated GPU/startup fields; only
        # activation is synthetic here. This is not a v1.20.2 live result.
        root=REPO/'local/native-taa-bypass-20260927/v1.19.1'
        rows=[json.loads(line) for line in (root/'inputs-47596-134349394655214204.jsonl').read_text().splitlines()]
        startup=[json.loads(line) for line in (root/'startup-47596-134349394655214204.jsonl').read_text().splitlines()]
        rows[0]['version']=startup[0]['version']='1.20.2';rows[0]['activation']='native-postqueue-preparation-v2'
        for s in rows[4:-1]:s['activation']=copy.deepcopy(self.s['activation'])
        result=review_run(rows,startup,rows[0]['dll_sha256'])
        self.assertTrue(result['activation']['scoped_activation_verified'])
        del rows[-2]['activation']
        with self.assertRaises(KeyError):review_run(rows,startup,rows[0]['dll_sha256'])
        del rows[0]['activation']
        with self.assertRaises(ValueError):parse((json.dumps(rows[0])+'\n').encode())

if __name__=='__main__':unittest.main()
