import unittest
from copy import deepcopy
from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from continuous_silhouette import review,HASHES

def sample():
    s=dict(frame=1,epoch=1,tick=1,key=0x7ffffff4,technique=14618,calls=1,caller=1,state=1,rejection=0,
        returned=True,stable=True,api_seen=True,root_ready=True,raster_ready=True,pixel_sha256=sorted(HASHES)[0],
        arguments=[6,1,0,-1,0],packet_arguments=[6,0,0xffffffff],packet_bindings=[0]*4,extra_stages=[0]*3,
        binding_owners=[0]*12,screen_words=[0]*28,material_words=[0]*5,color_state=[0]*2,
        native_viewport=[0,0,1280,720],native_scissor=[0,0,1280,720],descriptions=[[0]*14 for _ in range(8)],
        raster=dict(index=[0,0,57],vertices=[[0,0,0] for _ in range(4)],index_known=True,output_known=True,vertex_mask=15,viewport=[0,0,1280,720,0,1]))
    return dict(schema=1,mode='read-only-indexed-contract',visits=1,errors=0,candidate_rejections=0,sample_count=1,samples=[s])
class SilhouetteTests(unittest.TestCase):
    def test_receipt(self):self.assertEqual(review(sample(),final=True),sample())
    def test_no_samples(self):
        s=sample();s.update(visits=0,sample_count=0,samples=[]);review(s,final=True)
    def test_callback_retirement(self):
        s=sample();s['samples'][0].update(returned=False,stable=False);review(s)
        with self.assertRaises(ValueError):review(s,final=True)
    def test_diagnostic_rejection(self):
        s=sample();s['samples'][0].update(rejection=3,stable=False);review(s,final=True)
        s['samples'][0]['stable']=True
        with self.assertRaises(ValueError):review(s)
    def test_health_and_bounds(self):
        for key,value in [('errors',1),('visits',4097),('sample_count',17),('schema',2),('visits',True)]:
            s=sample();s[key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):review(s)
    def test_native_identity(self):
        for key,value in [('pixel_sha256','0'*64),('key',0x80000000),('calls',2)]:
            s=sample();s['samples'][0][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):review(s)
    def test_geometry(self):
        for value in (float('nan'),float('inf')):
            s=sample();s['samples'][0]['raster']['viewport'][2]=value
            with self.assertRaises(ValueError):review(s)
    def test_native_api_arguments(self):
        for i,value in ((0,7),(1,2),(3,-2),(4,1)):
            s=sample();s['samples'][0]['arguments'][i]=value
            with self.assertRaises(ValueError):review(s)
    def test_immutable_retired_sample(self):
        old=sample();new=deepcopy(old);new['samples'][0]['technique']+=1
        with self.assertRaises(ValueError):review(new,old)
    def test_counter_rollback(self):
        old=sample();old['visits']=2
        with self.assertRaises(ValueError):review(sample(),old)
if __name__=='__main__':unittest.main()
