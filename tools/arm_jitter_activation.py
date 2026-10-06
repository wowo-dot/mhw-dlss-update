"""Control the scoped camera-jitter experiment, bound to process creation."""
import argparse
import ctypes as c
from ctypes import wintypes as w
import hashlib
import json
from pathlib import Path
from review_jitter_activation import parse, require, MAX_BYTES

def validate(records, phase, pid, creation):
    h=records[0]
    require(h['pid']==pid and h['creation']==creation,'process generation mismatch')
    require(not any(r['event'] in ('failed','stopped') for r in records),'recorder is not active')
    if phase:
        require(not any(r['event']=='armed' for r in records),'already armed')
        require(any(r['event']=='ready' and r.get('hooks_installed') is False for r in records),'not ready')
    else:
        require(any(r['event']=='armed' for r in records),'capture not armed')

def event_name(header, phase):
    require(header['schema']==1 and phase in (0,1),'control version/action')
    return f'Local\\MhwJitterActivation-v{header["schema"]}-{header["pid"]}-{header["creation"]}-{phase}'

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--log',type=Path,required=True)
    ap.add_argument('--action',choices=['arm','stop'],required=True)
    ap.add_argument('--output',type=Path,required=True)
    a=ap.parse_args()
    require(not a.output.exists(),'preserve existing receipt')
    require(a.log.stat().st_size<=MAX_BYTES,'capture size')
    raw=a.log.read_bytes(); records=parse(raw); pid=records[0]['pid']
    phase={'stop':0,'arm':1}[a.action]
    k=c.WinDLL('kernel32',use_last_error=True)
    k.OpenProcess.argtypes=[w.DWORD,w.BOOL,w.DWORD]; k.OpenProcess.restype=w.HANDLE
    k.GetProcessTimes.argtypes=[w.HANDLE]+[c.POINTER(w.FILETIME)]*4; k.GetProcessTimes.restype=w.BOOL
    k.OpenEventW.argtypes=[w.DWORD,w.BOOL,w.LPCWSTR]; k.OpenEventW.restype=w.HANDLE
    k.SetEvent.argtypes=[w.HANDLE]; k.SetEvent.restype=w.BOOL
    k.CloseHandle.argtypes=[w.HANDLE]
    k.GetTickCount64.restype=c.c_uint64
    process=k.OpenProcess(0x1000,False,pid); require(process,'process unavailable')
    try:
        t=[w.FILETIME() for _ in range(4)]
        require(k.GetProcessTimes(process,*map(c.byref,t)),'process times unavailable')
        creation=(t[0].dwHighDateTime<<32)|t[0].dwLowDateTime
        require(not(t[1].dwHighDateTime or t[1].dwLowDateTime),'process exited')
        validate(records,phase,pid,creation)
        event=k.OpenEventW(2,False,event_name(records[0],phase))
        require(event,'event unavailable')
        try:
            require(k.SetEvent(event),'event signal failed')
            tick=k.GetTickCount64()
        finally: k.CloseHandle(event)
    finally: k.CloseHandle(process)
    receipt=dict(pid=pid,creation=creation,phase=phase,requested_tick=tick,request_sent=True,acknowledged=False,
        process_access='PROCESS_QUERY_LIMITED_INFORMATION',event_access='EVENT_MODIFY_STATE',process_memory_writes=0,
        log_sha256_at_request=hashlib.sha256(raw).hexdigest(),next_check='Require armed, capture_complete and clean stopped records.')
    with a.output.open('x',encoding='utf-8') as f: json.dump(receipt,f,indent=2)
    print(json.dumps(receipt,indent=2))

if __name__=='__main__': main()

