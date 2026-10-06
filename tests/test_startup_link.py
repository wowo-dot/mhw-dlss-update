"""Cross-run receipt rejection using the accepted startup log, without altering it."""
import copy,json,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from temporal_sdk import review_startup_link
class StartupLinkTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.rows=[json.loads(s) for s in source.read_text().splitlines()]
        assert cls.rows[0]['version']=='1.12.1'
        cls.rows[0]['version']='1.13.0' # synthetic combined-version wrapper, not a live v1.13 result
        cls.header=copy.deepcopy(cls.rows[0])
        entry=next(r for r in cls.rows if r['event']=='native_enter')
        graphics=next(r for r in cls.rows if r['event']=='graphics')
        init=next(r for r in cls.rows if r['event']=='public_init')
        cls.capture={k:graphics[k] for k in ('device','width','height','swapchain_format')}
        cls.capture.update(started_tick=entry['tick']+10000,adapter_luid_low=init['adapter_luid_low'],adapter_luid_high=init['adapter_luid_high'])
    def test_link(self):review_startup_link(self.header,self.capture,self.rows)
    def test_version_binding(self):
        for version in ('1.14.0','1.15.0','1.15.1','1.15.2','1.15.3','1.16.0','1.17.0'):
            h=copy.deepcopy(self.header);r=copy.deepcopy(self.rows);h['version']=version
            with self.subTest(version=version),self.assertRaises(ValueError):review_startup_link(h,self.capture,r)
            r[0]['version']=version;review_startup_link(h,self.capture,r)
    def test_different_process_or_binary(self):
        for key in ('pid','creation','base','host_sha256','dll_sha256'):
            h=copy.deepcopy(self.header);h[key]=0
            with self.subTest(key=key),self.assertRaises(ValueError):review_startup_link(h,self.capture,self.rows)
    def test_different_device_or_display(self):
        for key in ('device','width','height','swapchain_format','adapter_luid_low','adapter_luid_high','started_tick'):
            c=copy.deepcopy(self.capture);c[key]=-1
            with self.subTest(key=key),self.assertRaises(ValueError):review_startup_link(self.header,c,self.rows)
    def test_failed_native(self):
        r=copy.deepcopy(self.rows);next(x for x in r if x['event']=='native_return')['result']=0xbad00002
        with self.assertRaises(ValueError):review_startup_link(self.header,self.capture,r)
    def test_incomplete_startup(self):
        r=copy.deepcopy(self.rows);r[-1]['hook_disabled']=False
        with self.assertRaises(ValueError):review_startup_link(self.header,self.capture,r)
    def test_wrong_version(self):
        r=copy.deepcopy(self.rows);r[0]['version']='1.12.1'
        with self.assertRaises(ValueError):review_startup_link(self.header,self.capture,r)
if __name__=='__main__':
    source=Path(sys.argv.pop(1));unittest.main()
