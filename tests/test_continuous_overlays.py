from copy import deepcopy
import sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from continuous_overlays import review

def sample():
    return dict(claimed=100,complete=100,overlays=dict(schema=1,policy='native-in-place-extent-v1',prepared=180,draws=180,completed=180,rejected=0),recording_states=dict(schema=1,policy='native-recording-observation-v1',observations=1000,outside_selected_callbacks=800,imported=600))

class OverlayTests(unittest.TestCase):
    def test_dof_route(self):
        s=sample();s['overlays']['policy']='native-extent-and-dependencies-v2'
        s['dependency_route']=dict(schema=2,copy_draws=40,radial_draws=40,dof_draws=100,rejection=0,last_description=[4,2,10,3,64,32])
        old=review(s,final=True)
        for value in (-1,True,101):
            bad=deepcopy(s);bad['dependency_route']['dof_draws']=value
            with self.assertRaises(ValueError):review(bad)
        s['dependency_route']['dof_draws']=99
        with self.assertRaises(ValueError):review(s,old)
    def test_dependency_route(self):
        s=sample();s['overlays']['policy']='native-extent-and-dependencies-v2'
        s['dependency_route']=dict(schema=1,copy_draws=40,radial_draws=40,rejection=0,last_description=[3,23,77,3,128,128])
        old=review(s,final=True)
        for key,value in [('schema',2),('copy_draws',141),('radial_draws',True),('rejection',5),('last_description',[0]*5)]:
            bad=deepcopy(s);bad['dependency_route'][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):review(bad)
        del s['dependency_route']
        with self.assertRaises(KeyError):review(s)
        s['dependency_route']=deepcopy(old['dependency_route']);s['dependency_route']['copy_draws']=39
        with self.assertRaises(ValueError):review(s,old)
    def test_extended_layout(self):
        s=sample();s['overlays'].update(extended_draws=120,extended_layout=[28,3,6,16,16,0]);old=review(s,final=True)
        for key,value in [('extended_draws',181),('extended_draws',True),('extended_layout',[28,3,6,20,16,0]),
                          ('extended_layout',[28,3,7,16,16,0]),('extended_layout',[28,3,6,0,16,4]),('extended_layout',None)]:
            bad=deepcopy(s);bad['overlays'][key]=value
            with self.subTest(key=key,value=value),self.assertRaises(ValueError):review(bad)
        s['overlays']['extended_draws']=119
        with self.assertRaises(ValueError):review(s,old)
        s['overlays'].update(extended_draws=0,extended_layout=None);review(s)
    def test_complete(self):review(sample(),final=True)
    def test_shared_keys(self):
        s=sample();s['overlays'].update(shared_key_prepared=60,shared_key_completed=60);review(s,final=True)
        for key,value in (('shared_key_prepared',91),('shared_key_completed',61),('shared_key_prepared',59),('shared_key_prepared',True)):
            bad=deepcopy(s);bad['overlays'][key]=value
            with self.assertRaises(ValueError):review(bad,final=True)
        before=review(s);s['overlays'].update(shared_key_prepared=59,shared_key_completed=59)
        with self.assertRaises(ValueError):review(s,before)
    def test_corruptions(self):
        for channel,key,value in [('overlays','policy','wrong'),('overlays','prepared',179),('overlays','draws',181),('overlays','completed',179),('overlays','draws',True),('recording_states','observations',799),('recording_states','imported',-1),('recording_states','schema',2)]:
            s=sample();s[channel][key]=value
            with self.subTest(channel=channel,key=key),self.assertRaises(ValueError):review(s,final=True)
    def test_rollback(self):
        old=review(sample());s=sample();s['recording_states']['imported']-=1
        with self.assertRaises(ValueError):review(s,old)
    def test_inflight(self):
        s=sample();s.update(claimed=101);s['overlays'].update(prepared=188,draws=184);review(s)
        with self.assertRaises(ValueError):review(s,final=True)
        s['overlays']['prepared']=189
        with self.assertRaises(ValueError):review(s)

if __name__=='__main__':unittest.main()
