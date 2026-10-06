from copy import deepcopy
from pathlib import Path
import sys
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
from continuous_prepared_queue import review


def sample():
    commands = dict(opcodes=[0]*32, draws=[])
    a = [3000, 4000, 2560, 1440, 26, 26, 1]
    b = [5000, 6000, 2560, 1440, 26, 26, 1]
    entries = []
    for descriptor, phase, order, unit, kind, source, target in (
        (2000+0x140, 15, 2, 2000, 123, a, b),
        (7000, 15, 17000, 8000, 999999, b, a),
        (9000, 15, 0x7fffff1, 0, 0, a, b),
    ):
        entries.append(dict(identity=[descriptor, phase, order, unit, kind], flags=[0, 0, 0],
                            input=source, output=target, commands=deepcopy(commands)))
    c = dict(stamp=[10, 9, 1000, 1100, 1200, 2000, 0, 1, 10000], packets=0,
             rectangle=[0, 0, 1280, 720], scale_bits=[0x3f000000, 0x3f800000, 0x3f000000],
             scratch=[a, b], entries=entries, chain_breaks=0, final_commands=commands)
    return dict(scaled_route=dict(prepared_queue=dict(schema=1, policy='native-prepared-color-chain-v1',
                diagnostic_only=True, interval=128, capacity=16, visits=1, attempts=1, accepted=1,
                results=[1, 0, 0, 0, 0, 0], samples=[dict(visit=1, attempt=1, capture=c)])))


class PreparedQueueTests(unittest.TestCase):
    def test_unknown_filter_and_scratch_reuse(self):
        self.assertEqual(review(sample())['accepted'], 1)

    def test_empty_and_failed_capture(self):
        s=sample(); r=s['scaled_route']['prepared_queue']
        r.update(visits=0, attempts=0, accepted=0, results=[0]*6, samples=[])
        review(s)
        r.update(visits=129, attempts=2, results=[0, 0, 0, 0, 1, 1])
        review(s)

    def test_corrupt_accounting(self):
        for key,value in [('interval',64),('capacity',32),('attempts',2),('accepted',0),
                          ('visits',129),('results',[2,0,0,0,0,0]),('samples',[]),('diagnostic_only',False)]:
            s=sample();s['scaled_route']['prepared_queue'][key]=value
            with self.subTest(key=key), self.assertRaises(ValueError):review(s)

    def test_corrupt_capture(self):
        for key,value in [('packets',65536),('scale_bits',[0x7fc00000]*3),
                          ('rectangle',[0,0,0,720]),('chain_breaks',1),('entries',[]),('scratch',[])]:
            s=sample();s['scaled_route']['prepared_queue']['samples'][0]['capture'][key]=value
            with self.subTest(key=key), self.assertRaises(ValueError):review(s)

    def test_broken_link_is_diagnostic(self):
        s=sample();c=s['scaled_route']['prepared_queue']['samples'][0]['capture']
        c['entries'][1]['input']=[12345,0,0,0,0,0,0];c['chain_breaks']=1
        review(s)
        c['chain_breaks']=0
        with self.assertRaises(ValueError):review(s)

    def test_unknown_metadata(self):
        s=sample();c=s['scaled_route']['prepared_queue']['samples'][0]['capture']
        c['scratch'][0]=[12345,0,0,0,0,0,0];review(s)
        c['scratch'][0][2]=2560
        with self.assertRaises(ValueError):review(s)

    def test_draw_coverage(self):
        s=sample();c=s['scaled_route']['prepared_queue']['samples'][0]['capture']
        c['packets']=3;c['final_commands']['opcodes'][0]=3
        c['final_commands']['draws']=[[1,2,0,0,2560,1440],[3,4,0,0,2560,1440]]
        review(s)
        c['final_commands']['draws'].pop()
        with self.assertRaises(ValueError):review(s)

    def test_temporal_identity(self):
        for index,value in [(0,123),(1,16),(2,3),(3,999)]:
            s=sample();s['scaled_route']['prepared_queue']['samples'][0]['capture']['entries'][0]['identity'][index]=value
            with self.subTest(index=index), self.assertRaises(ValueError):review(s)

    def test_duplicate_descriptor(self):
        s=sample();c=s['scaled_route']['prepared_queue']['samples'][0]['capture']
        c['entries'][1]['identity'][0]=c['entries'][0]['identity'][0]
        with self.assertRaises(ValueError):review(s)

    def test_monotonic_immutable_samples(self):
        previous=deepcopy(review(sample()));s=sample();r=s['scaled_route']['prepared_queue']
        r['visits']=128;review(s,previous)
        r['samples'][0]['capture']['stamp'][0]+=1
        with self.assertRaises(ValueError):review(s,previous)

    def test_first_sixteen_and_latest(self):
        s=sample();r=s['scaled_route']['prepared_queue'];base=deepcopy(r['samples'][0])
        r.update(visits=2433,attempts=20,accepted=20,results=[20,0,0,0,0,0])
        r['samples']=[]
        for attempt in [*range(1,17),20]:
            x=deepcopy(base);x.update(attempt=attempt,visit=(attempt-1)*128+1);r['samples'].append(x)
        review(s)
        r['samples'][-1]['visit']-=1
        with self.assertRaises(ValueError):review(s)


if __name__=='__main__':unittest.main()
