"""Audit a separate scene-depth candidate; no depth pixels or typed-view claim."""
from temporal_shader import require
from temporal_consumer import derive_states


def review_scene_depth(capture, *, paired_jitter=False,sample_count=3):
    require(sample_count in (3,9),'scene sample limit')
    require(capture.get('typed_views_verified') is False, 'scene typed-view coverage')
    require(('projection_stimulus' in capture) is paired_jitter, 'scene projection experiment scope')
    details=[]
    for cpu,gpu in zip(capture['records'],capture['native_compute']['records']):
        d=cpu['scene_depth'];g=gpu['scene_depth']
        require(d.get('stable') is True and g.get('equal') is True, 'scene ownership stability')
        require(all(type(d.get(k)) is int and d[k]>0 for k in
                    ('container','owner','type','backing','resource','identity','width','height','engine_width','engine_height','mips','array_size','sample_count')), 'scene resource identity')
        require(d['backing']==(d['secondary'] or d['primary']) and d['dimension']==3 and d['width']<=16384 and d['height']<=16384, 'scene resource description')
        require(type(d.get('srv_readable')) is bool and type(d.get('attachment_readable')) is bool, 'scene optional observations')
        for key in ('srv','cpu_descriptor','attachment','attachment_owner','attachment_resource'):
            require(type(d.get(key)) is int and 0<=d[key]<2**64,'scene optional pointer')
        if d['srv_readable'] and not d['srv']:require(d['cpu_descriptor']==0,'absent native SRV')
        if d['attachment_readable']:
            require(all(d[k]>0 for k in ('attachment','attachment_owner','attachment_resource')), 'scene attachment identity')
        else:require(d['attachment_resource']==0,'unreadable attachment resource')
        require(g['resource']==d['resource'],'scene consumer resource')
        state=derive_states([d['resource']],gpu['barrier_calls'],gpu['barriers'],gpu['dispatch_enter'])[0]
        require(g['state']==state,'scene state derivation')
        color,motion,fallback=cpu['inputs'][0],cpu['inputs'][5],cpu['inputs'][2]
        shape=(d['format']==39 and d['flags']==2 and d['mips']==d['array_size']==d['sample_count']==1 and
               d['sample_quality']==d['layout']==0 and d['width']==color['width']==motion['width'] and
               d['height']==color['height']==motion['height'] and cpu['rectangle']==[0,0,d['width'],d['height']] and
               all(d['identity']!=v['identity'] for v in (color,motion,fallback)))
        details.append(dict(frame=cpu['frame'],resource=d['resource'],format=d['format'],width=d['width'],height=d['height'],
                            matches_prior_scene_depth_shape=shape,state=state,
                            native_srv_address_observed=d['srv_readable'] and d['srv']>0 and d['cpu_descriptor']>0,
                            current_attachment_matches=d['attachment_readable'] and d['attachment_owner']==d['owner'] and d['attachment_resource']==d['resource']))
    require(len(details)==sample_count and all(details[i]['frame']+1==details[i+1]['frame'] for i in range(sample_count-1)), 'scene consecutive frames')
    return dict(scene_depth_request_association_verified=True,scene_depth_observations=details,
                scene_depth_matches_prior_shape=all(d['matches_prior_scene_depth_shape'] for d in details),
                scene_depth_recording_state_known=all(d['state']['known'] and not d['state']['conflict'] for d in details),
                scene_depth_pixels_saved=False,scene_depth_content_alignment_verified=False,scene_depth_copy_ready=False,
                typed_views_verified=False,dlaa_ready=False)
