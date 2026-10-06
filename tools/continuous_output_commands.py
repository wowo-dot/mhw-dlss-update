"""Bounded prepared-queue observations; these do not prove GPU execution."""
from copy import deepcopy
from arm_continuous_dlaa import require

def integer(x, maximum=2**64-1):
    return type(x) is int and 0 <= x <= maximum

def vector(x, n, maximum=2**64-1):
    require(type(x) is list and len(x)==n and all(integer(v,maximum) for v in x),'command vector')

def gate(g, reason=None):
    vector(g,8)
    branch,key,address,index,raw,read,seen,total=g
    require(1<=branch<=11 and key<=2**32-1 and index<=2**32-1 and raw<=2**32-1 and read in (0,1)
            and seen<=65534 and total<=8, 'command gate bounds')
    require(read or raw==0,'unread command word')
    if reason is not None:require(reason==(10 if branch==4 else 17),'command gate reason')
    if branch in (5,6,7,11):
        require(read==1 and index<65535 and address>0,'command gate packet identity')
        if branch==5:require(raw&31==0 and seen>0,'duplicate draw gate')
        if branch==6:require(raw&31==0 and total==8,'draw capacity gate')
        if branch==7:require(raw&31 not in (0,11),'unsupported opcode gate')
        if branch==11:require(raw&31==0 and total>0,'duplicate packet gate')
    if branch==4:require(read==0,'unreadable packet gate')

def qualification(value, reason, detail, command_gate=None):
    require(integer(value) and value>>16<=11 and
            (value==0 or 1<=value&65535<=14), 'command qualification')
    low=value&65535
    if low==14:
        require(value==14 and reason==17 and detail==0 and command_gate is not None,'native DOF qualification identity')
        gate(command_gate,reason)
        require(command_gate[0]==10 and command_gate[1]>0 and command_gate[2:7]==[0,2**32-1,0,0,0],'native DOF qualification gate')
    if low in (12,13):
        # Historical discovery reports omitted the failed draw. Since v1.25.42,
        # discovery includes its native filter key and packet; ordering still
        # reports the whole plan. Neither form reads a command word here.
        require(value==low and reason==17 and detail==0 and command_gate is not None,
                'dependency qualification identity')
        gate(command_gate,reason)
        legacy=command_gate[:7]==[10 if low==12 else 8,0,0,2**32-1,0,0,0]
        branch,key,packet,index,raw,read,seen,total=command_gate
        failed_draw=(low==12 and branch==10 and key>>27==15 and
                     100<(key&0x7ffffff)<0x7fffff1 and packet>0 and
                     index==2**32-1 and raw==read==seen==0 and 0<total<=8)
        require(legacy or failed_draw,'dependency qualification gate')

def review(t, previous=None):
    require(t['schema']==1 and t['policy']=='prepared-command-rejections-v1' and t['diagnostic_only'] is True
            and t['capacity']==16 and t['packet_capacity']==32,'command capture policy')
    require(integer(t['attempts']) and integer(t['overflow']) and type(t['samples']) is list and len(t['samples'])<=16,'command capture bounds')
    signatures=[]
    for s in t['samples']:
        vector(s['signature'],6);vector(s['at'],8);vector(s['gate'],8)
        reason,detail,qualified,branch,key,opcode=s['signature']
        require(branch==s['gate'][0] and key==s['gate'][1] and opcode==(s['gate'][4]&31 if s['gate'][5] else 32),'command signature identity')
        if branch:gate(s['gate'],reason)
        else:require(reason==6 and detail==4 and s['gate']==[0]*8,'color chain capture identity')
        qualification(qualified,reason,detail,s['gate'])
        require(s['signature'] not in signatures,'duplicate command shape');signatures.append(s['signature'])
        require(all(integer(s[k]) for k in ('occurrences','queue','total','scanned','matched','unreadable','error'))
                and s['occurrences']>0 and s['total']<=2**32-1 and s['scanned']<=65534 and s['error'] in (0,1,2),'command capture counters')
        require(s['unreadable']<=s['matched']<=s['scanned']<=s['total'],'command scan coverage')
        require(type(s['stable']) is bool and (not s['stable'] or (s['error']==0 and s['scanned']==s['total'])),'command stable receipt')
        if s['error']==0:require(0<s['total']<65535 and s['queue']>0 and s['scanned']==s['total'],'command complete scan')
        if s['error']==1:require(s['scanned']==0,'command invalid header traversal')
        vector(s['opcodes'],32,65534);require(sum(s['opcodes'])+s['unreadable']==s['matched'],'command opcode coverage')
        require(type(s['filters']) is list and len(s['filters'])<=16,'command filter bound')
        for f in s['filters']:vector(f,7)
        require(type(s['packets']) is list and len(s['packets'])==min(32,s['matched']),'command packet capacity')
        def packet(p):
            require(integer(p['index'],65534) and p['index']<s['scanned'] and integer(p['key'],2**32-1)
                    and integer(p['address']) and integer(p['raw'],2**32-1),'command packet fields')
            require(type(p['raw_read']) is bool and type(p['prefix_read']) is bool and (not p['prefix_read'] or p['raw_read']),'command packet reads')
            vector(p['prefix'],18,2**32-1)
            require((p['raw_read'] or p['raw']==0) and (p['prefix_read'] or p['prefix']==[0]*18),'command unread fields')
            require(any(f[1]==15 and 100<f[2]<0x7fffff1 and p['key']==(f[1]<<27)|f[2] for f in s['filters']),'command packet filter identity')
        for p in s['packets']:packet(p)
        require(all(a['index']<b['index'] for a,b in zip(s['packets'],s['packets'][1:])),'command packet order')
        if s['focus'] is not None:
            p=s['focus'];packet(p)
            require(branch>0 and (p['index']==s['gate'][3] if s['gate'][3]!=2**32-1 else p['address']==s['gate'][2]),'command focus identity')
    require(sum(s['occurrences'] for s in t['samples'])+t['overflow']==t['attempts'],'command attempt coverage')
    require(not t['overflow'] or len(t['samples'])==16,'command saturation coverage')
    if previous:
        require(t['attempts']>=previous['attempts'] and t['overflow']>=previous['overflow'] and len(t['samples'])>=len(previous['samples']),'command counter rollback')
        for a,b in zip(previous['samples'],t['samples']):
            require(b['occurrences']>=a['occurrences'] and all(b[k]==v for k,v in a.items() if k!='occurrences'),'command immutable capture')
    return deepcopy(t)
