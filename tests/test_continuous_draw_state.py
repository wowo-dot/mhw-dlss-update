from pathlib import Path
from copy import deepcopy
import sys,unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from continuous_draw_state import review

class DrawStateTests(unittest.TestCase):
    def status(self,mask=2):
        s=dict(mask=mask,at=[1,2,2,0,50,60,7],values=[1,0,0,10,10,1,1,20,20,30,30,40,40])
        return dict(scaled_route=dict(draw_state_failures=dict(schema=1,diagnostic_only=True,count=1,samples=[s])))
    def test_unknown_state(self):review(self.status())
    def test_all_predicates(self):
        for index,value,mask in [(0,0,1),(2,8,4),(4,11,8),(5,0,16),(6,0,32),(8,21,64),(10,31,128),(12,41,256)]:
            s=self.status(mask);v=s['scaled_route']['draw_state_failures']['samples'][0]['values']
            v[1]=1;v[2]=4;v[index]=value;review(s)
    def test_corrupt_mask(self):
        with self.assertRaises(ValueError):review(self.status(4))
    def test_no_fault(self):
        s=self.status();s['scaled_route']['draw_state_failures'].update(count=0,samples=[]);review(s)
    def test_retention(self):
        s=self.status();p=review(s);s['scaled_route']['draw_state_failures'].update(count=2,samples=[deepcopy(p['samples'][0]),deepcopy(p['samples'][0])]);review(s,p)
        s['scaled_route']['draw_state_failures']['samples'][0]['at'][0]=10
        with self.assertRaises(ValueError):review(s,p)

if __name__=='__main__':unittest.main()
