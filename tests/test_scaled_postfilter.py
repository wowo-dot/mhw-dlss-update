import copy,json,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from temporal_postfilter import review
from review_scaled_temporal import parse

class PostfilterReview(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.path=Path(sys.argv[1]);p=json.loads(cls.path.read_text());cls.h={'base':p['target']-0x24ca1f0,'colorgrade_observer':p['schema']==2}
        cls.d={'postfilter':p,'records':[]}
        # The fixture executes the real hook entry through its own ASM caller.
        assert all(r['caller']==p['records'][0]['caller']>0 for r in p['records'])
        for r in p['records']:
            r['caller']=cls.h['base']+0x24f228b
            cls.d['records'].append(dict(**{k:r[k] for k in ('frame','epoch','thread','context','view')},
                source=r['final_source'],taa_destination=r['input']['owner'],rectangle=r['rectangle'],
                dimensions=[2560,1440],offset=[-.5/2560,-.5/1440],packets=[{'object':r['final_geometry']['packet']}],
                filters=[dict(type=cls.h['base']+0x3537a70,stage=15,order=100,unit=r['unit'],input=r['input']['owner'],output=r['output']['owner'])]))
            if cls.h['colorgrade_observer']:
                g=r['colorgrade'];cls.d['records'][-1]['filters'] += [dict(type=cls.h['base']+0x3538400,stage=15,order=0,unit=g['unit']),
                    dict(type=0,stage=15,order=0x7fffff1,unit=0,entry=g['endpoint'],flags=[1,0,0],input=g['input']['owner'],output=g['output']['owner'])]

    def test_accept(self):
        result=review(self.h,self.d,self.path.parent)
        self.assertTrue(result['postfilter_cpu_construction_verified'])
        self.assertFalse(result['postfilter_gpu_consumption_verified'])
        self.assertFalse(result['full_output_handoff_verified'])

    def test_association(self):
        for key,value in [('frame',999),('unit',1),('thread',0),('caller',1),('valid',False),('saved',False),('returned_rectangle',[0,0,1,1])]:
            d=copy.deepcopy(self.d);d['postfilter']['records'][0][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):review(self.h,d,self.path.parent)

    def test_geometry_and_owner(self):
        for change in ('vertex','stride','uv','packet','owner','commands'):
            d=copy.deepcopy(self.d);r=d['postfilter']['records'][0]
            if change=='vertex':r['final_geometry']['vertices']+=16
            elif change=='stride':r['filter_geometry']['stride']=32
            elif change=='uv':r['final_geometry']['values'][10]=2
            elif change=='packet':r['filter_geometry']['packet']=1
            elif change=='owner':r['output']['owner']=1
            else:r['commands'][0]['key']+=1
            with self.subTest(change=change),self.assertRaises(ValueError):review(self.h,d,self.path.parent)

    def test_artifact_and_cleanup(self):
        for key,value in [('file','../secret'),('shader_size',262145),('sha256','bad')]:
            d=copy.deepcopy(self.d);d['postfilter']['records'][0][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):review(self.h,d,self.path.parent)
        for key,value in [('fault',1),('pending',1),('completed',2),('mutations',1),('drained',False),('hooks_disabled',False)]:
            d=copy.deepcopy(self.d);d['postfilter'][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):review(self.h,d,self.path.parent)

    def test_header(self):
        h=json.loads(Path(sys.argv[2]).read_text().splitlines()[0])
        h.update(version='1.23.9',hooks=25,postfilter_observer=True,postfilter_samples=3,
                 postfilter_command_limit=32,postfilter_shader_limit=262144,postfilter_mutations=0)
        self.assertEqual(parse(json.dumps(h).encode())[0],h)
        for key,value in [('hooks',24),('postfilter_observer',False),('postfilter_samples',4),('postfilter_command_limit',33),('postfilter_shader_limit',262145),('postfilter_mutations',1)]:
            bad=dict(h);bad[key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):parse(json.dumps(bad).encode())

    def test_colorgrade(self):
        if not self.h['colorgrade_observer']:self.skipTest('historical v1 record')
        self.assertTrue(review(self.h,self.d,self.path.parent)['colorgrade_cpu_construction_verified'])
        for key,value in [('endpoint',1),('unit',1),('saved',False),('stable',False),('selected',False),('command_index',65535),('file','../bad'),('shader_size',262145),('sha256','bad')]:
            d=copy.deepcopy(self.d);d['postfilter']['records'][0]['colorgrade'][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):review(self.h,d,self.path.parent)
        d=copy.deepcopy(self.d);d['postfilter']['records'][0]['colorgrade']['input']['owner']=1
        with self.assertRaises(ValueError):review(self.h,d,self.path.parent)

    def test_colorgrade_header(self):
        h=json.loads(Path(sys.argv[2]).read_text().splitlines()[0]);h.update(version='1.23.10',hooks=25,postfilter_observer=True,postfilter_samples=3,
            postfilter_command_limit=32,postfilter_shader_limit=262144,postfilter_mutations=0,colorgrade_observer=True,colorgrade_scan_limit=65535,colorgrade_shader_limit=262144)
        self.assertEqual(parse(json.dumps(h).encode())[0],h)
        for key,value in [('colorgrade_observer',False),('colorgrade_scan_limit',65536),('colorgrade_shader_limit',262145)]:
            bad=dict(h);bad[key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):parse(json.dumps(bad).encode())

if __name__=='__main__':
    result=unittest.TextTestRunner().run(unittest.defaultTestLoader.loadTestsFromTestCase(PostfilterReview))
    raise SystemExit(not result.wasSuccessful())
