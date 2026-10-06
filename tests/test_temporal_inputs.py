"""Review/armer adversarial tests using a real owned-resource probe fixture."""
import copy,json,sys,unittest,tempfile
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import review_temporal_inputs as review
from temporal_shader import compute_dxbc
from temporal_consumer import derive_states
from arm_temporal_inputs import validate
from temporal_motion import decode_word,history_uv_delta
from temporal_color import decode_color

class ReviewTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        captured=json.loads(Path(sys.argv_fixture).read_text())
        # The fixture has real D3D12 resources and native recorder output, but
        # no game/swapchain. Explicit synthetic outer identity for pure tests.
        cls.shader=Path(sys.argv_fixture+'.cs.dxbc')
        cls.rows=[dict(event='start',schema=1,version='1.1.0',host_sha256=review.HOST,host_version='0.0.0.0',
            experiment='ordinary-taa-input-requests',capacity=3,window_ms=2000,jitter_writes=0,gpu_commands=0,hooks=1,
            shader_limit=262144,owner_limit=64,binding_limit=128,shader_file='inputs-123-456.cs.dxbc',
            pid=123,creation=456,base=captured['records'][0]['caller']-0x2392f31,dll_sha256='a'*64),
            dict(event='reservation',ready=True,slot=1,hooks_installed=False),dict(event='ready',hooks_installed=False),
            dict(event='armed',installed=True,reason='',tick=1),captured,
            dict(event='stopped',armed=True,accepted=True,settled=True,dll_pinned=True,trampolines_retained=True)]
        captured.update(swapchain_format=87,width=2560,height=1440)
        if 'native_compute' in captured:
            cls.rows[0].update(version='1.2.0',hooks=4,barrier_limit=128,barrier_call_limit=16)
        if 'submission' in captured:
            cls.rows[0].update(version='1.3.0',hooks=7,warmup_ms=150,event_limit=4096,list_limit=128,
                allocator_limit=128,queue_limit=8,batch_limit=128,submitted_limit=8192,private_signal_limit=3)
        if captured.get('motion_readback',{}).get('enabled'):
            cls.rows[0].update(version='1.4.0',gpu_commands=9,motion_copy_limit=3,motion_byte_limit=16777216,motion_prefix='inputs-123-456')
            captured.update(width=17,height=13)
        if captured.get('color_readback',{}).get('enabled'):
            cls.rows[0].update(version='1.5.0',gpu_commands=18,color_copy_limit=3,color_byte_limit=16777216,total_readback_limit=100663296)
        if 'projection_stimulus' in captured:
            cls.rows[0].update(version='1.6.0',jitter_writes=1,hooks=8,projection_samples=3,projection_byte_limit=4096,jitter_restorations=1)
        if 'selection_attempts' in captured.get('projection_stimulus',{}):
            cls.rows[0].update(version='1.6.1',jitter_min_abs=.3125,jitter_selection_limit=20)
        if 'scene_depth' in captured['records'][0]:
            cls.rows[0].update(version='1.7.0',scene_depth_samples=3,scene_depth_copies=0,descriptor_writes=0)
        if 'depth_routes' in captured:
            cls.rows[0].update(version='1.8.0',hooks=10,depth_route_limit=64,depth_route_barrier_limit=32,depth_route_call_limit=8)
        if captured.get('depth_readback',{}).get('enabled'):
            cls.rows[0].update(version='1.9.0',gpu_commands=27,scene_depth_copies=3,depth_copy_limit=3,depth_byte_limit=16777216,depth_selected_scope=True,total_readback_limit=150994944)
        if captured.get('owned_gpu_inputs',{}).get('enabled'):
            cls.rows[0].update(version='1.10.0',gpu_commands=36,owned_copy_limit=9,owned_descriptor_writes=18,
                owned_set_byte_limit=314572800,owned_execute_limit=3,owned_signal_limit=3,
                owned_dispatch_limit=3,owned_readback_limit=352321536)
            owned=captured['owned_gpu_inputs']['sets'][0]
            captured.update(adapter_luid_low=owned['adapter_luid_low'],adapter_luid_high=owned['adapter_luid_high'])
    def owned_rows(self):
        if self.rows[0]['version']!='1.10.0':self.skipTest('owned fixture required')
        return copy.deepcopy(self.rows)
    def test_owned_exact_values(self):
        rows=self.owned_rows();r=review.review(rows,self.shader)
        self.assertTrue(r['live_owned_copy_integration_verified'])
        self.assertTrue(r['owned_typed_gpu_loads_verified'])
        self.assertTrue(r['owned_motion_conversion_verified'])
        self.assertEqual(r['owned_compared_float_values'],17*13*7*3)
        for key in ('sdk_evaluation_verified','engine_descriptor_contents_verified','dlaa_ready'):self.assertFalse(r[key])
    def test_owned_budget_damage(self):
        for key in ('owned_copy_limit','owned_descriptor_writes','owned_set_byte_limit','owned_execute_limit',
                    'owned_signal_limit','owned_dispatch_limit','owned_readback_limit'):
            rows=self.owned_rows();rows[0][key]+=1
            with self.subTest(key=key),self.assertRaisesRegex(ValueError,'owned GPU budgets'):review.parse(json.dumps(rows[0]).encode())
    def test_owned_unqualified(self):
        for key in ('enabled','prepared','producer_qualified','attempted','saved','native_state_restored'):
            rows=self.owned_rows();rows[4]['owned_gpu_inputs'][key]=False
            with self.subTest(key=key),self.assertRaises(ValueError):review.review(rows,self.shader)
        for key in ('native_bindings_changed','device_debug_settings_changed','failure_present'):
            rows=self.owned_rows();rows[4]['owned_gpu_inputs'][key]=True
            with self.subTest(key=key),self.assertRaises(ValueError):review.review(rows,self.shader)
    def test_owned_copy_damage(self):
        for group in ('color_readback','depth_readback','motion_readback'):
            for key,value in [('resource',1),('copied',False),('state_before',0),('state_after',64),('format',28),('flags',1),('subresource',1)]:
                rows=self.owned_rows();rows[4][group]['copies'][0]['owned_copy'][key]=value
                with self.subTest(group=group,key=key),self.assertRaises(ValueError):review.review(rows,self.shader)
    def test_owned_private_objects(self):
        for key,value in [('completed',False),('allocator_retired',False),('saved',False),('fence_value',2**64-1),
                          ('adapter_luid_low',0),('allocation_bytes',314572801),('srv_mapping',0),('output_formats',[2,41,42]),
                          ('input_formats',[26,41,41]),('command_recipe',{})]:
            rows=self.owned_rows();rows[4]['owned_gpu_inputs']['sets'][0][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):review.review(rows,self.shader)
        for kind in ('inputs','outputs','readbacks','objects'):
            rows=self.owned_rows();s=rows[4]['owned_gpu_inputs']['sets']
            s[1][kind][0]=s[0][kind][0]
            with self.subTest(kind=kind),self.assertRaises(ValueError):review.review(rows,self.shader)
    def test_owned_rehashed_wrong_pixels(self):
        import shutil,struct,hashlib
        for role in range(3):
            rows=self.owned_rows()
            with tempfile.TemporaryDirectory() as name:
                root=Path(name)
                for p in self.shader.parent.glob('inputs-123-456.*'):
                    if p.is_file():shutil.copy2(p,root/p.name)
                a=rows[4]['owned_gpu_inputs']['sets'][0]['files'][role];p=root/a['file']
                b=bytearray(p.read_bytes());struct.pack_into('<f',b,0,999.0);p.write_bytes(b)
                a['sha256']=hashlib.sha256(b).hexdigest()
                with self.subTest(role=role),self.assertRaisesRegex(ValueError,'full-image comparison'):
                    review.review(rows,root/self.shader.name)
    def test_depth_routes_scope(self):
        if 'depth_routes' not in self.rows[4]:self.skipTest('historical fixture')
        r=review.review(self.rows,self.shader)
        self.assertTrue(r['scene_depth_native_copy_association_verified'])
        self.assertTrue(r['scene_depth_native_copy_states_verified'])
        self.assertEqual(r['scene_depth_native_copy_count'],len(self.rows[4]['depth_routes']['consumers']))
        self.assertTrue(all(c['relative_to_taa']=='same-list-before-taa' for f in r['scene_depth_native_copy_frames'] for c in f['copies']))
        for k in ('scene_depth_final_writer_verified','scene_depth_copy_ready','scene_depth_content_alignment_verified','dlaa_ready'):
            self.assertFalse(r[k])
    def test_depth_routes_damage(self):
        if 'depth_routes' not in self.rows[4]:self.skipTest('historical fixture')
        mutations=[('fault',1),('released',0),('pending',1),('hooks_disabled',False),('started_tick',0),('requests',[]),('consumers',[]),('selections',[])]
        for key,value in mutations:
            rows=copy.deepcopy(self.rows);rows[4]['depth_routes'][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):review.review(rows,self.shader)
        for group,changes in [('requests',[('frame',0),('epoch',999),('packet',1),('valid',False),('returned',False),('opcode',6),('kind',7),('after_count',65537),('rectangle',[0,0,1,1,0,0]),('exit',2**63)]),
                ('consumers',[('receipt',63),('marker_index',0),('valid',False),('copy_returned',False),('copy_caller',1),('native_calls',2),('identity',1),('copy',0),('thread',1)]),
                ('selections',[('epoch',999),('resource',1),('owner',1),('packet',1),('order',1)])]:
            for key,value in changes:
                rows=copy.deepcopy(self.rows);rows[4]['depth_routes'][group][0][key]=value
                with self.subTest(group=group,key=key),self.assertRaises(ValueError):review.review(rows,self.shader)
    def test_depth_routes_state_damage(self):
        if 'depth_routes' not in self.rows[4]:self.skipTest('historical fixture')
        for key,value in [('flags',1),('subresource',0),('after',16),('type',1),('resource',0)]:
            rows=copy.deepcopy(self.rows);rows[4]['depth_routes']['consumers'][0]['barriers'][0][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):review.review(rows,self.shader)
        for key,value in [('returned',False),('stable',False),('first',1),('count',33),('exit',2**63)]:
            rows=copy.deepcopy(self.rows);rows[4]['depth_routes']['consumers'][0]['barrier_calls'][0][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):review.review(rows,self.shader)
        rows=copy.deepcopy(self.rows);r=rows[4]['depth_routes']['consumers'][0];r['source']['identity']=r['destination']['identity']
        with self.assertRaises(ValueError):review.review(rows,self.shader)
    def test_depth_route_generation(self):
        from temporal_depth_routes import generation
        def event(kind,enter,obj=1,lists=None):return dict(kind=kind,enter=enter,exit=enter+1,object=obj,lists=lists or [])
        events=[event(0,1),event(4,3),event(1,5),event(2,7,2,[9,1,8]),event(0,9)]
        r=generation(events,events[1]);self.assertEqual((r['queue'],r['batch_index']),(2,1))
        for altered in (events[:-1],events[1:],events[:3]+[event(2,7,2,[1,1])]+events[4:],
                        events[:4]+[event(2,8,2,[1])]+events[4:],events[:2]+[event(0,4)]+events[2:]):
            with self.assertRaises(ValueError):generation(altered,events[1])
    def test_depth_incomplete_tail_scope(self):
        from temporal_depth_routes import generation_or_incomplete_tail as audit
        def event(kind,enter,obj=1,lists=None):return dict(kind=kind,enter=enter,exit=enter+1,object=obj,lists=lists or [])
        events=[event(0,11),event(4,13),event(1,15)]
        r=audit(events,events[1],101,100,10)
        self.assertFalse(r['qualified']);self.assertEqual(r['reset'],11)
        self.assertFalse(audit(events[:-1],events[1],101,100,10)['qualified'])
        for frame,dispatch in [(100,10),(99,10),(101,14)]:
            with self.assertRaises(ValueError):audit(events,events[1],frame,100,dispatch)
        for altered in (events[1:],events+[event(2,17,2,[1])],events+[event(1,17)],
                        events+[event(0,17)],events[:2]+[event(1,14)]):
            with self.assertRaises(ValueError):audit(altered,events[1],101,100,10)
    def test_depth_routes_duplicate_or_stale(self):
        if 'depth_routes' not in self.rows[4]:self.skipTest('historical fixture')
        for mutate in ('consumer','marker','packet'):
            rows=copy.deepcopy(self.rows);t=rows[4]['depth_routes']
            if mutate=='consumer':t['consumers'][1]['receipt']=t['consumers'][0]['receipt']
            elif mutate=='marker':t['consumers'][1]['marker_index']=t['consumers'][0]['marker_index']
            else:t['requests'][0]['packet']=t['requests'][1]['packet'];t['consumers'][0]['enter']=t['requests'][1]['exit']+1
            with self.subTest(mutate=mutate),self.assertRaises(ValueError):review.review(rows,self.shader)
    def test_depth_routes_budgets(self):
        if self.rows[0]['version'] not in ('1.8.0','1.9.0','1.10.0'):self.skipTest('historical fixture')
        for key,value in [('hooks',7),('depth_route_limit',65),('depth_route_barrier_limit',33),('depth_route_call_limit',9)]:
            rows=copy.deepcopy(self.rows);rows[0][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):review.parse(json.dumps(rows[0]).encode())
    def test_depth_pixels_values(self):
        if self.rows[0]['version'] not in ('1.9.0','1.10.0'):self.skipTest('historical fixture')
        import struct
        result=review.review(self.rows,self.shader)
        self.assertTrue(result['depth_color_motion_triplets_verified']);self.assertTrue(result['scene_depth_pixels_saved'])
        self.assertFalse(result['scene_depth_content_alignment_verified']);self.assertFalse(result['dlaa_ready'])
        for i,p in enumerate(self.rows[4]['depth_readback']['copies']):
            values=list(struct.iter_unpack('<f',(self.shader.parent/p['file']).read_bytes()))
            self.assertEqual(len(values),17*13);self.assertTrue(all(v[0]==.25+.125*i for v in values))
    def test_depth_pixels_damage(self):
        if self.rows[0]['version'] not in ('1.9.0','1.10.0'):self.skipTest('historical fixture')
        for key,value in [('phase',3),('restored',False),('retained',False),('mapped',False),('map_result',1),('receipt',1),('frame',1),('packet',1),
                ('command',1),('allocator',1),('reset',0),('marker',1),('begin',1),('end',2**63),('resource',1),('identity',1),
                ('width',0),('format',39),('source_flags',2),('state_after',64),('copy_state',1024),('subresource',1),
                ('footprint',[0,41,17,13,1,68]),('row_bytes',67),('rows',12),('allocation_bytes',0),('pixel_bytes',0),('file','../depth.f32'),('sha256','0'*64)]:
            rows=copy.deepcopy(self.rows);rows[4]['depth_readback']['copies'][0][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):review.review(rows,self.shader)
    def test_depth_pixels_witnesses(self):
        if self.rows[0]['version'] not in ('1.9.0','1.10.0'):self.skipTest('historical fixture')
        for key,value in [('depth_ok',False),('depth_marker',0),('depth_begin',0),('depth_end',2**63),('depth_resource',0),('depth_identity',0)]:
            rows=copy.deepcopy(self.rows);rows[4]['submission']['samples'][0][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):review.review(rows,self.shader)
        for key,value in [('pixel_receipt',1),('pixel_begin',0),('pixel_end',2**63)]:
            rows=copy.deepcopy(self.rows);n=next(v for v in rows[4]['depth_routes']['consumers'] if v['pixel_receipt']==0);n[key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):review.review(rows,self.shader)
        rows=copy.deepcopy(self.rows);rows[4]['depth_readback']['copies'][0]['readback_buffer']=rows[4]['motion_readback']['copies'][0]['readback_buffer']
        with self.assertRaisesRegex(ValueError,'depth paired allocation identities'):review.review(rows,self.shader)
    def test_depth_pixels_artifact(self):
        if self.rows[0]['version'] not in ('1.9.0','1.10.0'):self.skipTest('historical fixture')
        import shutil
        with tempfile.TemporaryDirectory() as d:
            directory=Path(d);shader=directory/self.shader.name;shutil.copyfile(self.shader,shader)
            for kind in ('motion_readback','color_readback','depth_readback'):
                for p in self.rows[4][kind]['copies']:shutil.copyfile(self.shader.parent/p['file'],directory/p['file'])
            path=directory/self.rows[4]['depth_readback']['copies'][0]['file'];raw=path.read_bytes();path.write_bytes(raw[:-4])
            with self.assertRaisesRegex(ValueError,'depth artifact size'):review.review(self.rows,shader)
            path.write_bytes(bytes([raw[0]^1])+raw[1:])
            with self.assertRaisesRegex(ValueError,'depth artifact hash'):review.review(self.rows,shader)
    def test_depth_pixels_budgets(self):
        if self.rows[0]['version'] not in ('1.9.0','1.10.0'):self.skipTest('historical fixture')
        for key,value in [('gpu_commands',18),('scene_depth_copies',0),('depth_copy_limit',4),('depth_byte_limit',33554432),('depth_selected_scope',False),('total_readback_limit',100663296)]:
            rows=copy.deepcopy(self.rows);rows[0][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):review.parse(json.dumps(rows[0]).encode())
    def test_scene_depth_damage(self):
        if self.rows[0]['version'] not in ('1.7.0','1.8.0','1.9.0','1.10.0'):return
        for key,value in [('stable',False),('resource',0),('identity',0),('container',0),('backing',1),('dimension',1),('width',0)]:
            with self.subTest(key=key):
                rows=copy.deepcopy(self.rows);rows[4]['records'][0]['scene_depth'][key]=value
                with self.assertRaises(ValueError):review.review(rows,self.shader)
        for key,value in [('equal',False),('resource',0),('state',dict(known=True,conflict=False,value=0))]:
            rows=copy.deepcopy(self.rows);rows[4]['native_compute']['records'][0]['scene_depth'][key]=value
            with self.assertRaises(ValueError):review.review(rows,self.shader)
    def test_scene_depth_scope(self):
        if self.rows[0]['version'] not in ('1.7.0','1.8.0','1.9.0','1.10.0'):return
        result=review.review(self.rows,self.shader)
        self.assertTrue(result['scene_depth_request_association_verified'])
        self.assertEqual(result['scene_depth_matches_prior_shape'],self.rows[0]['version']=='1.7.0')
        self.assertEqual(result['scene_depth_pixels_saved'],self.rows[0]['version'] in ('1.9.0','1.10.0'))
        for flag in ('scene_depth_content_alignment_verified','scene_depth_copy_ready','typed_views_verified','dlaa_ready'):
            self.assertFalse(result[flag])
        # A captured candidate with the wrong shape is diagnostic evidence, not
        # a validated depth input. Its GPU state also remains independent.
        rows=copy.deepcopy(self.rows);rows[4]['records'][0]['scene_depth']['format']=26
        if self.rows[0]['version']=='1.7.0':self.assertFalse(review.review(rows,self.shader)['scene_depth_matches_prior_shape'])
        else:
            with self.assertRaises(ValueError):review.review(rows,self.shader)
    def test_positive(self):
        rows=review.parse(('\n'.join(json.dumps(r) for r in self.rows)+'\n').encode())
        result=review.review(rows,self.shader)
        self.assertTrue(result['accepted']);self.assertFalse(result['dlaa_ready']);self.assertFalse(result['motion_contents_verified'])
    def test_rejects_damage(self):
        changes=[(4,'fault',1),(4,'pending',1),(4,'pins_released',20),(4,'hooks_disabled',False),(4,'gpu_consumption_verified',True),
          (4,'width',0),(4,'device',0),(5,'accepted',False),(5,'settled',False),(3,'installed',False),(1,'ready',False)]
        for row,key,value in changes:
            with self.subTest(key=key):
                rows=copy.deepcopy(self.rows);rows[row][key]=value
                with self.assertRaises(ValueError):review.review(rows)
    def test_request_damage(self):
        changes=[('valid',False),('returned',False),('snapshots_equal',False),('caller',1),('context',1),('frame',0),('bank',2),
          ('async',2),('packet_state',1),('opcode',10),('mode',1),('indirect',1),('packet_groups',[159,90,1]),('count_after',9999)]
        for key,value in changes:
            with self.subTest(key=key):
                rows=copy.deepcopy(self.rows);rows[4]['records'][1][key]=value
                with self.assertRaises(ValueError):review.review(rows)
    def test_resource_damage(self):
        for key,value in [('name','MotionVectors'),('backing',1),('dimension',1),('resource',0),('width',0),('identity',0)]:
            with self.subTest(key=key):
                rows=copy.deepcopy(self.rows);rows[4]['records'][0]['inputs'][0][key]=value
                with self.assertRaises(ValueError):review.review(rows)
    def test_header_damage(self):
        for key,value in [('schema',2),('host_sha256','b'*64),('jitter_writes',9),('gpu_commands',1),('capacity',4),('version','2.0.0')]:
            with self.subTest(key=key):
                rows=copy.deepcopy(self.rows);rows[0][key]=value
                with self.assertRaises(ValueError):review.parse(json.dumps(rows[0]).encode())
    def test_arming(self):
        rows=self.rows[:3];validate(rows,123,456,'a'*64)
        for args in [(rows,124,456,'a'*64),(rows,123,457,'a'*64),(rows,123,456,'b'*64),(self.rows,123,456,'a'*64)]:
            with self.assertRaises(ValueError):validate(*args)
    def test_sequence(self):
        for rows in (self.rows[:-1],self.rows+[self.rows[-1]],self.rows[:4]+self.rows[5:]):
            with self.assertRaises(ValueError):review.review(rows)
    def test_shader_damage(self):
        for key,value in [('stable',False),('bytes',1),('technique',0),('bytecode',0),('owners',[]),('bindings',[]),('binding_start',65536)]:
            with self.subTest(key=key):
                rows=copy.deepcopy(self.rows);rows[4]['records'][0]['shader'][key]=value
                with self.assertRaises(ValueError):review.review(rows,self.shader)
        for key,value in [('id',0xffffffff),('owner',1),('mode',256)]:
            rows=copy.deepcopy(self.rows);rows[4]['records'][0]['shader']['owners'][0][key]=value
            with self.assertRaises(ValueError):review.review(rows,self.shader)
        for key,value in [('word',4),('word',0xff2),('view',0),('cpu_descriptor',0)]:
            rows=copy.deepcopy(self.rows);rows[4]['records'][0]['shader']['bindings'][0][key]=value
            with self.assertRaises(ValueError):review.review(rows,self.shader)
    def test_shader_artifact(self):
        with self.assertRaises(ValueError):review.review(self.rows)
        with tempfile.TemporaryDirectory() as d:
            bad=Path(d)/'changed.dxbc';data=bytearray(self.shader.read_bytes());data[-1]^=1;bad.write_bytes(data)
            with self.assertRaises(ValueError):review.review(self.rows,bad)
        for key,value in [('shader_sha256','a'*64),('shader_saved',False),('shader_bytes',1),('typed_views_verified',True)]:
            rows=copy.deepcopy(self.rows);rows[4][key]=value
            with self.assertRaises(ValueError):review.review(rows,self.shader)
    def test_compute_container(self):
        data=self.shader.read_bytes();compute_dxbc(data)
        for n in (0,4,31,51,len(data)-1):
            with self.assertRaises(ValueError):compute_dxbc(data[:n])
        import struct
        for at,value in ((24,0),(28,65),(32,0)):
            bad=bytearray(data);struct.pack_into('<I',bad,at,value)
            with self.assertRaises(ValueError):compute_dxbc(bad)
    def test_historical_review(self):
        rows=copy.deepcopy(self.rows);rows[0].update(version='1.0.0',hooks=1,gpu_commands=0,jitter_writes=0)
        rows[4].update(pins_acquired=21,pins_released=21)
        for r in rows[4]['records']:r.pop('shader')
        self.assertTrue(review.review(review.parse(('\n'.join(json.dumps(r) for r in rows)).encode()))['accepted'])
    def test_native_damage(self):
        if 'native_compute' not in self.rows[4]:self.skipTest('historical fixture')
        for key,value in [('fault',1),('pending',1),('completed',2),('pins',4),('released',2),('hooks_disabled',False),('gpu_submission_verified',True),('started_tick',0)]:
            with self.subTest(key=key):
                rows=copy.deepcopy(self.rows);rows[4]['native_compute'][key]=value
                with self.assertRaises(ValueError):review.review(rows,self.shader)
        for key,value in [('frame',0),('packet',1),('valid',False),('returned',False),('recipe_equal',False),('packet_equal',False),
            ('dispatch_returned',False),('command',0),('identity',0),('command_type',2),('dispatch_calls',2),('dispatch_caller',1),('dispatch_enter',0),('exit_tick',2**63)]:
            with self.subTest(key=key):
                rows=copy.deepcopy(self.rows);rows[4]['native_compute']['records'][0][key]=value
                with self.assertRaises(ValueError):review.review(rows,self.shader)
    def test_barrier_damage(self):
        if 'native_compute' not in self.rows[4]:self.skipTest('historical fixture')
        for key,value in [('first',1),('count',129),('returned',False),('stable',False),('enter',0),('caller',0)]:
            rows=copy.deepcopy(self.rows);rows[4]['native_compute']['records'][0]['barrier_calls'][0][key]=value
            with self.assertRaises(ValueError):review.review(rows,self.shader)
        rows=copy.deepcopy(self.rows);rows[4]['native_compute']['records'][0]['states'][5]['value']=2048
        with self.assertRaises(ValueError):review.review(rows,self.shader)
    def test_state_limits(self):
        calls=[dict(first=0,count=1,exit=3)];barrier=dict(type=0,flags=0,subresource=0xffffffff,before=4,after=64,resource=10,other=0)
        self.assertEqual(derive_states([10],calls,[barrier],4),[dict(known=True,conflict=False,value=64)])
        self.assertEqual(derive_states([10],calls,[barrier],2),[dict(known=False,conflict=False,value=0)])
        for altered in [dict(barrier,flags=1),dict(barrier,subresource=0),dict(barrier,type=1,resource=0)]:
            self.assertEqual(derive_states([10],calls,[altered],4),[dict(known=False,conflict=True,value=0)])
        chain=[dict(first=0,count=2,exit=3)]
        self.assertEqual(derive_states([10],chain,[barrier,dict(barrier,before=1024)],4),[dict(known=False,conflict=True,value=64)])
    def test_submission_closure(self):
        if 'submission' not in self.rows[4]:self.skipTest('historical fixture')
        for key,value in [('fault',1),('pending',1),('hooks_disabled',False),('retained',True),('released',0),('started_tick',0),('queues',[]),('events',[])]:
            with self.subTest(key=key):
                rows=copy.deepcopy(self.rows);rows[4]['submission'][key]=value
                with self.assertRaises(ValueError):review.review(rows,self.shader)
    def test_submission_sample(self):
        if 'submission' not in self.rows[4]:self.skipTest('historical fixture')
        for key,value in [('receipt',1),('frame',0),('packet',1),('command',1),('allocator',1),('queue',1),('reset',0),
            ('dispatch',0),('dispatch_exit',1),('close',0),('execute',0),('execute_exit',1),('retire',0),('retire_exit',1),
            ('signal_attempts',2),('signal_result',0x80004005),('signaled',False),('completed',False),('completed_value',2),('completed_order',1),('completed_tick',2**63)]:
            with self.subTest(key=key):
                rows=copy.deepcopy(self.rows);rows[4]['submission']['samples'][0][key]=value
                with self.assertRaises(ValueError):review.review(rows,self.shader)
    def test_submission_events(self):
        if 'submission' not in self.rows[4]:self.skipTest('historical fixture')
        for key,value in [('kind',4),('returned',False),('valid',False),('result',0x80004005),('enter',0),('exit',0),('object',1),('allocator',1)]:
            with self.subTest(key=key):
                rows=copy.deepcopy(self.rows);rows[4]['submission']['events'][0][key]=value
                with self.assertRaises(ValueError):review.review(rows,self.shader)
    def test_submission_resubmit_replay(self):
        if 'submission' not in self.rows[4]:self.skipTest('historical fixture')
        # Replace a private fence-order gap with another submission. The
        # sample still claims success; replay must discover the duplicate.
        rows=copy.deepcopy(self.rows);trace=rows[4]['submission'];s=trace['samples'][0]
        execute=next(e for e in trace['events'] if e['enter']==s['execute'])
        duplicate=dict(execute,enter=s['signal'],exit=s['signal_exit'])
        trace['events'].append(duplicate);trace['events'].sort(key=lambda e:e['enter'])
        with self.assertRaisesRegex(ValueError,'resubmission before retirement'):review.review(rows,self.shader)
    def test_submission_generation_replay(self):
        if 'submission' not in self.rows[4]:self.skipTest('historical fixture')
        rows=copy.deepcopy(self.rows);trace=rows[4]['submission'];s=trace['samples'][0]
        e=next(e for e in trace['events'] if e['enter']==s['retire'])
        s['retire']=trace['samples'][1]['retire'];s['retire_exit']=trace['samples'][1]['retire_exit']
        self.assertNotEqual(e['enter'],s['retire'])
        with self.assertRaisesRegex(ValueError,'intervening reset'):review.review(rows,self.shader)
    def test_submission_fence_reuse(self):
        if 'submission' not in self.rows[4]:self.skipTest('historical fixture')
        rows=copy.deepcopy(self.rows);s=rows[4]['submission']['samples'];s[1]['fence']=s[0]['fence']
        with self.assertRaisesRegex(ValueError,'private fence identities'):review.review(rows,self.shader)
    def test_submission_batch_damage(self):
        if 'submission' not in self.rows[4]:self.skipTest('historical fixture')
        for batch in ([],[1],[1]*129):
            rows=copy.deepcopy(self.rows);e=next(e for e in rows[4]['submission']['events'] if e['kind']==2);e['lists']=batch
            with self.assertRaises(ValueError):review.review(rows,self.shader)
    def test_submission_success_flags(self):
        if 'submission' not in self.rows[4]:self.skipTest('historical fixture')
        r=review.review(self.rows,self.shader)
        for key in ('gpu_submission_verified','gpu_completion_verified','recording_retirement_verified'):self.assertTrue(r[key])
        for key in ('readback_eligible','motion_contents_verified','dlaa_ready'):self.assertFalse(r[key])
        self.assertEqual(r['motion_pixels_saved'],self.rows[0]['version'] in ('1.4.0','1.5.0','1.6.0','1.6.1','1.7.0','1.8.0','1.9.0','1.10.0'))
    def test_motion_evidence(self):
        if self.rows[0]['version'] not in ('1.4.0','1.5.0','1.6.0','1.6.1','1.7.0','1.8.0','1.9.0','1.10.0'):self.skipTest('historical fixture')
        changes=[('phase',3),('attempted',False),('transitioned',False),('copied',False),('restored',False),('retained',False),('mapped',False),
            ('map_result',0x80004005),('receipt',1),('frame',0),('packet',1),('command',1),('allocator',1),('resource',1),('identity',1),
            ('reset',0),('marker',0),('begin',0),('end',2**63),('width',8193),('height',0),('format',34),('source_flags',0),('subresource',1),
            ('state_before',64),('state_after',64),('copy_state',1024),('footprint',[0,42,17,13,1,68]),('allocation_bytes',884),
            ('row_bytes',67),('rows',12),('pixel_bytes',3328),('file','../outside.u32'),('sha256','b'*64)]
        for key,value in changes:
            with self.subTest(key=key):
                rows=copy.deepcopy(self.rows);rows[4]['motion_readback']['copies'][0][key]=value
                with self.assertRaises(ValueError):review.review(rows,self.shader)
    def test_motion_order(self):
        if self.rows[0]['version'] not in ('1.4.0','1.5.0','1.6.0','1.6.1','1.7.0','1.8.0','1.9.0','1.10.0'):self.skipTest('historical fixture')
        for key,value in [('copy_ok',False),('copy_begin',0),('copy_end',0),('dispatch_forward',0),('copy_identity',1),('copy_resource',1)]:
            rows=copy.deepcopy(self.rows);rows[4]['submission']['samples'][0][key]=value
            with self.assertRaises(ValueError):review.review(rows,self.shader)
        rows=copy.deepcopy(self.rows);p=rows[4]['motion_readback']['copies'][0];s=rows[4]['submission']['samples'][0]
        p['end']=s['copy_end']=s['dispatch_exit']
        with self.assertRaisesRegex(ValueError,'motion copy order'):review.review(rows,self.shader)
    def test_motion_artifact(self):
        if self.rows[0]['version'] not in ('1.4.0','1.5.0','1.6.0','1.6.1','1.7.0','1.8.0','1.9.0','1.10.0'):self.skipTest('historical fixture')
        import shutil
        with tempfile.TemporaryDirectory() as d:
            shader=Path(d)/self.shader.name;shutil.copyfile(self.shader,shader)
            for p in self.rows[4]['motion_readback']['copies']:shutil.copyfile(self.shader.parent/p['file'],Path(d)/p['file'])
            path=Path(d)/self.rows[4]['motion_readback']['copies'][1]['file'];original=path.read_bytes()
            path.write_bytes(original[:-4])
            with self.assertRaisesRegex(ValueError,'motion artifact size'):review.review(self.rows,shader)
            changed=bytearray(original);changed[0]^=1;path.write_bytes(changed)
            with self.assertRaisesRegex(ValueError,'motion artifact hash'):review.review(self.rows,shader)
    def test_motion_decode(self):
        self.assertEqual(decode_word(0x3c00b800),(1.0,-0.5))
        self.assertEqual(decode_word(0x3c00b801),(1.0,-0.5))
        self.assertEqual(history_uv_delta(0x3c00b800),(-0.5,-0.25))
        self.assertEqual(decode_word(0),(0.0,0.0))
        self.assertEqual(decode_word(0x00010002),(2**-24,2**-23))
        import math
        self.assertTrue(math.isinf(decode_word(0x7c000000)[0]));self.assertTrue(math.isnan(decode_word(0x7e000000)[0]))
        for word in (-1,2**32,True):
            with self.assertRaises(ValueError):decode_word(word)
    def test_color_evidence(self):
        if self.rows[0]['version'] not in ('1.5.0','1.6.0','1.6.1','1.7.0','1.8.0','1.9.0','1.10.0'):self.skipTest('historical fixture')
        for key,value in [('phase',3),('restored',False),('mapped',False),('map_result',0x80004005),('frame',0),('packet',1),('resource',1),('identity',1),
            ('format',42),('source_flags',1),('state_before',192),('state_after',192),('readback_buffer',0),('begin',0),('end',2**63),
            ('footprint',[0,42,17,13,1,256]),('pixel_bytes',883),('file','../bad.u32'),('sha256','b'*64)]:
            with self.subTest(key=key):
                rows=copy.deepcopy(self.rows);rows[4]['color_readback']['copies'][0][key]=value
                with self.assertRaises(ValueError):review.review(rows,self.shader)
    def test_color_pair_constraints(self):
        if self.rows[0]['version'] not in ('1.5.0','1.6.0','1.6.1','1.7.0','1.8.0','1.9.0','1.10.0'):self.skipTest('historical fixture')
        rows=copy.deepcopy(self.rows);c=rows[4]
        c['color_readback']['copies'][0]['readback_buffer']=c['motion_readback']['copies'][0]['readback_buffer']
        with self.assertRaisesRegex(ValueError,'paired readback allocations'):review.review(rows,self.shader)
        for key,value in [('color_ok',False),('color_begin',0),('color_end',0),('color_resource',1),('color_identity',1)]:
            rows=copy.deepcopy(self.rows);rows[4]['submission']['samples'][0][key]=value
            with self.assertRaises(ValueError):review.review(rows,self.shader)
        rows=copy.deepcopy(self.rows);rows[4]['submission']['colors_enabled']=False
        with self.assertRaisesRegex(ValueError,'color submission mode'):review.review(rows,self.shader)
        rows=copy.deepcopy(self.rows);p=rows[4]['color_readback']['copies'][0];s=rows[4]['submission']['samples'][0]
        p['begin']=s['color_begin']=s['copy_begin']
        with self.assertRaisesRegex(ValueError,'paired copy order'):review.review(rows,self.shader)
        rows=copy.deepcopy(self.rows)
        for i in (1,2):
            for group in (rows[4]['records'],rows[4]['native_compute']['records'],rows[4]['submission']['samples'],rows[4]['motion_readback']['copies'],rows[4]['color_readback']['copies']):group[i]['frame']+=10
        with self.assertRaisesRegex(ValueError,'paired frame adjacency'):review.review(rows,self.shader)
    def test_color_artifact(self):
        if self.rows[0]['version'] not in ('1.5.0','1.6.0','1.6.1','1.7.0','1.8.0','1.9.0','1.10.0'):self.skipTest('historical fixture')
        import shutil
        with tempfile.TemporaryDirectory() as d:
            shader=Path(d)/self.shader.name;shutil.copyfile(self.shader,shader)
            for kind in ('motion_readback','color_readback'):
                for p in self.rows[4][kind]['copies']:shutil.copyfile(self.shader.parent/p['file'],Path(d)/p['file'])
            path=Path(d)/self.rows[4]['color_readback']['copies'][1]['file'];original=path.read_bytes();path.write_bytes(original[:-4])
            with self.assertRaisesRegex(ValueError,'color artifact size'):review.review(self.rows,shader)
            changed=bytearray(original);changed[0]^=1;path.write_bytes(changed)
            with self.assertRaisesRegex(ValueError,'color artifact hash'):review.review(self.rows,shader)
    def test_color_decode(self):
        import math
        self.assertEqual(decode_color(0),(0.0,0.0,0.0))
        self.assertEqual(decode_color(0x3c0|(0x400<<11)|(0x220<<22)),(1.0,2.0,4.0))
        self.assertEqual(decode_color(1|(1<<11)|(1<<22)),(2**-20,2**-20,2**-19))
        self.assertTrue(math.isinf(decode_color(0x7c0)[0]));self.assertTrue(math.isnan(decode_color(0x7c1)[0]))
        self.assertEqual(decode_color(0x7bf|(0x7bf<<11)|(0x3df<<22)),(65024.0,65024.0,64512.0))
        for word in (-1,2**32,True):
            with self.assertRaises(ValueError):decode_color(word)
    def test_color_gpu_pixels(self):
        if self.rows[0]['version'] not in ('1.5.0','1.6.0','1.6.1','1.7.0','1.8.0','1.9.0','1.10.0'):self.skipTest('historical fixture')
        import struct
        for i,p in enumerate(self.rows[4]['color_readback']['copies']):
            values=list(struct.iter_unpack('<I',(self.shader.parent/p['file']).read_bytes()))
            for y in range(13):
                for x in range(17):self.assertEqual(decode_color(values[y*17+x][0]),((x+i*2)*.125,(y+i*3)*.25,(x+y+i)*.5))
        r=review.review(self.rows,self.shader);self.assertTrue(r['paired_color_motion_readback_verified']);self.assertFalse(r['color_motion_alignment_verified'])
    def test_color_budgets(self):
        if self.rows[0]['version'] not in ('1.5.0','1.6.0','1.6.1','1.7.0','1.8.0','1.9.0','1.10.0'):self.skipTest('historical fixture')
        for key,value in [('color_copy_limit',4),('color_byte_limit',2**30),('total_readback_limit',2**30)]:
            rows=copy.deepcopy(self.rows);rows[0][key]=value
            with self.assertRaisesRegex(ValueError,'color budgets'):review.parse(json.dumps(rows[0]).encode())
    def test_jitter_positive(self):
        if self.rows[0]['version'] not in ('1.6.0','1.6.1'):self.skipTest('historical fixture')
        r=review.review(self.rows,self.shader)
        self.assertTrue(r['scoped_projection_stimulus_verified']);self.assertTrue(r['projection_taa_request_association_verified'])
        self.assertFalse(r['color_jitter_verified']);self.assertFalse(r['motion_jitter_inclusion_verified'])
        self.assertEqual([p['jitter'] for p in r['projection_samples']],[[0.,0.],([.375,-.4375] if self.rows[0]['version']=='1.6.1' else [.25,-.375]),[0.,0.]])
    def test_jitter_closure(self):
        if self.rows[0]['version'] not in ('1.6.0','1.6.1'):self.skipTest('historical fixture')
        for key,value in [('fault',1),('pending',1),('hooks_disabled',False),('completed',2),('matched',2),('injections',2),('restorations',0),('records',[])]:
            rows=copy.deepcopy(self.rows);rows[4]['projection_stimulus'][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):review.review(rows,self.shader)
    def test_jitter_record_damage(self):
        if self.rows[0]['version'] not in ('1.6.0','1.6.1'):self.skipTest('historical fixture')
        changes=[('stage',0),('valid',False),('returned',False),('acknowledged',False),('restored',False),('restored_bits',1),
            ('injected',False),('frame',99),('epoch',99),('bank',2),('async',2),('context',1),('view',1),('caller',1),('camera_type',1),
            ('camera',0),('constants',0),('provider_stable',False),('provider_sizing',1),('wrapper',1),('backend',1),('device',1),
            ('candidate',0),('candidate',0x7fc00000),('candidate',0x3f800000),('jitter_bits',[0,0]),('table20_bits',[0,0]),
            ('viewport_bits',[0,0]),('enter_tick',0),('exit_tick',2**63),('matrix_relation',False),('projection_valid',False),('matrices_valid',False)]
        for key,value in changes:
            rows=copy.deepcopy(self.rows);rows[4]['projection_stimulus']['records'][1][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):review.review(rows,self.shader)
    def test_jitter_matrix_damage(self):
        if self.rows[0]['version'] not in ('1.6.0','1.6.1'):self.skipTest('historical fixture')
        for key in ('projection_bits','unjittered_bits','view_projection_bits','view_bits','unjittered_view_projection_bits'):
            rows=copy.deepcopy(self.rows);rows[4]['projection_stimulus']['records'][1][key][0]=0x7fc00000
            with self.subTest(key=key),self.assertRaises(ValueError):review.review(rows,self.shader)
        rows=copy.deepcopy(self.rows);rows[4]['projection_stimulus']['records'][1]['view_projection_bits'][12]=0
        with self.assertRaises(ValueError):review.review(rows,self.shader)
    def test_jitter_budget_damage(self):
        if self.rows[0]['version'] not in ('1.6.0','1.6.1'):self.skipTest('historical fixture')
        for key,value in [('jitter_writes',0),('hooks',7),('projection_samples',4),('projection_byte_limit',8192),('jitter_restorations',0)]:
            rows=copy.deepcopy(self.rows);rows[0][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):review.parse(json.dumps(rows[0]).encode())
    def test_jitter_controls(self):
        if self.rows[0]['version'] not in ('1.6.0','1.6.1'):self.skipTest('historical fixture')
        for i in (0,2):
            rows=copy.deepcopy(self.rows);r=rows[4]['projection_stimulus']['records'][i];r['injected']=True
            with self.assertRaises(ValueError):review.review(rows,self.shader)
        for key in ('camera','table','buffer_id','words'):
            rows=copy.deepcopy(self.rows);rows[4]['projection_stimulus']['records'][2][key]+=1
            with self.subTest(key=key),self.assertRaises(ValueError):review.review(rows,self.shader)
    def test_jitter_selection_damage(self):
        if self.rows[0]['version']!='1.6.1':self.skipTest('historical fixture')
        changes=[('selection_attempts',0),('selection_attempts',21),('selection_attempts',True),
            ('selection_skipped',99),('selected_frame',99),('selected_candidate',0),('selected_candidate',True)]
        for key,value in changes:
            rows=copy.deepcopy(self.rows);rows[4]['projection_stimulus'][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):review.review(rows,self.shader)
    def test_jitter_selection_budget_damage(self):
        if self.rows[0]['version']!='1.6.1':self.skipTest('historical fixture')
        for key,value in [('jitter_min_abs',.1),('jitter_selection_limit',21)]:
            rows=copy.deepcopy(self.rows);rows[0][key]=value
            with self.subTest(key=key),self.assertRaisesRegex(ValueError,'jitter selection budgets'):review.parse(json.dumps(rows[0]).encode())
    def test_jitter_small_consistent_preview_rejected(self):
        if self.rows[0]['version']!='1.6.1':self.skipTest('historical fixture')
        from temporal_jitter import review_jitter
        import struct
        for x,y in [(.312499,-.4),(.4,-.312499),(float('nan'),.4),(.6,-.4)]:
            rows=copy.deepcopy(self.rows);j=rows[4]['projection_stimulus'];bits=struct.unpack('<Q',struct.pack('<ff',x,y))[0]
            j['selected_candidate']=j['records'][1]['candidate']=bits
            with self.subTest(x=x,y=y),self.assertRaises(ValueError):review_jitter(rows[0],rows[4])
if __name__=='__main__':
    sys.argv_fixture=sys.argv.pop(1)
    unittest.main()
