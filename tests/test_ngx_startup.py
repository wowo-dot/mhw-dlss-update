"""Reject damaged ordering/capability receipts, using an explicitly synthetic host wrapper."""
import copy,json,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from review_ngx_startup import review,HOST
class StartupReviewTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        fixture_rows=[json.loads(s) for s in (fixture/'events.jsonl').read_text().splitlines()]
        assert fixture_rows[-1]['event']=='fixture_pass' and fixture_rows[-1]['native_error_preserved']
        base=0x140000000
        header=dict(event='start',schema=1,version='1.12.0',experiment='ngx-startup-capabilities',pid=1,creation=1,
            base=base,host_sha256=HOST,host_version='0.0.0.0',dll_sha256='a'*64,graphics_api='D3D12',hooks=1,gpu_commands=0,feature_creations=0)
        cls.rows=[header]+copy.deepcopy(fixture_rows[:-1])+[dict(event='stopped',calls=1,transaction_complete=True,
            capabilities_supported=True,hook_disabled=True,settled=True,swapchain_observed=True,dll_pinned=True,
            trampoline_retained=True,sdk_shutdown_deferred=True,sdk_evaluation_verified=False)]
        cls.rows[2]['caller']=base+0x23842dc
        cls.rows[5]['result']=1 # synthetic success case; the hardware stub deliberately returned failure
    def test_synthetic_success(self):self.assertTrue(review(self.rows)['accepted'])
    def readiness_rows(self):
        r=copy.deepcopy(self.rows);r[0]['version']='1.12.1';tick=r[1]['tick']
        r.insert(1,dict(event='code_readiness',started_tick=tick-20,tick=tick,polls=3,
            first_mask=0,final_mask=15,slots_empty=True,timeout_ms=300000,ready=True,hooks_installed=False))
        return r
    def test_delayed_readiness(self):self.assertTrue(review(self.readiness_rows())['accepted'])
    def test_independent_stream_version(self):
        for version in ('0.8.0','0.8.1','0.8.2','0.8.3'):
            r=self.readiness_rows();r[0]['version']=version
            self.assertTrue(review(r)['accepted'])
    def test_already_ready(self):
        r=self.readiness_rows();r[1].update(started_tick=r[1]['tick'],polls=1,first_mask=15)
        self.assertTrue(review(r)['accepted'])
    def test_readiness_rejections(self):
        for key,value in [('polls',0),('polls',True),('first_mask',16),('final_mask',7),('final_mask',16),
                          ('slots_empty',False),('ready',False),('hooks_installed',True),('timeout_ms',300001)]:
            r=self.readiness_rows();r[1][key]=value
            with self.subTest(key=key,value=value),self.assertRaises(ValueError):review(r)
    def test_readiness_timing(self):
        for change in ('deadline','backward','late'):
            r=self.readiness_rows();ready=r[1]
            if change=='deadline':ready['started_tick']=ready['tick']-300000
            elif change=='backward':ready['started_tick']=ready['tick']+1
            else:ready['tick']=r[2]['tick']+1
            with self.subTest(change=change),self.assertRaises(ValueError):review(r)
    def test_readiness_required_only_for_new_version(self):
        r=copy.deepcopy(self.rows);r[0]['version']='1.12.1'
        with self.assertRaises(ValueError):review(r)
        r=self.readiness_rows();r[0]['version']='1.12.0'
        with self.assertRaises(ValueError):review(r)
    def test_missed_or_timed_out_run(self):
        for reason in ('native-init-already-observed','startup-code-timeout'):
            r=self.readiness_rows()[:2];r[1]['ready']=False
            r.append(dict(event='failed',reason=reason,hook_disabled=True))
            with self.subTest(reason=reason),self.assertRaises(ValueError):review(r)
    def test_bad_capabilities(self):
        for i in (4,6):
            for key,value in [('get_result',0xbad00004),('get_values',[0,1,1]),('values_u32',[0,0,0xbad00004]),
                              ('destroy_result',0),('modern_loaded',False),('provider_identity_verified',False)]:
                r=copy.deepcopy(self.rows);r[i][key]=value
                with self.subTest(i=i,key=key),self.assertRaises(ValueError):review(r)
    def test_bad_admission(self):
        for key,value in [('caller',1),('device',0),('api',21),('slots_zero',False)]:
            r=copy.deepcopy(self.rows);r[2][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):review(r)
    def test_native_failure(self):
        r=copy.deepcopy(self.rows);r[5]['result']=0xbad00002
        with self.assertRaises(ValueError):review(r)
    def test_provider_identity(self):
        for key,value in [('runtime_sha256','a'*64),('project_id','wrong')]:
            r=copy.deepcopy(self.rows);r[3][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):review(r)
    def test_changed_original(self):
        for key in ('arguments_unchanged','result_unchanged'):
            r=copy.deepcopy(self.rows);r[5][key]=False
            with self.assertRaises(ValueError):review(r)
    def test_order(self):
        r=copy.deepcopy(self.rows);r[3],r[5]=r[5],r[3]
        with self.assertRaises(ValueError):review(r)
    def test_closure(self):
        for key,value in [('calls',2),('hook_disabled',False),('settled',False),('swapchain_observed',False),('sdk_shutdown_deferred',False)]:
            r=copy.deepcopy(self.rows);r[-1][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):review(r)
    def test_graphics(self):
        for key,value in [('device',1),('width',0),('swapchain_format',0)]:
            r=copy.deepcopy(self.rows);r[7][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):review(r)
if __name__=='__main__':
    fixture=Path(sys.argv.pop(1));unittest.main()
