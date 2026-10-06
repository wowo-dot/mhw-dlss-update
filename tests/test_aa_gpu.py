import copy
import unittest
from test_aa_deferred import fixture as deferred_fixture
from test_aa_owned import raw
from arm_aa_boundary import parse
from review_aa_gpu import review


def fixture():
    rows = deferred_fixture()
    rows[0].update(version='0.4.0', experiment='independent-aa-gpu-trace', private_queue_signals=8, gpu_state_trace=True)
    rows[3]['hooks'] = 6
    events, calls, samples, records = [], [], [], []
    counter = 0

    def event(kind, obj, allocator=0, lists=None):
        nonlocal counter
        e = dict(kind=kind, object=obj, allocator=allocator, enter=counter+1, exit=counter+2,
                 returned=True, valid=True, result=0, lists=lists or [])
        counter += 2; events.append(e)
        return e

    for i, request in enumerate(rows[12]['requests']):
        request['list'] = 904
        for j, fmt in enumerate((26, 41, 42)):request['inputs'][j]['format'] = fmt
        reset = event(0, 904, 990)
        token = len(events); barrier = event(5, 904)
        marker = event(3, 904); close = event(1, 904); execute = event(2, 1001, lists=[904])
        signal, signal_exit, completed_order = counter+1, counter+2, counter+3; counter += 3
        retire = event(0, 904, 991)
        calls.append(dict(token=token, command=904, reset=reset['enter'], enter=barrier['enter'], returned=True, unchanged=True,
                          barriers=[dict(type=0, flags=0, resource=600+j, subresource=0xffffffff, before=0, after=2048) for j in range(3)]))
        samples.append(dict(receipt=i, frame=request['source_frame'], packet=request['packet'], command=904,
                            reset=reset['enter'], allocator=990, dispatch=marker['enter'], dispatch_exit=marker['exit'],
                            close=close['enter'], execute=execute['enter'], execute_exit=execute['exit'], queue=1001,
                            retire=retire['enter'], retire_exit=retire['exit'], fence=10000+i, signal_attempts=1, signal_result=0,
                            signal=signal, signal_exit=signal_exit, completed_order=completed_order, signaled=True, completed=True, completed_value=1))
        records.append(dict(receipt=i, frame=request['source_frame'], packet=request['packet'], command=904,
                            reset=reset['enter'], allocator=990, marker=marker['enter'], captured=True,
                            states=[dict(known=True, state=2048, issue=0, transitions=1, last=barrier['enter']*1025+j+1) for j in range(3)]))
    report = dict(event='gpu_trace', fault=0, pending=0, hooks_disabled=True, drained=True, qualified=True, completed=True,
                  journal_released=True, watch_released=True, watched=[600, 601, 602], records=records, barrier_calls=calls,
                  submission=dict(fault=0, pending=0, hooks_disabled=True, admission_closed=True, retained=False,
                                  pins=30, released=30, copies_enabled=False, colors_enabled=False, depths_enabled=False,
                                  events=events, samples=samples))
    rows.insert(-1, report)
    return rows


class Cases(unittest.TestCase):
    def test_reconstruct(self):
        result = review(raw(fixture()), 'a'*64)
        self.assertEqual(result['input_states_at_marker'], [[2048]*3]*8)
        self.assertTrue(result['gpu_completion_verified'])
        self.assertFalse(result['pixel_freshness_verified'])
        self.assertFalse(result['live_independent_dlaa_verified'])

    def test_arm_version_and_scope(self):
        rows = fixture()[:2]
        parse(raw(rows), gpu=True)
        with self.assertRaises(ValueError): parse(raw(rows), deferred=True)
        rows[0]['private_queue_signals'] = 0
        with self.assertRaises(ValueError): parse(raw(rows), gpu=True)

    def test_cannot_trust_declared_state(self):
        for key, value in (('state', 4), ('known', False), ('last', 5), ('transitions', 9), ('issue', 2)):
            rows = fixture(); rows[13]['records'][0]['states'][0][key] = value
            with self.assertRaises(ValueError): review(raw(rows), 'a'*64)

    def test_barrier_generation_split_alias(self):
        for change in ('generation', 'split', 'alias', 'missing', 'after_marker', 'unreturned', 'changed'):
            rows = fixture(); report = rows[13]; call = report['barrier_calls'][0]
            if change == 'generation':call['reset'] += 1
            elif change == 'split':call['barriers'][0]['flags'] = 1
            elif change == 'alias':call['barriers'][0] = dict(type=1, flags=0, before=0, after=600)
            elif change == 'missing':call['barriers'].pop()
            elif change == 'after_marker':call['enter'] = report['records'][0]['marker']+1
            elif change == 'unreturned':call['returned'] = False
            elif change == 'changed':call['unchanged'] = False
            with self.assertRaises(ValueError, msg=change): review(raw(rows), 'a'*64)

    def test_fence_and_retirement_rejection(self):
        for key, value in (('completed_value', 0), ('signal_attempts', 2), ('retire', 0), ('retire_exit', 1), ('signal', 1), ('fence', 0), ('allocator', 5)):
            rows = fixture(); rows[13]['submission']['samples'][0][key] = value
            with self.assertRaises(ValueError, msg=key): review(raw(rows), 'a'*64)

    def test_resource_and_closure(self):
        for change in ('resource', 'format', 'release', 'pending'):
            rows = fixture()
            if change == 'resource':rows[12]['requests'][0]['inputs'][2]['resource'] += 1
            elif change == 'format':rows[12]['requests'][0]['inputs'][2]['format'] = 41
            elif change == 'release':rows[13]['submission']['released'] -= 1
            else:rows[13]['pending'] = 1
            with self.assertRaises(ValueError): review(raw(rows), 'a'*64)


if __name__ == '__main__':unittest.main()
