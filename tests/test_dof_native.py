import unittest,sys
from pathlib import Path
from copy import deepcopy
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from continuous_dof_native import review
from continuous_output_commands import qualification

def sample():
    return dict(schema=1,policy='native-six-compute-preserved-views-v1',qualified=101,warmups=6,
                dispatches=600,adapted=200,completed=100,rejected=0,restored=200,reason=0,passes=[100]*6,**{'pass':0})
class NativeDofTests(unittest.TestCase):
    def test_complete(self):review(sample(),final=True)
    def test_empty(self):
        s=sample()
        for k in ('qualified','warmups','dispatches','adapted','completed','restored'):s[k]=0
        s['passes']=[0]*6;review(s,final=True)
    def test_missing_dispatch(self):
        s=sample();s['passes'][2]-=1
        with self.assertRaises(ValueError):review(s,final=True)
    def test_missing_restore(self):
        s=sample();s['restored']-=1
        with self.assertRaises(ValueError):review(s)
    def test_unreturned(self):
        s=sample();s['completed']-=1;review(s)
        with self.assertRaises(ValueError):review(s,final=True)
    def test_rollback(self):
        old=sample();s=deepcopy(old);s['qualified']-=1
        with self.assertRaises(ValueError):review(s,old)
    def test_inflight_bound(self):
        s=sample();s['completed']=67
        with self.assertRaises(ValueError):review(s)
    def test_qualification(self):
        good=[10,0x78006595,0,2**32-1,0,0,0,1];qualification(14,17,0,good)
        for bad in ([10,0,0,2**32-1,0,0,0,1],[8,0x78006595,0,2**32-1,0,0,0,1]):
            with self.assertRaises(ValueError):qualification(14,17,0,bad)
if __name__=='__main__':unittest.main()
