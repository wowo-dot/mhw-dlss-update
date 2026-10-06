import copy,json,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from temporal_downstream import review

class DownstreamReview(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.d=json.loads(Path(sys.argv[1]).read_text())
        cls.h={'base':cls.d['target']-0x2297e20}
        # Native fixture uses its own real ASM caller. Normalize only that
        # code-location field for the production executable identity check.
        actual=cls.d['expected_caller']
        assert actual>0 and all(r['caller']==actual for r in cls.d['records'])
        cls.d['expected_caller']=cls.h['base']+0x23c7b08
        for r in cls.d['records']:r['caller']=cls.d['expected_caller']
        cls.c={'downstream':cls.d,'records':[]}
        for q in cls.d['records']:
            taa=q['filters'][q['taa_index']]
            cls.c['records'].append(dict(**{k:q[k] for k in ('frame','epoch','bank','async','thread','context','view')},
                layer=taa['unit'],destination={'owner':q['taa_destination']},count_before=q['count_after']))
    def test_accept(self):
        result=review(self.h,self.c)
        self.assertTrue(result['downstream_cpu_construction_verified'])
        self.assertFalse(result['downstream_gpu_consumption_verified'])
        self.assertFalse(result['full_output_handoff_verified'])
    def test_frame_and_extent(self):
        for key,value in [('frame',999),('thread',0),('caller',1),('viewport',[0,0,9000,9000]),
                          ('rectangle',[0,0,9000,720]),('dimensions',[1280,720]),('offset',[float('nan'),0]),('triangle',2)]:
            c=copy.deepcopy(self.c);c['downstream']['records'][0][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):review(self.h,c)
    def test_sort_and_identity(self):
        for change in ('sort','packet','output','taa','filter'):
            c=copy.deepcopy(self.c);r=c['downstream']['records'][0]
            if change=='sort':r['sort']=15
            elif change=='packet':r['packets'][0]['key']=15<<27
            elif change=='output':r['view_output']=1
            elif change=='taa':r['taa_destination']=1
            else:r['filters'][0]['output']=1
            with self.subTest(change=change),self.assertRaises(ValueError):review(self.h,c)
    def test_cleanup_and_bounds(self):
        for key,value in [('selected',2),('fault',1),('mutations',1),('pending',1),('hooks_disabled',False),('gpu_consumption_verified',True)]:
            c=copy.deepcopy(self.c);c['downstream'][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):review(self.h,c)
        c=copy.deepcopy(self.c);c['downstream']['records'][0]['filters']*=17
        with self.assertRaises(ValueError):review(self.h,c)

if __name__=='__main__':
    result=unittest.TextTestRunner().run(unittest.defaultTestLoader.loadTestsFromTestCase(DownstreamReview))
    raise SystemExit(not result.wasSuccessful())
