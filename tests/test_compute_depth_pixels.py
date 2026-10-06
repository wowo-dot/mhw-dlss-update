import sys,unittest
from pathlib import Path
from copy import deepcopy
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from continuous_compute_depth_pixels import review


def fixture():
    c=dict(error=0,attempted=False,recorded=False,reset=False,saved=False,released=False,list=0,generation=0,resource=0,
           frame=0,packet=0,state=0,queue=0,signal=0,completed=0,width=2560,height=1440,format=41,subresource=0,
           row_pitch=10240,allocation_bytes=14745600,pixel_bytes=0)
    return dict(schema=1,policy='native-depth-before-reduction-v1',diagnostic_only=True,captures=[c,deepcopy(c)]),dict(samples=[])


def recorded():
    t,r=fixture();c=t['captures'][0]
    c.update(attempted=True,recorded=True,list=16,generation=2,resource=32,frame=8,packet=64,state=192)
    p=dict(error=0,calls=1,entered=True,at=[1,16,2,48,2],entry=[0]*8+[8],packet=64,uses=[[2,0]],states=[[1,192]])
    r['samples']=[dict(error=0,passes=[p],resources=[dict(owner=[0,0,0,32],description=[3,65536,2560,1440,1,1,41,1,0,0,1,1])])]
    return t,r


class Tests(unittest.TestCase):
    def test_not_yet_sampled(self):
        t,r=fixture();review(t,r)
        for c in t['captures']:c['released']=True
        review(t,r,final=True)
    def test_full_lifetime(self):
        t,r=recorded();prior=review(t,r);c=t['captures'][0]
        c.update(queue=128,signal=4);prior=review(t,r,prior)
        c.update(reset=True);prior=review(t,r,prior)
        c.update(completed=4);prior=review(t,r,prior)
        c.update(saved=True,released=True,pixel_bytes=14745600);t['captures'][1]['released']=True
        review(t,r,prior,True)
    def test_save_requires_reset_and_completion(self):
        for missing in ('reset','completed','signal','released'):
            t,r=recorded();c=t['captures'][0];c.update(queue=128,signal=4,completed=4,reset=True,saved=True,released=True,pixel_bytes=14745600)
            c[missing]=False if missing in ('reset','released') else 0
            with self.assertRaises(ValueError):review(t,r)
    def test_wrong_native_association(self):
        for field in ('list','generation','resource','frame','packet','state'):
            t,r=recorded();t['captures'][0][field]+=1
            with self.assertRaises(ValueError):review(t,r)
    def test_unknown_state(self):
        t,r=recorded();r['samples'][0]['passes'][0]['states'][0]=[0,0]
        with self.assertRaises(ValueError):review(t,r)
    def test_wrong_footprint(self):
        for field in ('width','height','format','subresource','row_pitch','allocation_bytes'):
            t,r=recorded();t['captures'][0][field]+=1
            with self.assertRaises(ValueError):review(t,r)
    def test_unissued_cannot_claim_pixels(self):
        t,r=fixture();t['captures'][0]['pixel_bytes']=14745600
        with self.assertRaises(ValueError):review(t,r)
    def test_partial_scope_is_explicit(self):
        t,r=fixture();t['captures'][0]['error']=2;review(t,r)
        t['captures'][0]['released']=t['captures'][1]['released']=True;review(t,r,final=True)
    def test_premature_reference_release(self):
        t,r=recorded();t['captures'][0]['released']=True
        with self.assertRaises(ValueError):review(t,r)
    def test_final_pending_rejected(self):
        t,r=recorded()
        with self.assertRaises(ValueError):review(t,r,final=True)
    def test_signal_cannot_change(self):
        t,r=recorded();c=t['captures'][0];c.update(queue=128,signal=4);old=deepcopy(t);c['signal']=5
        with self.assertRaises(ValueError):review(t,r,old)
    def test_record_cannot_change(self):
        t,r=recorded();old=deepcopy(t);t['captures'][0]['attempted']=False
        with self.assertRaises(ValueError):review(t,r,old)


if __name__=='__main__':unittest.main()
