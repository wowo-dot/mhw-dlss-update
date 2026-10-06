import copy
import hashlib
from pathlib import Path
import tempfile
import unittest
from test_aa_deferred import fixture as deferred_fixture
from test_aa_owned import raw
from arm_aa_boundary import parse
from review_aa_inputs import review


def fixture(images):
    rows = deferred_fixture()
    rows[0].update(version='0.5.0',experiment='independent-aa-input-readback',private_gpu_commands=27,
                   private_queue_signals=8,gpu_state_trace=True,readback_receipts=[0,3,7],read_point='first-paired-transition-after-marker')
    rows[3]['hooks'] = 6
    events,calls,samples,records = [],[],[],[]
    counter = 0

    def event(kind,obj,allocator=0,lists=None):
        nonlocal counter
        e = dict(kind=kind,object=obj,allocator=allocator,enter=counter+1,exit=counter+2,
                 returned=True,valid=True,result=0,lists=lists or [])
        counter += 2;events.append(e)
        return e

    for i,request in enumerate(rows[12]['requests']):
        request['list'] = 904
        for k,fmt in enumerate((26,41,42)): request['inputs'][k]['format'] = fmt
        reset = event(0,904,990)
        before_token = len(events); before = event(5,904)
        marker = event(3,904)
        read_token = len(events); point = event(5,904)
        close = event(1,904); execute = event(2,1001,lists=[904])
        signal,signal_exit,completed_order = counter+1,counter+2,counter+3;counter += 3
        retire = event(0,904,991)
        for token,ev,barriers in (
            (before_token,before,[dict(type=0,flags=0,resource=600+k,subresource=0xffffffff,before=128,after=value) for k,value in enumerate((4,128))]),
            (read_token,point,[dict(type=0,flags=0,resource=600+k,subresource=0xffffffff,before=4,after=128) for k in (0,2)]),
        ):
            calls.append(dict(token=token,command=904,reset=reset['enter'],enter=ev['enter'],returned=True,unchanged=True,barriers=barriers))
        samples.append(dict(receipt=i,frame=request['source_frame'],packet=request['packet'],command=904,allocator=990,
                            reset=reset['enter'],dispatch=marker['enter'],dispatch_exit=marker['exit'],close=close['enter'],
                            execute=execute['enter'],execute_exit=execute['exit'],queue=1001,retire=retire['enter'],retire_exit=retire['exit'],
                            fence=10000+i,signal_attempts=1,signal_result=0,signal=signal,signal_exit=signal_exit,
                            completed_order=completed_order,signaled=True,completed=True,completed_value=1))
        records.append(dict(receipt=i,frame=request['source_frame'],packet=request['packet'],command=904,allocator=990,
                            reset=reset['enter'],marker=marker['enter'],captured=True,read_point=point['enter'],read_token=read_token,
                            read_ready=True,copy_ok=True,
                            states=[dict(known=True,state=value,issue=0,transitions=1,last=before['enter']*1025+k+1) for k,value in enumerate((4,128))]
                                + [dict(known=False,state=0,issue=1,transitions=0,last=0)],
                            read_states=[dict(known=True,state=128,issue=0,transitions=2,last=point['enter']*1025+1),
                                         dict(known=True,state=128,issue=0,transitions=1,last=before['enter']*1025+2),
                                         dict(known=True,state=128,issue=0,transitions=1,last=point['enter']*1025+2)]))
    g = dict(event='gpu_trace',fault=0,pending=0,hooks_disabled=True,drained=True,qualified=True,completed=True,
             journal_released=True,watch_released=True,watched=[600,601,602],records=records,barrier_calls=calls,
             submission=dict(fault=0,pending=0,hooks_disabled=True,admission_closed=True,retained=False,pins=30,released=30,
                             copies_enabled=False,colors_enabled=False,depths_enabled=False,events=events,samples=samples))
    rows.insert(-1,g)
    rows.insert(-1,dict(event='input_readbacks',prepared=True,gpu_safe=True,released=True,images=copy.deepcopy(images)))
    return rows


class Cases(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix='mhw-aa-inputs-')
        cls.root = Path(cls.temp.name)
        # Nine immutable test images; corruption cases alter declarations only.
        cls.images = []
        for receipt in (0,3,7):
            for role in range(3):
                data = bytes([role+1]) * 14745600
                name = f'input-{receipt}-{role}.bin'
                (cls.root/name).write_bytes(data)
                cls.images.append(dict(receipt=receipt,role=role,attempted=True,transitioned=True,copied=True,restored=True,saved=True,
                    map_result=0,format=(26,41,42)[role],row_pitch=10240,bytes=len(data),state_before=128,copy_state=2048,state_after=128,
                    file=name,sha256=hashlib.sha256(data).hexdigest()))

    @classmethod
    def tearDownClass(cls): cls.temp.cleanup()

    def test_complete(self):
        result = review(raw(fixture(self.images)),'a'*64,self.root)
        self.assertEqual(result['input_states_at_read_point'],[[128]*3]*8)
        self.assertEqual(result['readback_images_verified'],9)
        self.assertFalse(result['pixel_freshness_verified'])
        self.assertFalse(result['live_independent_dlaa_verified'])

    def test_distinct_version(self):
        rows = fixture(self.images)[:2]
        parse(raw(rows),pixels=True)
        with self.assertRaises(ValueError): parse(raw(rows),gpu=True)
        rows[0]['private_gpu_commands'] = 0
        with self.assertRaises(ValueError): parse(raw(rows),pixels=True)

    def test_point_requires_native_pair_and_exact_generation(self):
        for change in ('token','earlier','generation','pair','split','unknown','copy','marker_state','read_state'):
            rows = fixture(self.images);g=rows[13];r=g['records'][0];b=g['barrier_calls'][1]
            if change == 'token':r['read_token'] = 1
            elif change == 'earlier':r['read_point'] = r['marker']
            elif change == 'generation':b['reset'] += 1
            elif change == 'pair':b['barriers'][1]['resource'] = 601
            elif change == 'split':b['barriers'][1]['flags'] = 1
            elif change == 'unknown':r['read_ready'] = False
            elif change == 'copy':r['copy_ok'] = False
            elif change == 'marker_state':r['states'][2]['known'] = True
            else:r['read_states'][2]['state'] = 4
            with self.assertRaises(ValueError,msg=change):review(raw(rows),'a'*64,self.root)

    def test_readback_integrity_and_lifetime(self):
        for key,value in (('file','../escape.bin'),('file','input-0-0.bin'),('sha256','0'*64),('bytes',1),
                          ('state_after',4),('restored',False),('map_result',0x80004005),('receipt',4),('format',42)):
            rows = fixture(self.images);rows[14]['images'][1][key] = value
            with self.assertRaises(ValueError,msg=key):review(raw(rows),'a'*64,self.root)
        for key in ('gpu_safe','released'):
            rows = fixture(self.images);rows[14][key] = False
            with self.assertRaises(ValueError):review(raw(rows),'a'*64,self.root)


if __name__ == '__main__':unittest.main()
