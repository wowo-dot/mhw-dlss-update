"""Review frame-matched native scaling construction; no GPU execution claim."""
import math
from temporal_shader import require


def review(header, handoff):
    d=handoff['downstream']
    require(d['schema']==1 and d['policy']=='native-downstream-scale-observer-v1','downstream policy')
    require(d['fault']==d['pending']==d['mutations']==0 and d['selected']==3 and d['seen']>=3,'downstream counts')
    require(d['hooks_disabled'] is True and d['drained_before_disable'] is True,'downstream cleanup')
    require(d['target']==header['base']+0x2297e20 and d['expected_caller']==header['base']+0x23c7b08,'downstream code identity')
    require(d['gpu_consumption_verified'] is False,'downstream scope')
    require(len(d['records'])==len(handoff['records'])==3,'downstream record count')
    draws=[]
    for i,(r,c) in enumerate(zip(d['records'],handoff['records'])):
        require(r['index']==i and all(r[k] is True for k in ('valid','returned','stable')),'downstream qualification')
        require(all(r[k]==c[k] for k in ('frame','epoch','bank','async','thread','context','view')),'downstream frame association')
        require(r['caller']==d['expected_caller'] and r['attachment']>0,'downstream caller')
        require(0<=r['count_before']<r['count_after']<=c['count_before'] and
                r['count_after']-r['count_before']==len(r['packets']) and 1<=len(r['packets'])<=8,'downstream recorded before final copy')
        stage=r['sort']&31
        require(stage>15 and all(p['object']>0 and 0<=p['opcode']<=31 and p['key']>>27==stage for p in r['packets']),'downstream sorted stage')
        require(r['triangle'] in (0,1) and len(r['offset'])==2 and all(math.isfinite(x) and abs(x)<=1 for x in r['offset']),'downstream UV offset')
        rect,viewport,dimensions=r['rectangle'],r['viewport'],r['dimensions']
        require(len(rect)==len(viewport)==4 and len(dimensions)==2 and
                0<=rect[0]<rect[2]<=dimensions[0]<=16384 and 0<=rect[1]<rect[3]<=dimensions[1]<=16384,'downstream source extent')
        require(0<=viewport[0]<viewport[2]<=16384 and 0<=viewport[1]<viewport[3]<=16384,'downstream viewport')
        for owner in (r['source'],r['destination']):
            require(all(owner[k]>0 for k in ('owner','resource','type','width','height')),'downstream engine owner')
        require(dimensions==[r['source']['width'],r['source']['height']],'downstream UV allocation')
        require(viewport[2]<=r['destination']['width'] and viewport[3]<=r['destination']['height'],'downstream destination bounds')
        require(r['view_output']==r['destination']['owner'] and r['view_color']>0,'downstream view ownership')
        require(r['taa_destination']==c['destination']['owner'],'downstream TAA destination')
        require(1<=len(r['filters'])<=16 and 0<=r['taa_index']<len(r['filters']),'downstream filter bounds')
        taa=r['filters'][r['taa_index']]
        require(taa['entry']==c['layer']+0x140 and taa['unit']==c['layer'] and taa['stage']==15 and taa['order']==2 and
                taa['output']==c['destination']['owner'],'downstream TAA filter association')
        require(len({f['entry'] for f in r['filters']})==len(r['filters']) and
                all(f['entry']>0 and 0<=f['stage']<=31 and len(f['flags'])==3 for f in r['filters']),'downstream filter entries')
        draws.append(dict(frame=r['frame'],source_rectangle=rect,destination_viewport=viewport,
                          source_owner=r['source']['owner'],destination_owner=r['destination']['owner'],stage=stage,
                          taa_index=r['taa_index'],filter_count=len(r['filters'])))
    return dict(downstream_cpu_construction_verified=True,downstream_draws=draws,
                downstream_gpu_consumption_verified=False,full_output_handoff_verified=False)
