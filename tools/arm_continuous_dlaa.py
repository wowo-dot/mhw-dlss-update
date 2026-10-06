"""Signal the continuous DLAA prototype for this exact process generation."""
import argparse
import ctypes as c
from ctypes import wintypes as w
import hashlib,json
from pathlib import Path
# Bounded provenance snapshots can be larger than the original counters-only
# trace. Keep stop signaling usable for the complete short diagnostic log.
MAX_BYTES=16*1048576
HOST="c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea"
def require(ok,reason):
    if not ok: raise ValueError(reason)
def parse(raw):
    require(0<len(raw)<=MAX_BYTES,"log size")
    rows=[json.loads(line) for line in raw.decode("utf-8").splitlines() if line.strip()]
    require(rows and all(isinstance(row,dict) for row in rows),"records")
    h=rows[0]
    require(h.get("event")=="start" and h.get("version") in ("1.18.0", "1.18.1", "1.18.2", "1.18.3", "1.18.4", "1.19.0", "1.19.1", "1.20.0", "1.20.1", "1.20.2", "1.21.0", "1.21.1", "1.21.2", "1.21.3", "1.21.4", "1.21.5", "1.25.0", "1.25.1", "1.25.2", "1.25.3", "1.25.4", "1.25.5", "1.25.6", "1.25.7", "1.25.8", "1.25.9", "1.25.10", "1.25.11", "1.25.12", "1.25.13", "1.25.14", "1.25.15", "1.25.16", "1.25.17", "1.25.18", "1.25.19", "1.25.20", "1.25.21", "1.25.22", "1.25.23", "1.25.24", "1.25.25", "1.25.26", "1.25.27", "1.25.28", "1.25.29", "1.25.30", "1.25.31", "1.25.32", "1.25.33", "1.25.34", "1.25.35", "1.25.36", "1.25.37", "1.25.38", "1.25.39", "1.25.40", "1.25.41", "1.25.42", "1.25.43", "1.25.44") and h.get("schema")==1,"continuous header")
    require(h.get("host_sha256")==HOST and h.get("host_version")=="0.0.0.0","host identity")
    if h['version'] in ('1.19.0', '1.19.1', '1.20.0', '1.20.1', '1.20.2', '1.21.0', '1.21.1', '1.21.2', '1.21.3', '1.21.4', '1.21.5', '1.25.0', '1.25.1', '1.25.2', '1.25.3', '1.25.4', '1.25.5', '1.25.6', '1.25.7', '1.25.8', '1.25.9', '1.25.10', '1.25.11', '1.25.12', '1.25.13', '1.25.14', '1.25.15', '1.25.16', '1.25.17', '1.25.18', '1.25.19', '1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):
        require(h.get('experiment') == ('continuous-scaled-native-resolve' if h['version'] in ('1.25.0','1.25.1', '1.25.2', '1.25.3', '1.25.4', '1.25.5', '1.25.6', '1.25.7', '1.25.8', '1.25.9', '1.25.10', '1.25.11', '1.25.12', '1.25.13', '1.25.14', '1.25.15', '1.25.16', '1.25.17', '1.25.18', '1.25.19', '1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44') else 'native-temporal-resolve') and
                h.get('replacement_scope') == 'qualified-native-resolve' and
                h.get('exclusive_mode') is False and h.get('retention') == 'reset-and-fence-v1',
                'qualified resolve experiment')
        if h['version'] in ('1.19.1', '1.20.0', '1.20.1', '1.20.2', '1.21.0', '1.21.1', '1.21.2', '1.21.3', '1.21.4', '1.21.5', '1.25.0', '1.25.1', '1.25.2', '1.25.3', '1.25.4', '1.25.5', '1.25.6', '1.25.7', '1.25.8', '1.25.9', '1.25.10', '1.25.11', '1.25.12', '1.25.13', '1.25.14', '1.25.15', '1.25.16', '1.25.17', '1.25.18', '1.25.19', '1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):
            require(h.get('request_retirement') == 'validated-native-return-v1', 'native request retirement policy')
        if h['version'] in ('1.20.0', '1.20.1', '1.20.2', '1.21.0', '1.21.1', '1.21.2', '1.21.3', '1.21.4', '1.21.5', '1.25.0', '1.25.1', '1.25.2', '1.25.3', '1.25.4', '1.25.5', '1.25.6', '1.25.7', '1.25.8', '1.25.9', '1.25.10', '1.25.11', '1.25.12', '1.25.13', '1.25.14', '1.25.15', '1.25.16', '1.25.17', '1.25.18', '1.25.19', '1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):
            require(h.get('activation') == ('native-postqueue-preparation-v2' if h['version'] in ('1.20.2', '1.21.0', '1.21.1', '1.21.2', '1.21.3', '1.21.4', '1.21.5', '1.25.0', '1.25.1', '1.25.2', '1.25.3', '1.25.4', '1.25.5', '1.25.6', '1.25.7', '1.25.8', '1.25.9', '1.25.10', '1.25.11', '1.25.12', '1.25.13', '1.25.14', '1.25.15', '1.25.16', '1.25.17', '1.25.18', '1.25.19', '1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44') else 'scoped-native-preparation-v1'), 'native activation policy')
    else:
        require(h.get('experiment') == 'continuous-dlaa-prototype', 'continuous experiment')
    require(h.get("frame_limit") is None and h.get("graphics_api")=="D3D12","continuous limits/API")
    if h['version'] in ('1.21.0', '1.21.1', '1.21.2', '1.21.3', '1.21.4', '1.21.5', '1.25.0', '1.25.1', '1.25.2', '1.25.3', '1.25.4', '1.25.5', '1.25.6', '1.25.7', '1.25.8', '1.25.9', '1.25.10', '1.25.11', '1.25.12', '1.25.13', '1.25.14', '1.25.15', '1.25.16', '1.25.17', '1.25.18', '1.25.19', '1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):require(h.get('reconstruction')=='suspend-or-latch-unfiltered-v1','reconstruction policy')
    require(all(type(h.get(key)) is int and h[key]>0 for key in ("pid","creation","base")),"process identity")
    if h['version'] in ('1.21.1', '1.21.2', '1.21.3', '1.21.4', '1.21.5', '1.25.0', '1.25.1', '1.25.2', '1.25.3', '1.25.4', '1.25.5', '1.25.6', '1.25.7', '1.25.8', '1.25.9', '1.25.10', '1.25.11', '1.25.12', '1.25.13', '1.25.14', '1.25.15', '1.25.16', '1.25.17', '1.25.18', '1.25.19', '1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):require(h.get('state_trace')=='consumer-local-state-v1','state trace policy')
    if h['version'] in ('1.21.2','1.21.3', '1.21.4', '1.21.5', '1.25.0', '1.25.1', '1.25.2', '1.25.3', '1.25.4', '1.25.5', '1.25.6', '1.25.7', '1.25.8', '1.25.9', '1.25.10', '1.25.11', '1.25.12', '1.25.13', '1.25.14', '1.25.15', '1.25.16', '1.25.17', '1.25.18', '1.25.19', '1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):require(h.get('native_state_trace')=='scheduled-state-evidence-v1','native provenance policy')
    if h['version'] in ('1.21.3', '1.21.4', '1.21.5', '1.25.0', '1.25.1', '1.25.2', '1.25.3', '1.25.4', '1.25.5', '1.25.6', '1.25.7', '1.25.8', '1.25.9', '1.25.10', '1.25.11', '1.25.12', '1.25.13', '1.25.14', '1.25.15', '1.25.16', '1.25.17', '1.25.18', '1.25.19', '1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):require(h.get('state_admission')=='native-current-use-v1' and h.get('copy_telemetry')=='single-fence-observation-v1','native state admission policy')
    if h['version'] in ('1.21.4', '1.21.5', '1.25.0', '1.25.1', '1.25.2', '1.25.3', '1.25.4', '1.25.5', '1.25.6', '1.25.7', '1.25.8', '1.25.9', '1.25.10', '1.25.11', '1.25.12', '1.25.13', '1.25.14', '1.25.15', '1.25.16', '1.25.17', '1.25.18', '1.25.19', '1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):require(h.get('depth_eligibility')=='depth-eligibility-observation-v1','depth eligibility policy')
    if h['version'] in ('1.21.5','1.25.0', '1.25.1', '1.25.2', '1.25.3', '1.25.4', '1.25.5', '1.25.6', '1.25.7', '1.25.8', '1.25.9', '1.25.10', '1.25.11', '1.25.12', '1.25.13', '1.25.14', '1.25.15', '1.25.16', '1.25.17', '1.25.18', '1.25.19', '1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44'):require(h.get('depth_destination')=='depth-destination-identity-v1','depth destination policy')
    return rows

def validate(rows,pid,creation,expected_hash,stop=False):
    h=rows[0]
    require(h['pid']==pid and h['creation']==creation,'process generation')
    require(h['dll_sha256']==expected_hash.lower(),'installed DLL identity')
    require(not any(r['event'] in ('failed','stopped','capture') for r in rows),'recorder finished')
    if not stop:
        require([r['event'] for r in rows]==['start','reservation','ready'],'recorder readiness')
        require(rows[1].get('ready') is True and rows[1].get('slot',0)>0 and rows[2].get('hooks_installed') is False,'reservation/readiness')

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--log',type=Path,required=True)
    ap.add_argument('--dll-sha256',required=True);ap.add_argument('--output',type=Path,required=True);ap.add_argument('--stop',action='store_true');a=ap.parse_args()
    require(not a.output.exists(),'preserve receipt');require(a.log.stat().st_size<=MAX_BYTES,'log size')
    raw=a.log.read_bytes();rows=parse(raw);pid=rows[0]['pid']
    k=c.WinDLL('kernel32',use_last_error=True)
    k.OpenProcess.argtypes=[w.DWORD,w.BOOL,w.DWORD];k.OpenProcess.restype=w.HANDLE
    k.GetProcessTimes.argtypes=[w.HANDLE]+[c.POINTER(w.FILETIME)]*4;k.GetProcessTimes.restype=w.BOOL
    k.OpenEventW.argtypes=[w.DWORD,w.BOOL,w.LPCWSTR];k.OpenEventW.restype=w.HANDLE
    k.SetEvent.argtypes=[w.HANDLE];k.SetEvent.restype=w.BOOL;k.CloseHandle.argtypes=[w.HANDLE];k.GetTickCount64.restype=c.c_uint64
    process=k.OpenProcess(0x1000,False,pid);require(process,'process unavailable')
    try:
        times=[w.FILETIME() for _ in range(4)];require(k.GetProcessTimes(process,*map(c.byref,times)),'process times')
        creation=(times[0].dwHighDateTime<<32)|times[0].dwLowDateTime
        require(not(times[1].dwHighDateTime or times[1].dwLowDateTime),'process exited')
        validate(rows,pid,creation,a.dll_sha256,a.stop)
        event=k.OpenEventW(2,False,f'Local\\MhwTemporalInputs-v1-{pid}-{creation}-{0 if a.stop else 1}')
        require(event,'event unavailable')
        try:require(k.SetEvent(event),'signal failed');tick=k.GetTickCount64()
        finally:k.CloseHandle(event)
    finally:k.CloseHandle(process)
    result=dict(pid=pid,creation=creation,requested_tick=tick,stop=a.stop,request_sent=True,acknowledged=False,
                log_sha256_at_request=hashlib.sha256(raw).hexdigest(),process_memory_writes=0)
    with a.output.open('x') as out:json.dump(result,out,indent=2)
    print(json.dumps(result,indent=2))
if __name__=='__main__':main()
