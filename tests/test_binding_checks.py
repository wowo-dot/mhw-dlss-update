import sys,json,unittest
from pathlib import Path
from copy import deepcopy
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from continuous_binding_checks import review
FIXTURES=[json.loads(x) for x in Path(sys.argv.pop(1)).read_text().splitlines()]
class BindingChecks(unittest.TestCase):
 def test_fifth_texture_schema(self):
  v=deepcopy(FIXTURES[0]);v.update(schema=2,policy='dependency-binding-boundaries-v2')
  s=v['samples'][0];s['at'][4]=3
  s['draw'].update(fields=512,differences=1,changes=[[9,4,100,132]],equal=False)
  review(v)
  old=deepcopy(v);old.update(schema=1,policy='dependency-binding-boundaries-v1')
  with self.assertRaises(ValueError):review(old)
  v['samples'][0]['draw']['changes'][0][1]=5
  with self.assertRaises(ValueError):review(v)
 def test_real_cpp_receipts(self):
  for v in FIXTURES:self.assertEqual(review(v),v)
 def test_corruptions(self):
  mutations=[lambda t:t.update(capacity=4),lambda t:t.update(diagnostic_only=False),lambda t:t.update(draw_rejections=0),
   lambda t:t['samples'][0]['at'].__setitem__(3,12),lambda t:t['samples'][0]['draw'].update(readable=False),
   lambda t:t['samples'][0]['draw'].update(equal=True),lambda t:t['samples'][0]['draw'].update(fields=512),
   lambda t:t['samples'][0]['draw']['changes'][0].__setitem__(0,11),lambda t:t['samples'][0]['draw']['changes'].pop()]
  for mutate in mutations:
   with self.subTest(mutation=mutate):
    v=deepcopy(FIXTURES[0]);mutate(v)
    with self.assertRaises((ValueError,KeyError)):review(v)
 def test_mutation_and_rollback(self):
  v=deepcopy(FIXTURES[0]);v['samples'][0]['at'][1]+=1
  with self.assertRaises(ValueError):review(v,FIXTURES[0])
  v=deepcopy(FIXTURES[0]);v['entry_checks']-=1
  with self.assertRaises(ValueError):review(v,FIXTURES[0])
if __name__=='__main__':unittest.main()
