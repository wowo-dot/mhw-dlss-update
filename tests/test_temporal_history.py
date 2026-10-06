"""Combined native recorder review; SDK is tested separately on the RTX device."""
import copy
import json
from pathlib import Path
import sys
import unittest
from unittest.mock import patch

import test_temporal_inputs as native
review=native.review

class HistoryCaptureTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        native.ReviewTests.setUpClass()
        cls.rows=copy.deepcopy(native.ReviewTests.rows)
        cls.shader=native.ReviewTests.shader
        cls.rows[0].update(version='1.14.0',hooks=11,jitter_writes=1,
            sdk_evaluation_limit=3,sdk_create_limit=1,sdk_image_byte_limit=150994944,
            sdk_output_limit=201326592,sdk_shutdown_deferred=True)
        cls.n=len(cls.rows[4]['records'])
        if cls.n==9:
            cls.rows[0].update(version='1.15.0',capacity=9,jitter_writes=7,gpu_commands=108,
                private_signal_limit=9,motion_copy_limit=9,color_copy_limit=9,
                total_readback_limit=452984832,projection_samples=9,jitter_restorations=7,
                scene_depth_samples=9,scene_depth_copies=9,depth_copy_limit=9,
                owned_copy_limit=27,owned_descriptor_writes=54,owned_execute_limit=9,
                owned_signal_limit=9,owned_dispatch_limit=9,owned_readback_limit=1056964608,
                sdk_evaluation_limit=9,sdk_output_limit=603979776)
            if 'publication' in cls.rows[4]['native_compute']['records'][0]:
                cls.rows[0].update(version='1.15.1',packet_generations=True)
            if 'drained_before_disable' in cls.rows[4]['depth_routes']:
                cls.rows[0].update(version='1.15.2',journal_drain=True)
            if 'shutdown' in cls.rows[4]:
                cls.rows[0].update(version='1.15.3',pre_unhook_cutoff=True)
            if 'native_bindings' in cls.rows[4]:
                cls.rows[0].update(version='1.16.0',hooks=29,binding_observer=True,binding_hooks=18,
                    binding_lists=128,binding_arguments=64,binding_constant_words=64,presentation_writes=0)

    def run_native(self,rows):
        rows=review.parse(('\n'.join(json.dumps(r) for r in rows)).encode())
        # No SDK exists on this WARP fixture. Exercise the complete native
        # validation path, then check that the SDK handoff is mandatory.
        with patch.object(review,'review_live_sdk',return_value={'sdk_evaluation_verified':False}) as sdk:
            result=review.review(rows,self.shader)
            sdk.assert_called_once_with(rows[0],rows[4],self.shader.parent)
        return result

    def test_combined_native(self):
        r=self.run_native(self.rows)
        self.assertTrue(r['scoped_projection_stimulus_verified'])
        self.assertTrue(r['projection_taa_request_association_verified'])
        self.assertTrue(r['live_owned_copy_integration_verified'])
        self.assertEqual(r['owned_compared_float_values'],17*13*7*self.n)
        self.assertFalse(r['sdk_evaluation_verified'])
        self.assertFalse(r['dlaa_ready'])

    def test_missing_sdk_rejected(self):
        with self.assertRaises(KeyError):review.review(self.rows,self.shader)

    def test_frame_mismatch(self):
        for field in ('frame','epoch','bank','async','context','view'):
            r=copy.deepcopy(self.rows);r[4]['projection_stimulus']['records'][1][field]+=1
            with self.subTest(field=field),self.assertRaises(ValueError):self.run_native(r)

    def test_incomplete_restoration(self):
        for key,value in [('restorations',0),('pending',1),('fault',1),('hooks_disabled',False)]:
            r=copy.deepcopy(self.rows);r[4]['projection_stimulus'][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):self.run_native(r)

    def test_no_bypass_for_new_version(self):
        for section in ('projection_stimulus','owned_gpu_inputs','depth_readback','motion_readback','color_readback'):
            r=copy.deepcopy(self.rows);del r[4][section]
            with self.subTest(section=section),self.assertRaises((KeyError,ValueError)):self.run_native(r)

    def test_budgets(self):
        for key in ('hooks','jitter_writes','projection_samples','jitter_restorations','jitter_selection_limit','sdk_evaluation_limit'):
            r=copy.deepcopy(self.rows);r[0][key]+=1
            with self.subTest(key=key),self.assertRaises(ValueError):self.run_native(r)

    def test_late_jitter_damage(self):
        if self.n!=9:self.skipTest('nine-frame fixture required')
        for field in ('frame','epoch','candidate','jitter_bits','restored'):
            r=copy.deepcopy(self.rows);p=r[4]['projection_stimulus']['records']
            p[7][field]=False if field=='restored' else copy.deepcopy(p[1][field])
            with self.subTest(field=field),self.assertRaises(ValueError):self.run_native(r)

    def test_truncated_sequence(self):
        r=copy.deepcopy(self.rows);r[4]['records'].pop()
        with self.assertRaises(ValueError):self.run_native(r)

    def test_packet_generation_rejections(self):
        if self.rows[0]['version'] not in ('1.15.1','1.15.2','1.15.3','1.16.0'):self.skipTest('generation fixture required')
        for i in range(self.n):
            for field,bad in (('publication',0),('producer_begin',0),('packet_generation',0),('packet_predecessor',self.n),('returned',False)):
                r=copy.deepcopy(self.rows);r[4]['native_compute']['records'][i][field]=bad
                with self.subTest(i=i,field=field),self.assertRaises(ValueError):self.run_native(r)
        r=copy.deepcopy(self.rows);r[0]['packet_generations']=False
        with self.assertRaises(ValueError):self.run_native(r)
        r=copy.deepcopy(self.rows);r[4]['records'][1]['producer_order']+=1
        with self.assertRaises(ValueError):self.run_native(r)

    def test_reused_packet_completion(self):
        if self.rows[0]['version'] not in ('1.15.1','1.15.2','1.15.3','1.16.0'):self.skipTest('generation fixture required')
        records=self.rows[4]['native_compute']['records']
        reused=[i for i,r in enumerate(records) if r['packet_predecessor']!=0xffffffff]
        if not reused:self.skipTest('reused packet fixture required')
        result=self.run_native(self.rows)
        self.assertTrue(result['packet_generations_verified'])
        self.assertEqual(result['packet_address_reuses'],len(reused))
        for i in reused:
            previous=records[i]['packet_predecessor']
            for field,bad in [('producer_begin',records[previous]['exit']),('publication',records[previous]['exit']),('packet_predecessor',0xffffffff),
                              ('packet_generation',records[previous]['packet_generation'])]:
                r=copy.deepcopy(self.rows);r[4]['native_compute']['records'][i][field]=bad
                if field=='producer_begin':r[4]['records'][i]['producer_order']=bad
                with self.subTest(i=i,field=field),self.assertRaises(ValueError):self.run_native(r)
        r=copy.deepcopy(self.rows);r[0].update(version='1.15.0',hooks=11)
        with self.assertRaisesRegex(ValueError,'packet reused'):self.run_native(r)

    def test_drain_evidence_required(self):
        if self.rows[0]['version'] not in ('1.15.2','1.15.3','1.16.0'):self.skipTest('drain fixture required')
        for section in ('native_compute','depth_routes'):
            for value in (False,0,None):
                r=copy.deepcopy(self.rows);r[4][section]['drained_before_disable']=value
                with self.subTest(section=section,value=value),self.assertRaises(ValueError):self.run_native(r)
        r=copy.deepcopy(self.rows);del r[0]['journal_drain']
        with self.assertRaises(ValueError):self.run_native(r)

    def test_no_missing_marker_exception(self):
        if self.rows[0]['version'] not in ('1.15.2','1.15.3','1.16.0'):self.skipTest('drain fixture required')
        for i in range(len(self.rows[4]['depth_routes']['consumers'])):
            for field,value in (('marker_index',0xffffffff),('marker_failure',2),('marker_failure',None),('marker_failure',False)):
                r=copy.deepcopy(self.rows);r[4]['depth_routes']['consumers'][i][field]=value
                with self.subTest(i=i,field=field,value=value),self.assertRaises(ValueError):self.run_native(r)

    def test_cutoff_witnesses_required(self):
        if self.rows[0]['version'] not in ('1.15.3','1.16.0'):self.skipTest('cutoff fixture required')
        for field in ('callbacks_drained','journal_stopped_before_unhook'):
            r=copy.deepcopy(self.rows);r[4]['shutdown'][field]=False
            with self.subTest(field=field),self.assertRaises(ValueError):self.run_native(r)
        for field in ('drain_started_tick','drain_finished_tick','journal_stopped_tick','unhook_started_tick','finish_tick'):
            r=copy.deepcopy(self.rows);r[4]['shutdown'][field]=0
            with self.subTest(field=field),self.assertRaises(ValueError):self.run_native(r)
        r=copy.deepcopy(self.rows);r[4]['shutdown']['unhook_started_tick']=r[4]['shutdown']['journal_stopped_tick']-1
        with self.assertRaises(ValueError):self.run_native(r)
        r=copy.deepcopy(self.rows);r[4]['submission']['admission_closed']=False
        with self.assertRaises(ValueError):self.run_native(r)
        r=copy.deepcopy(self.rows);r[4]['submission']['stopped_tick']+=1
        with self.assertRaises(ValueError):self.run_native(r)
        r=copy.deepcopy(self.rows);del r[0]['pre_unhook_cutoff']
        with self.assertRaises(ValueError):self.run_native(r)

if __name__=='__main__':
    sys.argv_fixture=sys.argv.pop(1)
    unittest.main()
