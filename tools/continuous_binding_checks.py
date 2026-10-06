"""Validate diagnostic binding differences, including failed-renderer receipts."""
from copy import deepcopy
from arm_continuous_dlaa import require
from continuous_output_commands import integer, vector

def check(c, schema=1):
    for k in ('observed','readable','contract','equal'):require(type(c[k]) is bool,'binding check boolean')
    vector(c['read'],2,255);phase,index=c['read'];require(phase<=12,'binding read phase')
    require(integer(c['fields'],2047) and integer(c['differences'],549 if schema==2 else 533),'binding difference bounds')
    require(type(c['changes']) is list and len(c['changes'])==min(16,c['differences']),'bounded binding changes')
    mask=0;keys=[];limits=(1,64,64,64,4,16,256,65,5,5,5) if schema==2 else (1,64,64,64,4,16,256,52,4,4,4)
    for v in c['changes']:
        vector(v,4);field,i,before,after=v
        require(field<len(limits) and i<limits[field] and before!=after,'binding change fields')
        mask|=1<<field;keys.append((field,i))
    require(len(set(keys))==len(keys) and mask&c['fields']==mask,'binding change coverage')
    if c['differences']<=16:require(mask==c['fields'],'complete difference mask')
    require((c['differences']==0)==(c['fields']==0),'binding mask count')
    if not c['observed']:
        require(not any(c[k] for k in ('readable','contract','equal','fields','differences')) and c['read']==[0,0],'unobserved check')
    else:
        require(c['readable']==(phase==0),'binding read outcome')
        require(not c['contract'] or c['readable'],'contract after read')
        require(c['equal']==(c['readable'] and c['differences']==0),'binding equality evidence')
    return all(c[k] for k in ('observed','readable','contract','equal'))

def review(t,previous=None):
    require(t['schema'] in (1,2) and t['policy']=='dependency-binding-boundaries-v'+str(t['schema']) and t['diagnostic_only'] is True and t['capacity']==3,'binding trace policy')
    for k in ('entry_checks','draw_checks','entry_rejections','draw_rejections'):require(integer(t[k]),'binding trace count')
    require(t['entry_rejections']<=t['entry_checks'] and t['draw_rejections']<=t['draw_checks'],'binding trace accounting')
    require(type(t['samples']) is list and len(t['samples'])==min(3,t['draw_rejections']),'binding trace samples')
    for s in t['samples']:
        vector(s['at'],8);a=s['at'];require(a[0]>0 and a[1]>0 and a[2]>0 and 4<=a[3]<12 and a[4] in ((1,2,3) if t['schema']==2 else (1,2)) and all(a[5:]),'binding trace identity')
        check(s['entry'],t['schema']);require(s['draw']['observed'] and not check(s['draw'],t['schema']),'failure trace outcome')
    if previous:
        require(all(t[k]>=previous[k] for k in ('entry_checks','draw_checks','entry_rejections','draw_rejections')) and t['samples'][:len(previous['samples'])]==previous['samples'],'immutable binding trace')
    return deepcopy(t)
