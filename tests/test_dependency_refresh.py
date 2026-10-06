import sys, unittest
from pathlib import Path
from copy import deepcopy
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from continuous_dependency_refresh import review

def empty():
    return dict(schema=1, policy='auxiliary-entry-retention-v1', entries=0, unchanged=0,
                refreshed=0, rejected=0, api_checks=0, api_rejected=0, reason=0, last=[0]*7)

class RefreshTests(unittest.TestCase):
    def test_admission_and_failure_receipts(self):
        a = empty(); review(a)
        b = dict(a, entries=10, unchanged=9, refreshed=1, api_checks=10, last=[16266,5,2,128,256,128,256])
        review(b, a)
        review(dict(b, entries=11, rejected=1, api_checks=11, api_rejected=1, reason=3), b)
    def test_corruptions(self):
        a = dict(empty(), entries=1, refreshed=1, api_checks=1, last=[77,5,2,128,256,128,256])
        for key, value in [('entries',2),('api_checks',2),('api_rejected',2),('reason',1),('refreshed',0),('last',[77,5,0,128,256,128,256])]:
            with self.subTest(key=key), self.assertRaises(ValueError): review(dict(a, **{key:value}))
    def test_history(self):
        a = dict(empty(), entries=1, refreshed=1, api_checks=1, last=[77,5,2,128,256,128,256])
        with self.assertRaises(ValueError): review(empty(), a)
        b = deepcopy(a); b['last'][0] += 1
        with self.assertRaises(ValueError): review(b, a)

if __name__ == '__main__': unittest.main()
