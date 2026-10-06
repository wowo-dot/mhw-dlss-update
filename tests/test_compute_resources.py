import sys,unittest
from pathlib import Path
from copy import deepcopy
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from continuous_compute_resources import review

def fixture():
    owner=[16,32,0,64,2560+(1440<<16),16,2**32-1]
    cpu=dict(samples=[dict(at=[1,2,2,4,5,6,0,1],computes=[dict(packet=128+i,words=[2],textures=[dict(index=0,fields=owner)]) for i in range(6)])])
    passes=[dict(packet=128+i,error=0,entered=True,returned=True,calls=1,at=[100,256,1,512,101],pipeline=[1024,2048],uses=[[2,0]],states=[[1,64]]) for i in range(6)]
    trace=dict(schema=1,policy='native-compute-resource-observation-v1',diagnostic_only=True,gpu_work_added=False,
               pixel_footprint_verified=False,descriptor_contents_verified=False,lost=0,stopped=False,
               samples=[dict(at=cpu['samples'][0]['at'],error=0,resources=[dict(owner=owner,description=[3,0,2560,1440,1,1,39,1,0,0,0,1],held=True)],passes=passes)],
               barriers=[[256,1,64,0,0,2**32-1,0,64,0]])
    return trace,cpu

def entry_fixture():
    t,c=fixture();t.update(schema=2,policy='native-compute-entry-observation-v2')
    for d,p in zip(c['samples'][0]['computes'],t['samples'][0]['passes']):
        d['prefix']=[6]+[0]*11
        p.update(entry=[0,4096,512,256,1,1,1,1,2],recipe=[1,0],observed_prefix=d['prefix'].copy())
    return t,c

def stable_fixture():
    t,c=entry_fixture();t.update(schema=3,policy='native-compute-stable-recipe-v3')
    for p in t['samples'][0]['passes']:
        p['stable_difference']=0
        p['observed_prefix'][2:4]=[0x14a35580,0x12];p['recipe']=[1,2]
    return t,c

class Tests(unittest.TestCase):
    def test_capture_and_stop(self):
        t,c=fixture();prior=review(t,c);t['stopped']=True;t['samples'][0]['resources'][0]['held']=False
        review(t,c,prior,True)
    def test_unknown_is_valid_observation(self):
        t,c=fixture();t['samples'][0]['passes'][0]['states']=[[0,0]];review(t,c)
        t['samples'][0]['passes'][0]['states']=[[0,64]]
        with self.assertRaises(ValueError):review(t,c)
    def test_no_pixel_claim(self):
        for key in ('gpu_work_added','pixel_footprint_verified','descriptor_contents_verified'):
            t,c=fixture();t[key]=True
            with self.assertRaises(ValueError):review(t,c)
    def test_binding_identity(self):
        t,c=fixture();t['samples'][0]['resources'][0]['owner']=list(t['samples'][0]['resources'][0]['owner']);t['samples'][0]['resources'][0]['owner'][3]=72
        with self.assertRaises(ValueError):review(t,c)
    def test_complete_immutable(self):
        t,c=fixture();prior=deepcopy(t);t['samples'][0]['passes'][0]['states']=[[1,8]]
        with self.assertRaises(ValueError):review(t,c,prior)
    def test_bounded_evidence(self):
        t,c=fixture();t['barriers']*=257
        with self.assertRaises(ValueError):review(t,c)
    def test_final_releases(self):
        t,c=fixture();t['stopped']=True
        with self.assertRaises(ValueError):review(t,c,final=True)
    def test_missing_use(self):
        t,c=fixture();t['samples'][0]['passes'][0]['uses']=[];t['samples'][0]['passes'][0]['states']=[]
        with self.assertRaises(ValueError):review(t,c)
    def test_entry_success(self):
        t,c=entry_fixture();review(t,c)
        t['samples'][0]['passes'][0]['entry'][7]=2
        with self.assertRaises(ValueError):review(t,c)
    def test_recipe_diagnostics(self):
        t,c=entry_fixture();p=t['samples'][0]['passes'][0]
        p.update(error=1,entered=False,returned=False,calls=0,at=[0]*5,pipeline=[0]*2)
        p['entry'][0]=11;p['recipe']=[1,128];review(t,c)
        p['recipe']=[1,2]
        with self.assertRaises(ValueError):review(t,c)
        p['observed_prefix'][9]=256;review(t,c)
        p['entry'][0]=10;p['recipe']=[0,0];p['observed_prefix']=[0]*12;review(t,c)
    def test_observer_diagnostics(self):
        t,c=entry_fixture();p=t['samples'][0]['passes'][0]
        p.update(error=1,entered=False,returned=False,calls=0,at=[0]*5,pipeline=[0]*2,recipe=[0,0],observed_prefix=[0]*12)
        p['entry']=[8,4096,512,256,1,1,0,1,2];review(t,c)
        p['entry'][6]=1
        with self.assertRaises(ValueError):review(t,c)
    def test_missing_entry_reason(self):
        t,c=entry_fixture();p=t['samples'][0]['passes'][0];p['error']=1
        with self.assertRaises(ValueError):review(t,c)

    def test_scheduled_pointer_is_retained(self):
        t,c=stable_fixture();result=review(t,c)
        self.assertEqual(result['samples'][0]['passes'][0]['observed_prefix'][2:4],[0x14a35580,0x12])
        t['samples'][0]['passes'][0]['observed_prefix'][2:4]=[0,0]
        with self.assertRaises(ValueError):review(t,c)  # raw mask must also agree

    def test_only_scheduling_words_excluded(self):
        for word in range(12):
            t,c=stable_fixture();p=t['samples'][0]['passes'][0];p['observed_prefix'][word]^=0x80000000
            if word in (2,3):review(t,c)
            else:
                with self.assertRaises(ValueError):review(t,c)

    def test_stable_recipe_changes_rejected(self):
        for bit in (1,4,8,16,32,64,128):
            t,c=stable_fixture();p=t['samples'][0]['passes'][0];p['recipe'][1]|=bit
            with self.assertRaises(ValueError):review(t,c)  # cannot suppress a group
            p['stable_difference']=bit
            with self.assertRaises(ValueError):review(t,c)  # cannot call it success
            p['error']=3;review(t,c)  # valid evidence of an API-time rejection

    def test_pointer_only_entry_rejection_is_invalid(self):
        t,c=stable_fixture();p=t['samples'][0]['passes'][0]
        p.update(error=1,entered=False,returned=False,calls=0,at=[0]*5,pipeline=[0]*2)
        p['entry'][0]=11
        with self.assertRaises(ValueError):review(t,c)
        p['observed_prefix'][6]=80;p['stable_difference']=2;review(t,c)

    def test_stable_unreadable_and_schema_requirements(self):
        t,c=stable_fixture();p=t['samples'][0]['passes'][0]
        del p['stable_difference']
        with self.assertRaises(KeyError):review(t,c)
        p.update(stable_difference=0,error=3,recipe=[0,0],observed_prefix=[0]*12);review(t,c)
        p['stable_difference']=2
        with self.assertRaises(ValueError):review(t,c)

    def test_completed_scheduled_pointer_immutable(self):
        t,c=stable_fixture();prior=review(t,c)
        t['samples'][0]['passes'][0]['observed_prefix'][2]+=64
        with self.assertRaises(ValueError):review(t,c,prior)

if __name__=='__main__':unittest.main()
