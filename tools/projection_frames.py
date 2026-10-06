"""Link one readback to the original copy-request stamp, never all scene draws."""
from projection_provenance import require, uint


def packet(p):
    require(type(p) is dict and p.get('opcode')==10 and p.get('kind')==8, 'depth packet type')
    require(all(uint(p.get(k)) and p[k]>0 for k in ('source','destination')) and
            p['source']!=p['destination'] and uint(p.get('nested')), 'depth packet owners')
    require(type(p.get('rectangle')) is list and len(p['rectangle'])==6 and
            all(uint(v,32) for v in p['rectangle']), 'depth packet rectangle')


def validate(t, depth, source):
    f=t.get('frame_packets',{})
    require(f.get('schema')==1 and f.get('capacity')==256 and uint(f.get('renderer')) and
            f['renderer']==source['renderer']>0 and f['renderer']<2**64-0x1f218, 'frame packet header')
    receipts, consumers=f.get('receipts'),f.get('consumers')
    orders=[]
    for name,rows in (('receipts',receipts),('consumers',consumers)):
        require(type(rows) is list and 0<len(rows)==f.get(name+'_reserved')<=256, 'frame packet capacity')
        for r in rows:
            require(r.get('published')==2 and r.get('valid') is True and r.get('returned') is True, 'frame packet completion')
            require(all(uint(r.get(k)) and r[k]>0 for k in ('enter','exit','thread','context','packet')) and
                    r['enter']<r['exit'], 'frame packet interval')
            packet(r.get('data'));orders.extend([r['enter'],r['exit']])
    require(len(set(orders))==len(orders), 'frame packet duplicate order')
    native_orders={e[k] for e in t['history']+t['watch'] for k in ('enter','exit')}
    require(not (set(orders)&native_orders), 'frame/native order collision')
    for r in receipts:
        require(r['context']==f['renderer']+0x710 and uint(r.get('caller')) and r['caller']>0, 'frame producer context')
        require(all(uint(r.get(k)) for k in ('frame','epoch','bank','async_mode')) and r['bank']<=1 and
                r['async_mode']<=1, 'frame producer stamp')
        require(all(uint(r.get(k),32) for k in ('before_count','after_count')) and
                0<=r['before_count']<r['after_count']<=65536 and r['after_count']-r['before_count']<=64,
                'frame publication extent')
        require(all(uint(r.get(k)) and r[k]>0 for k in ('before_array','after_array','source','destination')) and
                r['after_array']+16*r['after_count']<2**64, 'frame publication array')
        require((r['source'],r['destination'])==(r['data']['source'],r['data']['destination']), 'frame publication owners')
    site=depth['site']
    matches=[c for c in consumers if c.get('copy')==site['copy']]
    require(len(matches)==1, 'readback packet association absent or ambiguous')
    c=matches[0]
    require(uint(c.get('receipt'),32) and 0<c['receipt']<=len(receipts), 'readback producer receipt')
    p=receipts[c['receipt']-1]
    require(c['data']==p['data'] and c['packet']==p['packet'] and p['exit']<c['enter'], 'packet publication/consumption')
    require(c.get('native_calls')==1 and c['enter']<c['copy']<c.get('copy_exit',0)<c['exit']<site['close'], 'packet copy nesting')
    require(c['command']==site['command'] and c['source_resource']==site['resource'] and
            all(uint(c.get(k)) and c[k]>0 for k in ('executor','command','source_resource','destination_resource')), 'packet copy identity')
    calls=[e for e in t['history'] if e['enter']==site['copy']]
    require(len(calls)==1,'tagged native copy missing')
    e=calls[0]
    require(e['kind']==16 and e['thread']==c['thread'] and e['object']==c['command'] and
            e['resource']==c['source_resource'] and e['backing']==c['destination_resource'] and e['exit']==c['copy_exit'], 'tagged copy arguments')
    # Reused packet addresses are allowed only when the selected receipt is a
    # complete new generation, with no other construction during its lifetime.
    for other in receipts:
        if other is p or other['packet']!=p['packet']:continue
        require(other['exit']<p['enter'] or other['enter']>c['exit'], 'packet reused before consumption completed')
    require(sum(x.get('receipt')==c['receipt'] for x in consumers)==1, 'packet consumed more than once')
    require(uint(source['frame']) and uint(source['epoch']), 'selected projection stamp')
    return dict(depth_copy_request_frame_verified=True,depth_copy_request_frame=p['frame'],
        depth_copy_request_epoch=p['epoch'],depth_copy_request_bank=p['bank'],
        depth_copy_frame_delta_from_selected_draw=p['frame']-source['frame'],
        depth_copy_epoch_delta_from_selected_draw=p['epoch']-source['epoch'],
        depth_copy_packet=p['packet'],depth_copy_packet_producer_order=p['enter'],
        depth_copy_packet_consumer_order=c['enter'],
        depth_pixel_producer_frame_verified=False,
        depth_frame_scope='Original copy-request stamp; all contributing depth draws are not covered.')
