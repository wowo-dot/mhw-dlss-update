import copy
import importlib.util
from pathlib import Path
import unittest

spec=importlib.util.spec_from_file_location('review',Path(__file__).resolve().parents[1]/'tools/review_reflex_presentation.py')
review=importlib.util.module_from_spec(spec); spec.loader.exec_module(review)

def valid():
    # Construct a lifecycle independent of implementation logging to test
    # acceptance, cardinality, chronology, frame accounting and fail-closed flags.
    rows=[dict(event='start',schema=1,version='0.2.1',experiment='reflex-persistent-presentation',
        host_sha256=review.HOST_SHA,graphics_api='D3D12',sdk='2.12.0',pid=1,creation=2,dll_sha256='a'*64,
        mode=0,sleep=False,markers=False,upscaling=False)]
    rows += [dict(event='api_module',kind=i,loaded_from_system32=True,target=100+i) for i in range(2)]
    rows += [dict(event='prepared',targets=[100,101],queued_api_hooks=2,sdk_initialized=False,original_heads_rva=4096),dict(event='ready')]
    rows += [dict(event='sdk_initialized',after_native_device_creation=True,before_native_queues_and_swapchain=True,
        caller_rva=0x23a4e2e,device=200,thread=3,sleep=False,markers=False),
        dict(event='device_checked',caller_rva=0x23a4e2e,device=200,thread=3,low_latency_available=True,
        mode=0,rendering_modified=False,sleep_calls=0,markers=0),
        dict(event='factory_context',source='native-adapter-parent',output_owner_matches=True,
            factory=201,backend=4096,global_backend_published=False),
        dict(event='presentation_installed',device_native=True,returned_interfaces_native=True,factory=201,creation_routes=4,mode=0)]
    rows += [dict(event='hook_restoration',kind=i,target=100+i,restored=True,original_head_sha256='b'*64) for i in range(2)]
    rows += [dict(event='swapchain_snapshot',native_interface=True,swapchain=202,width=320,height=240,format=87,buffers=2,windowed=True),
        dict(event='persistent_ready',mode=0,sleep_calls=0,markers=0,ceiling_seconds=600),
        dict(event='heartbeat',faults=0,installed_hooks=12,presents=10,native_presents=10,native_swapchain=202,proxy_swapchain=203)]
    rows += [dict(event='presentation_hook_restoration',kind=i,target=1000+i,restored=True,original_head_sha256='c'*64) for i in range(12)]
    rows += [dict(event='presentation_cleanup',restored=True,heads_rva=8192,head_record_bytes=48),
        dict(event='sdk_shutdown',passed=True,after_presentation_stop=True),
        dict(event='finished',startup_passed=True,hooks_restored=True,sdk_shutdown_passed=True,swapchain_observed=True,
            pins_released=True,game_reflex_active=False,swapchain_proxy_installed=False,qualified_devices=1,
            pending_calls=0,presentation_pending=0,faults=0,presents=20,native_presents=20,queues=1,swapchains=1,resizes=0)]
    return rows

class ReviewTests(unittest.TestCase):
    def test_valid(self):
        self.assertEqual(review.validate(valid())['hooks_restored'],14)
    def test_bad_contracts(self):
        for event,key,value in (('start','mode',1),('start','sleep',True),('start','upscaling',True),
            ('sdk_initialized','before_native_queues_and_swapchain',False),('device_checked','device',201),
            ('factory_context','output_owner_matches',False),('factory_context','factory',999),
            ('factory_context','source','published-global'),
            ('presentation_installed','returned_interfaces_native',False),('presentation_cleanup','restored',False),
            ('sdk_shutdown','passed',False),('finished','pending_calls',1),('finished','pins_released',False),
            ('finished','native_presents',21),('finished','faults',1),('heartbeat','presents',21),
            ('heartbeat','native_swapchain',203)):
            with self.subTest(event=event,key=key):
                rows=valid(); next(row for row in rows if row['event']==event)[key]=value
                with self.assertRaises(ValueError): review.validate(rows)
    def test_missing_duplicate_order_and_fault(self):
        for event in ('factory_context','presentation_hook_restoration','presentation_cleanup','sdk_shutdown','finished'):
            rows=valid(); index=next(i for i,row in enumerate(rows) if row['event']==event)
            missing=copy.deepcopy(rows); missing.pop(index)
            duplicate=copy.deepcopy(rows); duplicate.insert(index,copy.deepcopy(rows[index]))
            for candidate in (missing,duplicate):
                with self.subTest(event=event):
                    with self.assertRaises(ValueError): review.validate(candidate)
        rows=valid(); cleanup=next(row for row in rows if row['event']=='presentation_cleanup'); rows.remove(cleanup); rows.insert(1,cleanup)
        with self.assertRaises(ValueError): review.validate(rows)
        rows=valid(); rows.insert(10,dict(event='runtime_rejection',reason='native-device-changed'))
        with self.assertRaises(ValueError): review.validate(rows)

if __name__=='__main__': unittest.main()
