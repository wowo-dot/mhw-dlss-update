"""Replay native depth-copy provenance; no depth-pixel or final-writer claim."""
from temporal_limits import frame_count
from temporal_scaled_geometry import depth_operation,depth_draw_states
from temporal_shader import require
from temporal_consumer import derive_states

STAMP=('context','view','container','frame','epoch','bank','async')

def review_marker_coverage(header,capture,depth_markers):
    """Every native-work marker has exactly one verified owner."""
    output_markers=set()
    if header.get('full_output_route') is True:
        # The scaled route uses the same journal marker kind for its copy and
        # three draws. Validate their entire contract before assigning them;
        # arbitrary extra markers or overlap with depth are still rejected.
        from temporal_scaled_output import header_contract,route_contract
        header_contract(header)
        route=route_contract(capture)
        output_markers={route['copy_event'],*route['draw_events']}
        require(not depth_markers.intersection(output_markers),'depth/output marker overlap')
    events=capture['submission']['events']
    require({i for i,e in enumerate(events) if e['kind']==4}==depth_markers|output_markers,'depth marker coverage')


def generation(events, marker):
    """Resolve a recording through its next Reset, rejecting reuse/resubmit."""
    command=marker['object']
    touched=[e for e in events if (command in e['lists'] if e['kind']==2 else e['object']==command)]
    before=[e for e in touched if e['kind']==0 and e['exit']<marker['enter']]
    after=[e for e in touched if e['kind']==0 and e['enter']>marker['exit']]
    require(before and after,'depth copy reset boundaries')
    reset,retire=before[-1],after[0]
    scope=[e for e in touched if e['exit']>reset['enter'] and e['enter']<retire['exit']]
    require(all(a['exit']<b['enter'] for a,b in zip(scope,scope[1:])),'depth overlapping list operations')
    require([e for e in scope if e['kind']==0]==[reset,retire],'depth intervening reset')
    closes=[e for e in scope if e['kind']==1];executes=[e for e in scope if e['kind']==2]
    require(len(closes)==len(executes)==1,'depth close/execute multiplicity')
    close,execute=closes[0],executes[0]
    require(reset['exit']<marker['enter']<marker['exit']<close['enter']<close['exit']<execute['enter']<execute['exit']<retire['enter'], 'depth generation order')
    require(execute['lists'].count(command)==1,'depth batch multiplicity')
    return dict(reset=reset['enter'],close=close['enter'],execute=execute['enter'],retire=retire['enter'],
                queue=execute['object'],batch_index=execute['lists'].index(command),marker=marker['enter'])

def generation_or_incomplete_tail(events,marker,frame,last_selected_frame,last_selected_dispatch):
    """Explicit supplemental scope; never waive a selected-frame witness."""
    try:
        return dict(generation(events,marker),qualified=True)
    except ValueError as error:
        require(str(error)=='depth copy reset boundaries','depth tail is not a missing boundary')
    require(frame>last_selected_frame and marker['enter']>last_selected_dispatch,'depth selected generation incomplete')
    command=marker['object']
    touched=[e for e in events if (command in e['lists'] if e['kind']==2 else e['object']==command)]
    resets=[e for e in touched if e['kind']==0 and e['exit']<marker['enter']]
    require(resets,'depth tail missing initial reset')
    reset=resets[-1];scope=[e for e in touched if e['exit']>reset['enter']]
    require(all(a['exit']<b['enter'] for a,b in zip(scope,scope[1:])),'depth tail overlapping operations')
    require([e for e in scope if e['kind']==0]==[reset],'depth tail has another reset')
    require(not any(e['kind']==2 for e in scope),'depth tail has execution without retirement')
    closes=[e for e in scope if e['kind']==1]
    require(len(closes)<=1 and (not closes or closes[0]['enter']>marker['exit']),'depth tail invalid close')
    return dict(qualified=False,reset=reset['enter'],marker=marker['enter'],command=command,
                reason='capture ended before observed Execute and retirement')

