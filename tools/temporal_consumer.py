"""Review native TAA recording association, without asserting GPU submission."""
from temporal_limits import frame_count
from temporal_shader import require

def derive_states(resource_ids, calls, barriers, dispatch_enter):
    states=[dict(known=False,conflict=False,value=0) for _ in resource_ids]
    for call in calls:
        if call['exit'] >= dispatch_enter:
            continue
        for b in barriers[call['first']:call['first']+call['count']]:
            for resource,s in zip(resource_ids,states):
                if b['type']==1 and (not b['resource'] or not b['other'] or resource in (b['resource'],b['other'])):
                    s.update(known=False,conflict=True)
                if b['type']!=0 or b['resource']!=resource:
                    continue
                if b['flags'] or b['subresource']!=0xffffffff:
                    s.update(known=False,conflict=True)
                    continue
                if s['known'] and s['value']!=b['before']:
                    s['conflict']=True
                s['value']=b['after'];s['known']=not s['conflict']
    return states

def review_consumer(header,capture):
    n=frame_count(header)
    native=capture['native_compute']
    require(native['fault']==0 and native['pending']==0 and native['completed']==n,'native closure')
    require(native['hooks_disabled'] is True and native['pins']==native['released']==n,'command list references')
    require(native['gpu_submission_verified'] is False,'native coverage')
    require(native['started_tick']==capture['started_tick'],'consumer clock')
    records=native['records'];require(len(records)==n,'native record count')
    generations=header['version'] in ('1.15.1','1.15.2','1.15.3','1.16.0','1.17.0','1.23.0','1.23.1','1.23.2','1.23.3','1.23.4','1.23.5','1.23.6','1.23.7','1.23.8','1.23.9','1.23.10','1.23.11','1.23.12','1.24.0','1.24.1','1.24.2','1.24.3','1.24.4','1.24.5')
    seen={};states=[];orders=set();last_publication=0
    def remember(values):
        require(all(type(v) is int and v>0 for v in values) and len(set(values))==len(values) and not orders.intersection(values),'consumer generation order collision')
        orders.update(values)
    for index,(r,cpu) in enumerate(zip(records,capture['records'])):
        require(r['receipt']==index and r['frame']==cpu['frame'] and r['packet']==cpu['packet'],'consumer receipt')
        previous=seen.get(r['packet'])
        if generations:
            publication=r['publication'];begin=r['producer_begin'];generation=r['packet_generation'];predecessor=r['packet_predecessor']
            require(type(publication) is int and type(begin) is int and begin==cpu['producer_order'] and last_publication<begin<publication<r['enter'],'packet publication order')
            require(type(generation) is int and 1<=generation<=n and type(predecessor) is int,'packet generation identity')
            if previous is None:
                require(generation==1 and predecessor==0xffffffff,'first packet generation')
            else:
                old=records[previous];old_cpu=capture['records'][previous]
                require(predecessor==previous and generation==old['packet_generation']+1,'packet generation chain')
                require(old['exit']<begin and old_cpu['frame']<cpu['frame'] and old_cpu['epoch']<cpu['epoch'],'packet reuse before completion/new frame')
            remember([begin,publication,r['enter'],r['dispatch_enter'],r['dispatch_exit'],r['exit']])
            last_publication=publication
        else:require(previous is None,'packet reused')
        seen[r['packet']]=index
        require(all(r.get(k) is True for k in ('valid','returned','packet_equal','recipe_equal','dispatch_returned')),'consumer stability')
        require(all(type(r.get(k)) is int and r[k]>0 for k in ('executor','context','recording','command','identity','thread')),'consumer identity')
        require(r['command_type']==0 and r['dispatch_calls']==1 and r['dispatch_caller']==header['base']+0x259cfca,'native dispatch')
        require(0<r['enter']<r['dispatch_enter']<r['dispatch_exit']<r['exit'],'native order')
        require(cpu['exit_tick']<=r['enter_tick']<=r['exit_tick']<capture['started_tick']+2000,'consumer interval')
        calls,barriers=r['barrier_calls'],r['barriers']
        require(len(calls)<=16 and len(barriers)<=128,'barrier budgets')
        offset=0;previous=r['enter']
        for c in calls:
            if generations:remember([c['enter'],c['exit']])
            require(c['first']==offset and type(c['count']) is int and 0<=c['count']<=128-offset,'barrier slices')
            offset+=c['count']
            require(c['returned'] is True and c['stable'] is True and c['caller']>0,'barrier completion')
            require(previous<c['enter']<c['exit']<r['exit'],'barrier order');previous=c['exit']
            require(c['exit']<r['dispatch_enter'] or c['enter']>r['dispatch_exit'],'barrier overlaps dispatch')
        require(offset==len(barriers),'barrier storage')
        for b in barriers:
            require(b['type'] in (0,1,2) and 0<=b['flags']<=3,'barrier kind')
            require(all(type(b[k]) is int and 0<=b[k]<=0xffffffff for k in ('subresource','before','after')),'barrier fields')
            require(all(type(b[k]) is int and 0<=b[k]<2**64 for k in ('resource','other')),'barrier pointers')
        derived=derive_states([v['resource'] for v in cpu['inputs']],calls,barriers,r['dispatch_enter'])
        require(r['states']==derived,'state derivation')
        states.append(dict(frame=r['frame'],motion_state=derived[5],barrier_calls=len(calls),barriers=len(barriers)))
    return dict(native_dispatch_recording_verified=True,gpu_submission_verified=False,recording_local_states=states,
                packet_generations_verified=generations,packet_address_reuses=n-len(seen),
                readback_eligible=False,motion_pixels_saved=False)
