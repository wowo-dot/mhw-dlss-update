"""Control a versioned projection binding experiment, bound to process creation."""
import argparse
import ctypes as c
from ctypes import wintypes as w
import hashlib
import json
from pathlib import Path
from review_projection_binding import parse, require, reservation, MAX_BYTES

def validate(records, phase, pid, creation):
    h=records[0]
    require(not h.get('owned_fixture'),'cannot arm a fixture')
    require(h['pid']==pid and h['creation']==creation,'process generation mismatch')
    require(not any(r['event'] in ('failed','stopped') for r in records),'recorder is not active')
    if phase:
        reservation(records)
        require(not any(r['event']=='armed' for r in records),'already armed')
        if h['version'] in ('0.4.0','0.5.0','0.6.0','0.7.0','0.7.1','0.8.0','0.8.1','0.8.2','0.8.3','0.8.4','0.9.0','0.9.1'):
            from review_projection_binding import one
            import projection_provenance
            projection_provenance.ready(one(records,'provenance_ready'),h['version'])
            if h['version'] in ('0.8.2','0.8.3','0.8.4','0.9.0','0.9.1'):require(one(records,'provenance_ready').get('same_frame_readback') is True,'same-frame selection disabled')
            if h['version'] in ('0.8.1','0.8.2','0.8.3','0.8.4','0.9.0','0.9.1'):require(one(records,'provenance_ready').get('frame_tagging') is True,'frame receipt hooks missing')
            require(one(records,'provenance_ready')['schema'] == (4 if h['version'] in ('0.7.0','0.7.1','0.8.0','0.8.1','0.8.2','0.8.3','0.8.4','0.9.0','0.9.1') else 3 if h['version']=='0.6.0' else 2 if h['version']=='0.5.0' else 1), 'version/schema mismatch')
            if h['version'] in ('0.7.1','0.8.0','0.8.1','0.8.2','0.8.3','0.8.4','0.9.0','0.9.1'):
                require(one(records,'provenance_ready').get('copy_retention')=='per-call-until-drain','copy retention version')
            if h['version'] in ('0.8.3','0.8.4','0.9.0','0.9.1'):require(one(records,'provenance_ready').get('triplet_readback') is True,'triplet readback disabled')
            if h['version'] in ('0.9.0','0.9.1'):require(one(records,'provenance_ready').get('sequence_readback') is True and one(records,'provenance_ready').get('readback_slots')==5,'sequence readback disabled')
            ready=one(records,'ready')
            require(ready.get('hooks_installed') is True and ready.get('core_hooks_installed') is False,'not ready')
            require(records.index(one(records,'provenance_ready'))<records.index(ready),'watch chronology')
        else:
            require(any(r['event']=='ready' and r.get('hooks_installed') is False for r in records),'not ready')
    else:
        require(any(r['event']=='armed' for r in records),'capture not armed')

def event_name(header, phase):
    require(header['schema']==1 and phase in (0,1),'control version/action')
    return f'Local\\MhwProjectionBinding-v{header["schema"]}-{header["pid"]}-{header["creation"]}-{phase}'

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

