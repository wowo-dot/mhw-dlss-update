import copy
import json
from pathlib import Path
import struct
import sys
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import review_jitter_sequence as seq
from review_submission import ReviewError
import test_submission_review as old


def fixture():
    source = old.SubmissionReviewTests(); source.setUp(); source.typed_projection_fixture()
    p = copy.deepcopy(source.capture['frame_packet']['producer'])
    packet = dict(schema=2, queued=False, consumer_bound=False, evaluate_bound=False, publication_bound=False,
                  temporal_alignment_verified=False, shader_jitter_verified=False, jitter_units_verified=False, producer=p)
    gates = dict(attempted=True, valid=True, predicate=True, reads=26, rejection=0, layer=500, wrapper_slot=600, wrapper=700,
                 vtable=0x140000000+0x351bda8, vtable_rva=0x351bda8, layer_type=8, layer_flags=0x12, view_flags=0,
                 blend_bits=struct.unpack('<I', struct.pack('<f', .95))[0], enabled=1, use_dlss=1, reset_vector_bits=[0, 0])
    rows = []
    for i in range(32):
        row = dict(schema=1, producer=1400, index=i, phase=i//16, tick=1000+i*100, valid=True,
                   duplicate=False, gpu_execution_verified=False, gates=copy.deepcopy(gates), frame_packet=copy.deepcopy(packet))
        p = row['frame_packet']['producer']; p['frame']=p['frame_after']=6+i
        p['current_index']=(6+i)%3; p['next_index']=(7+i)%3
        if i >= 16: row['gates'].update(wrapper=0, use_dlss=0)
        rows.append(row)
    return rows, dict(schema=1, producer=1400, count=32, present=16, absent=16, stop=1, started=1000)


class JitterSequenceReviewTests(unittest.TestCase):
    def setUp(self): self.rows, self.state = fixture()
    def reject(self, pattern):
        with self.assertRaisesRegex(ReviewError, pattern): seq.review_rows(self.rows, self.state)
    def test_two_phases_and_nonzero_jitter(self):
        r=seq.review_rows(self.rows,self.state)
        self.assertEqual([p['nonzero_jitter'] for p in r['phases']],[16,16])
        self.assertFalse(r['raster_jitter_verified'])
    def test_zero_jitter_is_independent_of_candidate(self):
        for s in self.rows:
            p=s['frame_packet']['producer']['projection']; p['jitter_bits']=[0,0];p['projection_bits']=p['unjittered_bits'].copy()
        self.assertEqual(seq.review_rows(self.rows,self.state)['phases'][0]['zero_jitter'],16)
    def test_missing_slots_and_budget_rejected(self):
        self.rows.pop(7);self.state['count']-=1;self.reject('sample identity')
    def test_subsample_spacing_rejected(self):
        self.rows[1]['tick']=1099;self.reject('spacing')
    def test_wrong_gate_predicate_rejected(self):
        self.rows[0]['gates']['enabled']=0;self.reject('predicate')
    def test_blocked_gate_still_valid_cpu_observation(self):
        self.rows[0]['gates'].update(enabled=0,predicate=False)
        r=seq.review_rows(self.rows,self.state);self.assertTrue(r['samples'][0]['valid'])
    def test_unknown_type_cannot_claim_valid(self):
        self.rows[0]['gates']['vtable_rva']+=8;self.reject('gate anchors')
    def test_invalid_observation_retained(self):
        self.rows[0]['gates'].update(valid=False,predicate=False,rejection=4,vtable_rva=0x1234);self.rows[0]['valid']=False
        self.assertEqual(seq.review_rows(self.rows,self.state)['phases'][0]['valid'],15)
    def test_duplicate_frame_cannot_claim_valid(self):
        self.rows[1]['frame_packet']['producer']=copy.deepcopy(self.rows[0]['frame_packet']['producer']);self.reject('duplicate frame')
    def test_partial_closed_trace_does_not_claim_complete_phase(self):
        self.rows=self.rows[:17];self.state.update(count=17,absent=1,stop=3)
        self.assertEqual(seq.review_rows(self.rows,self.state)['phases'][1]['samples'],1)
    def test_deadline_and_gpu_claim(self):
        self.rows[-1]['tick']=121000;self.reject('sequence time')
        self.rows[-1]['tick']=4100;self.rows[0]['gpu_execution_verified']=True;self.reject('GPU claim')
    def test_trace_requires_correct_version_and_state(self):
        text='trace.start version=0.14.0 owned-fixture=true\n'
        text+='\n'.join('bridge.jitter-sample '+json.dumps(s) for s in self.rows)+'\n'
        with self.assertRaisesRegex(ReviewError,'state missing'):seq.parse_trace(text)
        text+='bridge.jitter-sequence-state '+json.dumps(self.state)
        self.assertTrue(seq.parse_trace(text)['stopped'])
        with self.assertRaisesRegex(ReviewError,'trace version'):seq.parse_trace(text.replace('0.14.0','0.13.1'))


if __name__ == '__main__': unittest.main()
