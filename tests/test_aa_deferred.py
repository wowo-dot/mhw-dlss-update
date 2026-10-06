import copy
import unittest
from test_aa_owned import fixture as owned_fixture, raw
from arm_aa_boundary import parse, validate
from review_aa_deferred import review


def fixture():
    rows = owned_fixture()
    rows[0].update(version='0.3.0', experiment='independent-aa-deferred-marker', write_scope='scoped-post-queue-and-one-empty-packet')
    rows[3]['hooks'] = 2
    requests = []
    for i, sample in enumerate(rows[4:12]):
        owned = sample['owned']; owned.update(published=True, packets_after=211)
        q = sample['before']
        inputs = [dict(role=j, owner=20+j, backing=80+j, resource=600+j, dimension=3, width=2560,
                       height=1440, format=26 if j == 0 else 10, mips=1, array=1, samples=1) for j in range(3)]
        requests.append(dict(index=i, generation=i+1, stage=4, reason=0, calls=1, packet=800+i, entry_at_publish=900+i,
                             key=(15 << 27) | 2, queue_limit=400, queue_capacity=1000, cached_batch=0,
                             source_frame=q['frame'], source_epoch=q['epoch'], source_bank=q['bank'],
                             renderer_frame_at_consume=q['frame']+1, publish_tick=200, consume_tick=205, thread=900,
                             executor=901, backend_context=902, recording=903, list=904, enqueued=True,
                             scope_restored=True, list_valid=True, owners_match=True, parameters_same=True,
                             native_returned=True, pins_released=True, inputs=inputs))
    rows.insert(-1, dict(event='deferred', requests=requests, fault=0, pending=0, hook_disabled=True,
                        private_gpu_commands=0, gpu_states_verified=False))
    return rows


class Cases(unittest.TestCase):
    def test_advance_does_not_relabel_source(self):
        result = review(raw(fixture()), 'a'*64)
        self.assertEqual(result['deferred_consumptions'], 8)
        self.assertFalse(result['gpu_completion_verified'])
        self.assertFalse(result['live_independent_dlaa_verified'])

    def test_arm_exact_version(self):
        rows = fixture()[:2]
        validate(parse(raw(rows), deferred=True), 7, 8, 'a'*64)
        with self.assertRaises(ValueError): parse(raw(rows), owned=True)

    def test_request_corruption(self):
        for key, value in (('generation', 2), ('stage', 2), ('calls', 2), ('key', 0), ('scope_restored', False),
                           ('owners_match', False), ('parameters_same', False), ('pins_released', False),
                           ('source_frame', 11), ('cached_batch', 4), ('queue_limit', 337)):
            rows = fixture(); rows[-2]['requests'][0][key] = value
            with self.assertRaises(ValueError, msg=key): review(raw(rows), 'a'*64)

    def test_duplicate_and_partial(self):
        for field in ('packet', 'generation'):
            rows = fixture(); rows[-2]['requests'][1][field] = rows[-2]['requests'][0][field]
            with self.assertRaises(ValueError): review(raw(rows), 'a'*64)
        rows = fixture(); rows[-2]['requests'].pop()
        with self.assertRaises(ValueError): review(raw(rows), 'a'*64)

    def test_wrong_resource(self):
        for key, value in (('resource', 601), ('width', 1920), ('format', 10), ('owner', 21)):
            rows = fixture(); rows[-2]['requests'][0]['inputs'][0][key] = value
            with self.assertRaises(ValueError): review(raw(rows), 'a'*64)

    def test_failure_or_scope(self):
        for key, value in (('pending', 1), ('fault', 3), ('hook_disabled', False), ('private_gpu_commands', 1), ('gpu_states_verified', True)):
            rows = fixture(); rows[-2][key] = value
            with self.assertRaises(ValueError): review(raw(rows), 'a'*64)


if __name__ == '__main__':
    unittest.main()
