"""Check bounded provenance receipts; never infer D3D state from native intent."""
from arm_continuous_dlaa import require

MAX32=2**32-1

def uint(v,limit=2**64-1):
    require(type(v) is int and 0<=v<=limit,'native provenance integer')

def review(p):
    require(p['schema']==1 and p['diagnostic_only'] is True,'native provenance policy')
    for k in ('renderer','epoch','array'):uint(p[k])
    for k in ('packets','scanned','index','matches'):uint(p[k],MAX32)
    for k in ('readable','scan_complete','stable'):require(type(p[k]) is bool,'provenance flag')
    require(p['scanned']<=min(p['packets'],32768) and p['matches']<=p['scanned'],'packet scan bounds')
    require((p['index']<p['scanned']) if p['matches'] else p['index']==MAX32,'packet position')
    if p['scan_complete']:require(p['readable'] and 0<p['packets']==p['scanned']<=32768,'complete schedule scan')
    if p['stable']:require(p['readable'],'stable schedule read')
    ts=p['trackers'];require(type(ts) is list and len(ts)==3,'tracker roles')
    for t in ts:
        for k in ('address','first','last','epoch','used_epoch','immediate_epoch'):uint(t[k])
        for k in ('initial','final','immediate'):uint(t[k],65535)
        uint(t['last_use'],MAX32);uint(t['initialized'],255)
        for k in ('readable','stable','complete'):require(type(t[k]) is bool,'tracker flag')
        if t['stable'] or t['complete']:require(t['readable'],'tracker read required')
        spans=t['spans'];require(type(spans) is list and len(spans)<=16,'span cap')
        if spans:require(t['readable'],'span read required')
        for i,row in enumerate(spans):
            require(type(row) is list and len(row)==8,'span shape')
            for n in row:uint(n)
            uint(row[2],MAX32);uint(row[5],65535);uint(row[6],65535);uint(row[7],255)
            require(row[0]>0,'span address')
            if i:require(spans[i-1][3]==row[0],'span linkage')
        if spans:require(spans[0][0]==t['first'],'first span identity')
        if t['complete']:require((spans[-1][3]==0) if spans else t['first']==0,'complete span chain')
    earlier=p['earlier'];require(type(earlier) is list and len(earlier)<=32,'earlier packet cap')
    if earlier:require(p['scan_complete'] and p['matches']==1,'earlier packet association')
    for i,v in enumerate(earlier):
        for k in ('index','opcode','visited','relevant'):uint(v[k],MAX32)
        uint(v['packet']);require(v['packet']>0 and type(v['complete']) is bool,'earlier identity')
        require(v['index']==max(0,p['index']-32)+i and v['index']<p['index'],'earlier ordinal')
        require(v['opcode']<32 and v['relevant']<=v['visited']<=128,'earlier scan count')
        require(type(v['nodes']) is list and len(v['nodes'])==min(v['relevant'],16),'earlier node cap')
        previous=-1
        for row in v['nodes']:
            require(type(row) is list and len(row)==5,'earlier node row')
            for n in row:uint(n)
            require(previous<row[0]<v['visited'] and 0<row[1]<8 and row[2]<=65535 and row[3]<=65535,'earlier node metadata')
            previous=row[0]
    return True
