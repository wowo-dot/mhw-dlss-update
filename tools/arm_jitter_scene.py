"""Request one CPU scene sample burst from v0.15 after the user reaches gameplay."""
import argparse
import ctypes as c
from ctypes import wintypes as w
import hashlib
import json
from pathlib import Path
from review_submission import require, unique_object
from review_jitter_sequence import parse_trace


def validate_request(preflight, creation, text, now):
    require(preflight['read_only'] and preflight['writes'] == 0 and preflight['live_code_verified'], 'verified preflight required')
    require(preflight['creation'] == creation and creation > 0, 'process generation mismatch')
    require(len(preflight['channels']) == 2 and all(ch['producer'] == 1500 for ch in preflight['channels']), 'v0.15 channels required')
    require('trace.start version=0.15.0 ' in text and 'trace.complete' not in text, 'active v0.15 trace required')
    require('bridge.jitter-scene-event ready=true ' in text, 'scene event unavailable')
    require('bridge.jitter-scene-trigger accepted=true ' not in text, 'scene trigger already accepted')
    result = parse_trace(text)
    require(result['producer'] == 1500 and not result['stopped'] and not result['scene_armed_tick'] and
            result['phases'][0]['samples'] == 16 and result['phases'][1]['samples'] == 0, 'initial burst must be complete; scene must be unarmed')
    state = None
    for line in text.splitlines():
        if 'bridge.jitter-sequence-state ' in line:
            state = json.loads(line.split('bridge.jitter-sequence-state ',1)[1],object_pairs_hook=unique_object)
    require(state and 0 <= now-state['started'] < 115000, 'scene window has less than five seconds remaining')
    require(0 <= now-int(text.split('\t',1)[0]) < 295000, 'trace window has less than five seconds remaining')


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--preflight',type=Path,required=True)
    ap.add_argument('--output',type=Path,required=True)
    args=ap.parse_args()
    require(not args.output.exists(),'preserve existing trigger receipt')
    preflight=json.loads(args.preflight.read_text(encoding='utf-8-sig'),object_pairs_hook=unique_object)
    pid=preflight['pid']; require(type(pid) is int and 0 < pid <= 0xffffffff,'process ID')
    k=c.WinDLL('kernel32',use_last_error=True)
    k.OpenProcess.argtypes=[w.DWORD,w.BOOL,w.DWORD];k.OpenProcess.restype=w.HANDLE
    k.GetProcessTimes.argtypes=[w.HANDLE]+[c.POINTER(w.FILETIME)]*4;k.GetProcessTimes.restype=w.BOOL
    k.OpenEventW.argtypes=[w.DWORD,w.BOOL,w.LPCWSTR];k.OpenEventW.restype=w.HANDLE
    k.SetEvent.argtypes=[w.HANDLE];k.SetEvent.restype=w.BOOL
    k.CloseHandle.argtypes=[w.HANDLE]
    k.GetTickCount64.restype=c.c_uint64
    process=k.OpenProcess(0x1000,False,pid)
    require(process,'process unavailable')
    try:
        times=[w.FILETIME() for _ in range(4)]
        require(k.GetProcessTimes(process,*[c.byref(t) for t in times]),'process times')
        creation=(times[0].dwHighDateTime<<32)|times[0].dwLowDateTime
        require(not (times[1].dwHighDateTime or times[1].dwLowDateTime),'process exited')
        log=Path.home()/'AppData/Local/MhwDlssBridgeProbe'/f'bridge-{pid}.log'
        raw=log.read_bytes();text=raw.decode('utf-8-sig');now=k.GetTickCount64()
        validate_request(preflight,creation,text,now)
        event=k.OpenEventW(2,False,f'Local\\MhwDlssJitterScene-v1-{pid}-{creation}')
        require(event,'scene event unavailable for this process generation')
        try:require(k.SetEvent(event),'scene event signal failed')
        finally:k.CloseHandle(event)
    finally:k.CloseHandle(process)
    report=dict(pid=pid,creation=creation,requested_tick=now,request_sent=True,acknowledged=False,
                event_access='EVENT_MODIFY_STATE',process_access='PROCESS_QUERY_LIMITED_INFORMATION',
                process_memory_writes=0,rendering_admissions_added=0,preflight_sha256=hashlib.sha256(args.preflight.read_bytes()).hexdigest().upper(),
                trace_at_request_sha256=hashlib.sha256(raw).hexdigest().upper(),
                next_check='Require bridge.jitter-scene-trigger accepted=true and schema-2 phase-1 rows before claiming capture')
    with args.output.open('x') as out:json.dump(report,out,indent=2)
    print(json.dumps(report,indent=2))


if __name__=='__main__':main()
