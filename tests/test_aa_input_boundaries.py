import copy
import sys
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from aa_input_boundaries import review_boundary,review_boundaries


def fixture():
    inputs=[dict(tracked=True,resource=r,state=dict(known=True,value=128,issue=0,last=0)) for r in (10,20,30)]
    return dict(schema=1,native_nodes=4,resources=[10,20,30],list=40,generation=1,frame=123,
                count=2,marker_order=0,begin_order=0,end_order=2,marker=copy.deepcopy(inputs),before=copy.deepcopy(inputs),after=copy.deepcopy(inputs),
                batch=[dict(type=0,flags=0,resource=r,subresource=0xffffffff,before=4,after=128) for r in (10,30)],
                relevant=2,color=True,motion=True,invalid_states=0,accepted=True)


class Boundaries(unittest.TestCase):
    def test_good_and_mutations(self):
        s=fixture();self.assertTrue(review_boundary(s)['accepted'])
        for field,value in dict(flags=1,resource=20,subresource=0,before=0,after=64).items():
            bad=copy.deepcopy(s);bad['batch'][0][field]=value
            with self.assertRaises(ValueError):review_boundary(bad)
        for key,value in dict(relevant=3,motion=False,invalid_states=4,count=3,end_order=1).items():
            bad=copy.deepcopy(s);bad[key]=value
            with self.assertRaises(ValueError):review_boundary(bad)
    def test_precise_partial(self):
        s=fixture();s['batch'].pop();s.update(count=1,end_order=1,relevant=1,motion=False,accepted=False,invalid_states=4)
        s['after'][2]=dict(tracked=False,resource=0,state=dict(known=False,value=0,issue=1,last=0))
        self.assertEqual(review_boundary(s)['invalid_states'],4)
        s['accepted']=True
        with self.assertRaises(ValueError):review_boundary(s)
    def test_independent_states(self):
        for i in range(3):
            s=fixture();s['after'][i]['state']['value']=4
            with self.assertRaises(ValueError):review_boundary(s)
    def test_whole_run_scope(self):
        c=dict(input_boundaries=dict(policy='exact-color-motion-v1',first_base=fixture(),first_haze=None,first_rejection=None),
               scheduler_layouts=dict(base_evaluations=100,haze_evaluations=0))
        review_boundaries(c)
        c['input_boundaries']['first_rejection']=fixture()
        with self.assertRaises(ValueError):review_boundaries(c)
    def test_tracking_identity(self):
        for key in ('marker','before','after'):
            s=fixture();s[key][0]['resource']=999
            with self.assertRaises(ValueError):review_boundary(s)


if __name__=='__main__':unittest.main()