def review_depth_routes(header,capture,*,selected_scope=False):
    n=frame_count(header)
    t=capture['depth_routes'];events=capture['submission']['events']
    require(t['started_tick']==capture['started_tick'],'depth route clock')
    require(t['fault']==t['pending']==0 and t['hooks_disabled'] is True,'depth route closure')
    requests,consumers,selections=t['requests'],t['consumers'],t['selections']
    require(0<len(requests)<=64 and 0<len(consumers)<=64 and len(selections)==n,'depth route counts')
    if header['version'] in ('1.23.6','1.23.7','1.23.8','1.23.9','1.23.10','1.23.11','1.23.12','1.24.0','1.24.1','1.24.2','1.24.3','1.24.4','1.24.5'):
        gate=t.get('projection_gate',{});projection=capture['projection_stimulus']['records'][0]
        require(header.get('depth_projection_gate') is True and gate.get('required') is True and gate.get('opened') is True and
                type(gate.get('waits')) is int and gate['waits']>=0,'depth projection gate admission')
        require(all(gate.get(k)==requests[0][k] for k in STAMP) and
                all(gate.get(k)==projection[k] for k in STAMP if k!='container'),'depth projection gate association')
    require(t['pins']==t['released']==3*len(consumers),'depth route references')
    require(type(t['unmatched_consumers']) is int and t['unmatched_consumers']>=0,'depth unmatched count')
    orders=set()
    def remember(*values):
        require(all(type(v) is int and v>0 for v in values) and len(set(values))==len(values) and not orders.intersection(values),'depth route order collision')
        orders.update(values)
    def owner(v):
        require(all(type(v[k]) is int and 0<v[k]<2**64 for k in ('owner','type','backing','resource')),'depth owner identity')
        require(all(type(v[k]) is int and 0<=v[k]<2**64 for k in ('primary','secondary')) and v['backing']==(v['secondary'] or v['primary']),'depth backing')
    previous=0
    for q in requests:
        require(q['valid'] is True and q['returned'] is True and q['thread']>0 and q['caller']>0,'depth producer result')
        require(previous<q['enter']<q['exit'],'depth producer order');previous=q['exit'];remember(q['enter'],q['exit'])
        require(q['context']==capture['renderer']+0x710 and q['view']>0 and q['container']>0 and q['frame']>0 and q['epoch']>=0 and q['bank'] in (0,1) and q['async'] in (0,1),'depth producer stamp')
        require(0<=q['before_count']<q['after_count']<=65536 and q['after_count']-q['before_count']<=64 and q['before_array']>0 and q['after_array']>0,'depth publication bounds')
        require(q['packet']>0 and q['opcode']==10 and q['kind']==8 and 0<=q['nested']<2**64,'depth copy packet')
        require(len(q['rectangle'])==6 and all(type(x) is int and 0<=x<=0xffffffff for x in q['rectangle']),'depth rectangle')
        x,y,right,bottom,dx,dy=q['rectangle'];require(x<right and y<bottom,'depth empty rectangle')
        owner(q['source']);owner(q['destination'])
        require(all(q['source'][k]!=q['destination'][k] for k in ('owner','resource')),'depth source/destination alias')
    routes=[];seen_receipts=set();seen_markers=set()
    for r in consumers:
        index=r['receipt'];require(type(index) is int and 0<=index<len(requests) and index not in seen_receipts,'depth consumer receipt');seen_receipts.add(index)
        q=requests[index];draw=depth_operation(header,r,q)
        if header['version'] in ('1.15.2','1.15.3','1.16.0','1.17.0','1.23.0','1.23.1','1.23.2','1.23.3','1.23.4','1.23.5','1.23.6','1.23.7','1.23.8','1.23.9','1.23.10','1.23.11','1.23.12','1.24.0','1.24.1','1.24.2','1.24.3','1.24.4','1.24.5'):
            require(type(r.get('marker_failure')) is int and r['marker_failure']==0,'depth marker failure')
        require(r['packet']==q['packet'],'depth consumer packet association')
        require(r['valid'] is True and r['returned'] is True and r['copy_returned'] is True and r['native_calls']==1,'depth native result')
        require(all(type(r[k]) is int and r[k]>0 for k in ('executor','context','recording','command','identity','thread')),'depth consumer identity')
        require(q['exit']<r['enter']<r['copy']<r['copy_exit']<r['exit'] and r['copy_caller']==header['base']+(0x259d72e if draw else 0x259e100),'depth native copy order')
        remember(r['enter'],r['copy'],r['copy_exit'],r['exit'])
        # Reused packet storage must refer to the newest *published* receipt.
        newer=[j for j,p in enumerate(requests) if p['packet']==q['packet'] and p['exit']<r['enter']]
        require(newer and newer[-1]==index,'depth stale packet generation')
        for resource in (r['source'],r['destination']):
            require(resource['identity']>0 and resource['dimension']==3 and 0<resource['width']<=16384 and 0<resource['height']<=16384 and resource['mips']==resource['array_size']==resource['sample_count']==1 and resource['sample_quality']==0,'depth native resource description')
        require(r['source']['identity']!=r['destination']['identity'],'depth canonical alias')
        require(all(r['source'][k]==r['destination'][k] for k in ('width','height','mips','array_size','sample_count','sample_quality')),'depth full-copy dimensions')
        require(r['source']['format']==39 and r['source']['flags']==2 and r['destination']['format']==41 and r['destination']['flags']==1,'depth expected source/candidate shape')
        if not draw:require(q['rectangle']==[0,0,r['source']['width'],r['source']['height'],0,0],'depth full-copy rectangle')
        calls,barriers=r['barrier_calls'],r['barriers'];require(len(calls)<=8 and len(barriers)<=32,'depth barrier budgets')
        offset=0;previous=r['enter']
        for b in calls:
            require(b['first']==offset and type(b['count']) is int and 0<=b['count']<=32-offset,'depth barrier slices');offset+=b['count']
            require(b['returned'] is True and b['stable'] is True and b['caller']>0,'depth barrier result')
            require(previous<b['enter']<b['exit']<r['exit'] and (b['exit']<r['copy'] or b['enter']>r['copy_exit']),'depth barrier order');previous=b['exit'];remember(b['enter'],b['exit'])
        require(offset==len(barriers),'depth barrier storage')
        for b in barriers:
            require(type(b['type']) is int and b['type'] in (0,1,2) and 0<=b['flags']<=3,'depth barrier kind')
            require(all(type(b[k]) is int and 0<=b[k]<=0xffffffff for k in ('subresource','before','after')),'depth barrier fields')
            require(all(type(b[k]) is int and 0<=b[k]<2**64 for k in ('resource','other')),'depth barrier pointers')
        states=derive_states([q['source']['resource'],q['destination']['resource']],calls,barriers,r['copy'])
        known=all(s['known'] and not s['conflict'] for s in states)
        legal=known and bool(states[0]['value']&0x800) and not (states[0]['value']&0x14) and states[1]['value']==0x400
        if draw:legal=depth_draw_states(header,states)
        marker=r['marker_index'];require(type(marker) is int and 0<=marker<len(events) and marker not in seen_markers,'depth marker index');seen_markers.add(marker)
        event=events[marker];require(event['kind']==4 and event['object']==r['command'] and event['thread']==r['thread'],'depth submission marker')
        registry={v['object']:v['identity'] for v in capture['submission']['lists']}
        require(registry.get(r['command'])==r['identity'],'depth list canonical identity')
        if selected_scope:
            route=generation_or_incomplete_tail(events,event,q['frame'],max(s['frame'] for s in selections),
                                               max(s['dispatch'] for s in capture['submission']['samples']))
        else:route=dict(generation(events,event),qualified=True)
        route.update(receipt=index,operation="draw" if draw else "copy",frame=q['frame'],states=states,copy_states_verified=legal,source=q['source']['resource'],destination=q['destination']['resource'])
        routes.append(route)
    review_marker_coverage(header,capture,seen_markers)
    frames=[]
    for s,cpu,sample in zip(selections,capture['records'],capture['submission']['samples']):
        d=cpu['scene_depth'];remember(s['order'])
        require(all(s[k]==(d[k] if k=='container' else cpu[k]) for k in STAMP),'depth selection stamp')
        require(s['owner']==d['owner'] and s['resource']==d['resource'] and s['packet']==cpu['packet'],'depth selection identity')
        matched=[]
        taa=next(e for e in events if e['enter']==sample['execute'])
        for r,route in zip(consumers,routes):
            q=requests[r['receipt']]
            if q['frame']!=s['frame']:continue
            require(all(q[k]==s[k] for k in STAMP),'depth selected frame stamp')
            require(q['destination']['owner']==s['owner'] and q['destination']['resource']==s['resource'],'depth request/selection association')
            require(all(r['destination'][k]==d[k] for k in r['destination']),'depth selected resource description')
            # Array index is reported as submission order only. Execute may
            # overlap work within a batch: this is not a resource-ready proof.
            relation='different-queue'
            if route['queue']==sample['queue']:
                if route['execute']!=sample['execute']:relation='earlier-execute' if route['execute']<sample['execute'] else 'later-execute'
                elif r['command']==sample['command']:relation='same-list-before-taa' if route['marker']<sample['dispatch'] else 'same-list-after-taa'
                else:relation='earlier-list-in-batch' if route['batch_index']<taa['lists'].index(sample['command']) else 'later-list-in-batch'
            matched.append(dict(route,relative_to_taa=relation,produced_before_taa=q['exit']<s['order']))
        require(matched and any(r['produced_before_taa'] for r in matched) and all(r['copy_states_verified'] for r in matched),'depth selected copy/state association')
        frames.append(dict(frame=s['frame'],copies=matched))
    result=dict(scene_depth_native_copy_association_verified=True,scene_depth_native_copy_states_verified=True,
                scene_depth_native_copy_frames=frames,scene_depth_native_copy_count=len(routes),
                scene_depth_qualified_copy_count=sum(r['qualified'] for r in routes),
                scene_depth_incomplete_tail=[r for r in routes if not r['qualified']],
                scene_depth_unconsumed_requests=len(requests)-len(seen_receipts),
                scene_depth_unmatched_consumers=t['unmatched_consumers'],scene_depth_final_writer_verified=False,
                scene_depth_content_alignment_verified=False,scene_depth_pixels_saved=False,scene_depth_copy_ready=False,
                typed_views_verified=False,dlaa_ready=False)
    if any(r['operation']=='draw' for r in routes):
        for key in list(result):
            if 'native_copy' in key:result[key.replace('native_copy','native_writer')]=result.pop(key)
        result['scene_depth_draw_count']=sum(r['operation']=='draw' for r in routes)
    return result
