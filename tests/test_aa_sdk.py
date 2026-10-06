import copy
import hashlib
import json
from pathlib import Path
import tempfile
import unittest
import numpy as np
from arm_aa_boundary import parse
from test_aa_projection import shutdown_fixture
from review_aa_sdk import review_sdk_contract, review_sdk_pixels
from projection_contract import floats
from temporal_color import decode_color
from temporal_inline import pack_reference


def fixture():
    rows=shutdown_fixture([]);rows[0].update(version='0.7.0',experiment='independent-aa-sdk',sdk_used=True,
        private_gpu_commands=-1,sdk_evaluations=8,sdk_output='private-only')
    g=rows[13];receipts=[]
    for i,(s,b) in enumerate(zip(g['submission']['samples'],g['records'])):
        event=g['submission']['events'][b['read_token']];x,y=floats(b['jitter_bits'],2)
        receipts.append(dict(receipt=i,frame=s['frame'],epoch=b['projection_epoch'],generation=s['reset'],jitter_bits=b['jitter_bits'],
            sdk_jitter=[x/2,-y/2],begin_tick=event['enter_tick'],end_tick=event['exit_tick'],attempted=True,previous_completed=True,
            inputs_restored=True,bindings_restored=True,success=True))
    last=receipts[-1];sample=g['submission']['samples'][-1]
    p=dict(schema=1,failure='',creation_completed=True,issued=True,restored=True,copied=True,released=True,continuous=True,
        shutdown_deferred=True,create_result=1,evaluate_result=1,release_result=1,destroy_result=1,evaluations=8,resets=1,captures=0,
        reset=False,native_output=999,raw=[901,902,903],create_flags=75,mode=5,preset=11,input_formats=[26,41,42],converted_formats=[2,41,16],
        mv_scale=[1,1],pre_exposure=1,mv_jittered=False,width=2560,height=1440,device=500,frame_ms=16.666666,allocation_bytes=1024,
        frame=last['frame'],epoch=last['epoch'],generation=last['generation'],command=sample['command'],completion_fence=sample['fence'],
        completion_value=1,jitter=last['sdk_jitter'])
    rows.insert(-1,dict(event='aa_sdk',schema=1,count=8,finished=True,saved=True,failure='',native_output_written=False,
        private_destination=999,receipts=receipts,sdk=p))
    return rows


class Cases(unittest.TestCase):
    def test_sdk_scope_and_history(self):
        rows=fixture();parse(json.dumps(rows[0]).encode(),sdk=True)
        result=review_sdk_contract(rows)
        self.assertEqual(result['independent_sdk_evaluations'],8)
        self.assertFalse(result['independent_dlaa_presentation_verified'])
        with self.assertRaises(ValueError):parse(json.dumps(rows[0]).encode(),projection=True)

    def test_rejects_invariant_corruption(self):
        mutations=[lambda r:r[16].update(native_output_written=True),lambda r:r[16].update(private_destination=600),
            lambda r:r[16]['sdk'].update(resets=8),lambda r:r[16]['sdk'].update(evaluate_result=0),
            lambda r:r[16]['sdk'].update(completion_fence=123),lambda r:r[16]['sdk'].update(raw=[600,902,903]),
            lambda r:r[16]['receipts'][3].update(previous_completed=False),lambda r:r[16]['receipts'][3].update(inputs_restored=False),
            lambda r:r[16]['receipts'][3].update(bindings_restored=False),lambda r:r[16]['receipts'][3].update(frame=9999),
            lambda r:r[16]['receipts'][3].update(sdk_jitter=[.2,.3]),lambda r:r[16]['receipts'][3].update(begin_tick=0),
            lambda r:r[13]['submission']['samples'][2].update(completed_tick=99999)]
        for mutation in mutations:
            with self.subTest(mutation=mutation):
                rows=fixture();mutation(rows)
                with self.assertRaises(ValueError):review_sdk_contract(rows)

    def test_pixels_exact_and_corruption(self):
        with tempfile.TemporaryDirectory() as path:
            folder=Path(path);native={'images':[]};a={'images':[]}
            raw=[np.full(16,0x781e03c0,dtype='<u4'),np.full(16,.1,dtype='<f4'),np.zeros(16,dtype='<u4')]
            color=np.tile([*decode_color(int(raw[0][0])),1],(16,1)).astype('<f4')
            rgb=np.full((16,4),.2,dtype='<f4');packed=pack_reference(rgb[:,:3])
            converted=[color,raw[1],np.zeros((16,2),dtype='<f4'),rgb,packed]
            for receipt in (0,3,7):
                for role,data in enumerate(raw):
                    name=f'native-{receipt}-{role}';(folder/name).write_bytes(data.tobytes());native['images'].append(dict(receipt=receipt,role=role,file=name))
                for role,data in enumerate(converted):
                    name=f'sdk-{receipt}-{role}';b=data.tobytes();(folder/name).write_bytes(b)
                    a['images'].append(dict(receipt=receipt,role=role,file=name,sha256=hashlib.sha256(b).hexdigest(),bytes=len(b),
                        format=(2,41,16,2,26)[role],copied=True,saved=True))
            self.assertEqual(review_sdk_pixels(a,native,folder,4,4)['sdk_images_verified'],15)
            changed=copy.deepcopy(a);changed['images'][0]['sha256']='0'*64
            with self.assertRaises(ValueError):review_sdk_pixels(changed,native,folder,4,4)
            changed=copy.deepcopy(a);entry=changed['images'][0];bad=np.zeros_like(color).tobytes();(folder/entry['file']).write_bytes(bad);entry['sha256']=hashlib.sha256(bad).hexdigest()
            with self.assertRaises(ValueError):review_sdk_pixels(changed,native,folder,4,4)


if __name__=='__main__':unittest.main()
