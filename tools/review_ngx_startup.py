"""Review ordered startup capabilities; never accept feature evaluation."""
import argparse,hashlib,json
from pathlib import Path

HOST='c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea'
def require(ok,why):
    if not ok:raise ValueError(why)

def review(rows):
    h=rows[0]
    require(h['schema']==1 and h['version'] in ('0.7.0','0.8.0','0.8.1','0.8.2','0.8.3','1.12.0','1.12.1','1.13.0','1.14.0','1.15.0','1.15.1','1.15.2','1.15.3','1.16.0','1.17.0','1.18.0','1.18.1','1.18.2','1.18.3','1.18.4','1.19.0','1.19.1','1.20.0','1.20.1', '1.20.2', '1.21.0', '1.21.1', '1.21.2', '1.21.3', '1.21.4', '1.21.5', '1.24.0','1.24.1','1.24.2','1.24.3','1.24.4','1.24.5','1.25.0', '1.25.1', '1.25.2', '1.25.3', '1.25.4', '1.25.5', '1.25.6', '1.25.7', '1.25.8', '1.25.9', '1.25.10', '1.25.11', '1.25.12', '1.25.13', '1.25.14', '1.25.15', '1.25.16', '1.25.17', '1.25.18', '1.25.19', '1.25.20', '1.25.21', '1.25.22', '1.25.23', '1.25.24', '1.25.25', '1.25.26', '1.25.27', '1.25.28', '1.25.29', '1.25.30', '1.25.31', '1.25.32', '1.25.33', '1.25.34', '1.25.35', '1.25.36', '1.25.37', '1.25.38', '1.25.39', '1.25.40', '1.25.41', '1.25.42', '1.25.43', '1.25.44') and h['experiment']=='ngx-startup-capabilities','startup version')
    expected=['start','installing','native_enter','public_init','capabilities',
              'native_return','capabilities','graphics','stopped']
    if h['version']!='1.12.0':expected.insert(1,'code_readiness')
    require([r['event'] for r in rows]==expected,'startup event sequence')
    if h['version']!='1.12.0':
        ready=rows[1];install=rows[2]
        require(all(type(ready[k]) is int for k in ('started_tick','tick','polls','first_mask','final_mask','timeout_ms')),
                'startup readiness types')
        require(ready['timeout_ms']==300000 and ready['polls']>0 and 0<=ready['first_mask']<=15 and
                ready['final_mask']==15 and ready['slots_empty'] is True and ready['ready'] is True and
                ready['hooks_installed'] is False,'startup readiness admission')
        require(0<=ready['started_tick']<=ready['tick']<=install['tick'] and
                ready['tick']-ready['started_tick']<ready['timeout_ms'],'startup readiness timing')
        rows=[rows[0]]+rows[2:]
    h,install,entry,init,pre,native,post,graphics,end=rows
    require(h['host_sha256']==HOST and h['host_version']=='0.0.0.0','startup host')
    require(all(type(h[k]) is int and h[k]>0 for k in ('pid','creation','base')),'startup process')
    require(len(h['dll_sha256'])==64 and all(c in '0123456789abcdef' for c in h['dll_sha256']),'startup DLL')
    require(h['graphics_api']=='D3D12' and h['hooks']==1 and h['gpu_commands']==h['feature_creations']==0,'startup limits')
    require(install['sdk_initializations_limit']==1 and install['gpu_commands']==0,'startup initialization limit')
    require(entry['caller']==h['base']+0x23842dc and entry['device']>0 and entry['api']==18 and entry['slots_zero'] is True,'startup admission')
    require(init['result']==1 and init['api']==21 and 0<=init['adapter_luid_low']<2**32 and
            -(2**31)<=init['adapter_luid_high']<2**31,'public initialization')
    require(init['runtime_sha256']=='be6e434a94ca32499515eb62ca0e6c274526055d568d0426e4c652dcdfb6ee6e','public provider identity')
    require(init['project_id']=='956cdd19-7d3c-4660-b664-91c20d2bc9bd','public project identity')
    require(install['tick']<=entry['tick']<=init['tick']<=native['tick'],'startup timing')
    require(pre['phase']=='before_native' and post['phase']=='after_native','capability phases')
    for c in (pre,post):
        require(c['get_result']==1 and c['get_values']==[1,1,1] and c['values_u32']==[1,0,1] and
                c['destroy_result']==1 and c['modern_loaded'] is True and c['provider_identity_verified'] is True,'startup capabilities')
    require(native['result']==1 and native['arguments_unchanged'] is True and native['result_unchanged'] is True,'native client initialization')
    require(graphics['api']=='D3D12' and graphics['device']==entry['device'] and
            all(type(graphics[k]) is int and graphics[k]>0 for k in ('backend','swapchain','width','height','swapchain_format')),'startup graphics')
    require(end['calls']==1 and all(end[k] is True for k in ('transaction_complete','capabilities_supported','hook_disabled',
            'settled','swapchain_observed','dll_pinned','trampoline_retained','sdk_shutdown_deferred')),'startup closure')
    require(end['sdk_evaluation_verified'] is False,'startup coverage')
    return dict(accepted=True,meaning='Public modern initialization precedes the native client call and capabilities survive it.',
        pid=h['pid'],creation=h['creation'],adapter_luid=[init['adapter_luid_high'],init['adapter_luid_low']],
        width=graphics['width'],height=graphics['height'],swapchain_format=graphics['swapchain_format'],
        sdk_evaluation_verified=False,dlaa_ready=False)

def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('log',type=Path);ap.add_argument('--output',required=True,type=Path);a=ap.parse_args()
    require(not a.output.exists(),'preserve startup review');raw=a.log.read_bytes();require(0<len(raw)<=65536,'startup log size')
    try:result=review([json.loads(line) for line in raw.decode('utf-8').splitlines()])
    except (ValueError,KeyError,TypeError,IndexError) as e:result=dict(accepted=False,reason=str(e),sdk_evaluation_verified=False,dlaa_ready=False)
    result['log_sha256']=hashlib.sha256(raw).hexdigest()
    with a.output.open('x') as out:json.dump(result,out,indent=2)
    print(json.dumps(result,indent=2));return 0 if result['accepted'] else 1
if __name__=='__main__':raise SystemExit(main())
