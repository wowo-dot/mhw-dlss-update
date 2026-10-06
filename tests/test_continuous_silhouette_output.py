import copy,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from continuous_silhouette_output import review

class OutputTests(unittest.TestCase):
    def sample(self):
        return dict(schema=1,policy='indexed-native-coordinate-adapter-v1',planned=20,recorded=20,native=4,corrected=16,restored=16,pipelines=5,rejections=[0,0,4]+[0]*9)
    def test_complete(self):review(self.sample(),final=True)
    def test_inflight(self):
        s=self.sample();s['planned']=30;review(s)
        with self.assertRaises(ValueError):review(s,final=True)
    def test_unrestored(self):
        s=self.sample();s['restored']-=1
        with self.assertRaises(ValueError):review(s)
    def test_coverage(self):
        s=self.sample();s['corrected']+=1
        with self.assertRaises(ValueError):review(s)
    def test_rejections(self):
        s=self.sample();s['rejections'][2]=3
        with self.assertRaises(ValueError):review(s)
    def test_rollback(self):
        old=self.sample();s=copy.deepcopy(old);old['planned']+=1
        with self.assertRaises(ValueError):review(s,old)
    def test_cache_bound(self):
        s=self.sample();s['pipelines']=33
        with self.assertRaises(ValueError):review(s)
    def test_boolean_counter(self):
        s=self.sample();s['pipelines']=True
        with self.assertRaises(ValueError):review(s)
if __name__=='__main__':unittest.main()
