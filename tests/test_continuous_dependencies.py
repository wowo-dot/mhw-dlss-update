import json,sys,unittest
from pathlib import Path
from copy import deepcopy
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from continuous_dependencies import review, review_intermediate
from continuous_compute_dependencies import review as review_compute
FIXTURE=None
class Tests(unittest.TestCase):
    def setUp(self):self.cases={x['case']:deepcopy(x['trace']) for x in FIXTURE}
    def test_native_captures(self):
        for t in self.cases.values():(review_compute if t['schema'] in (2,3) else review)(t)
        review(self.cases['full'],self.cases['dependency'])
    def test_dependency(self):
        a,b=self.cases['dependency']['samples'][0]['draws']
        self.assertEqual(a['target'][5],b['textures'][0]['fields'][3])
        self.assertNotEqual(a['target'][5],b['target'][5])
    def test_scope(self):
        t=self.cases['dependency'];t['samples'][0]['gpu_verified']=True
        with self.assertRaises(ValueError):review(t)
    def test_binding_identity(self):
        for key in ('index','fields'):
            t=deepcopy(self.cases['dependency']);x=t['samples'][0]['draws'][0]['textures'][0]
            if key=='index':x[key]=1
            else:x[key][0]+=8
            with self.assertRaises(ValueError):review(t)
    def test_shader_corruption(self):
        for field,value in [('hex','00'*52),('size',53),('identity',[0,1,2])]:
            t=deepcopy(self.cases['dependency']);t['samples'][0]['draws'][0]['shaders'][0][field]=value
            with self.assertRaises(ValueError):review(t)
    def test_order_and_accounting(self):
        t=self.cases['dependency'];t['samples'][0]['draws'].reverse()
        with self.assertRaises(ValueError):review(t)
        t=self.cases['full'];t['attempts']+=1
        with self.assertRaises(ValueError):review(t)
    def test_immutable(self):
        p=self.cases['dependency'];t=deepcopy(self.cases['full']);t['samples'][0]['at'][0]+=1
        with self.assertRaises(ValueError):review(t,p)
    def test_vertex_overflow(self):
        t=self.cases['dependency'];t['samples'][0]['draws'][0]['vertex'][2]=16777217
        with self.assertRaises(ValueError):review(t)
    def test_intermediate_connection(self):
        t=deepcopy(self.cases['intermediate']);sample=t['samples'][0]
        self.assertEqual(sample['key'],0x78006595)
        self.assertGreater(sample['connection'][1],0)
        self.assertNotEqual(sample['connection'][1],sample['connection'][2])
        review(self.cases['intermediate-full'],t)
        for value in ([0,1,sample['destination']],[1,2,0],[1,2]):
            bad=deepcopy(t);bad['samples'][0]['connection']=value
            with self.assertRaises(ValueError):review(bad)
        # Old snapshots without the new optional receipt retain their schema.
        del sample['connection'];review(t)
        with self.assertRaises(ValueError):review_intermediate(t)
        for value in ([1,0,sample['destination']],[1,sample['destination'],sample['destination']]):
            bad=deepcopy(self.cases['intermediate']);bad['samples'][0]['connection']=value
            with self.assertRaises(ValueError):review_intermediate(bad)
        review_intermediate(self.cases['intermediate-full'],self.cases['intermediate'])
    def test_compute_bundle(self):
        t=self.cases['compute-bundle'];review_compute(self.cases['compute-full'],t)
        s=t['samples'][0]
        self.assertEqual([c[2]&31 for c in s['commands']],[11,6,6,6,6,6,6,0])
        self.assertEqual([d['prefix'][6:9] for d in s['computes']],[[8,1440,1],[128,5,1],[16,9,1]]+[[80,45,1]]*3)
    def test_compute_coverage(self):
        for mutate in (lambda s:s['commands'].pop(3),lambda s:s['commands'].reverse(),
                       lambda s:s['computes'].pop(),lambda s:s['commands'][3].__setitem__(3,0)):
            t=deepcopy(self.cases['compute-bundle']);mutate(t['samples'][0])
            with self.assertRaises(ValueError):review_compute(t)
    def test_compute_resources(self):
        for mutate in (lambda d:d['views'][0].__setitem__(1,0),lambda d:d['textures'][0]['fields'].__setitem__(0,1),
                       lambda d:d['words'].__setitem__(0,1026),lambda d:d['prefix'].__setitem__(9,1),
                       lambda d:d['prefix'].__setitem__(10,1)):
            t=deepcopy(self.cases['compute-bundle']);mutate(t['samples'][0]['computes'][0])
            with self.assertRaises(ValueError):review_compute(t)
    def test_compute_shader_and_scope(self):
        for field,value in [('hex','00'*52),('size',65537),('identity',[0,1,2])]:
            t=deepcopy(self.cases['compute-bundle']);t['samples'][0]['computes'][0]['shader'][field]=value
            with self.assertRaises(ValueError):review_compute(t)
        t=deepcopy(self.cases['compute-bundle']);t['samples'][0]['gpu_verified']=True
        with self.assertRaises(ValueError):review_compute(t)
        t=deepcopy(self.cases['compute-full']);t['samples'][0]['computes'][0]['technique']+=1
        with self.assertRaises(ValueError):review_compute(t,self.cases['compute-bundle'])
    def test_compute_extract(self):
        import tempfile
        from extract_compute_dependencies import extract
        with tempfile.TemporaryDirectory() as td:
            result=extract(self.cases['compute-bundle'],Path(td)/'out')
            self.assertEqual(len(result['shader_files']),3)
            self.assertEqual([x['producer'] for x in result['edges']],[1,2,3,4,5,5])
            self.assertEqual(result['edges'][-1]['stage'],'ps')
            self.assertFalse(result['gpu_verified'])
            self.assertEqual(len(result['constants']),19)
            self.assertEqual(len(result['constant_files']),3)
            screen=next(c for c in result['constants'] if c['name']=='CBScreen')
            self.assertEqual(screen['screen_size'][0],2560)
            self.assertEqual(screen['view_size'][0],1280)
    def test_constants(self):
        t=self.cases['compute-bundle'];d=t['samples'][0]['computes'][0]
        self.assertEqual([x['size'] for x in d['constants']],[1072,112,96])
        for mutate in (lambda d:d['constants'].pop(),lambda d:d['constants'][0].__setitem__('size',1071),
                       lambda d:d['owners'][3].__setitem__(1,1),lambda d:d['owners'][3].__setitem__(2,1),
                       lambda d:d['constants'][1].__setitem__('hex','00'),lambda d:d['words'].__setitem__(3,0xb400)):
            bad=deepcopy(t);mutate(bad['samples'][0]['computes'][0])
            with self.assertRaises(ValueError):review_compute(bad)
if __name__=='__main__':
    FIXTURE=[json.loads(x) for x in Path(sys.argv.pop(1)).read_text(encoding='utf-8-sig').splitlines()]
    unittest.main()
