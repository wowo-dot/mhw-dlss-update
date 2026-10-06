"""Continuous static Performance DLSS route telemetry; no image-quality claim."""
from arm_continuous_dlaa import require
from continuous_overlays import review as review_overlays
from continuous_binding_checks import review as review_binding_checks
COUNTERS=('published','built','unqualified','claimed','copies','draws','complete','warmup_draws','retained','released','live','peak','pending','fault')
def review(s,previous=None,final=False):
    r=s['scaled_route'];scale=s['resolution'];p=s['sdk']
    if 'silhouette_output' in r:
        from continuous_silhouette_output import review as review_output
        review_output(r['silhouette_output'],previous.get('silhouette_output') if previous else None,final)
    if 'silhouette_probe' in r:
        from continuous_silhouette import review as review_silhouette
        review_silhouette(r['silhouette_probe'],previous.get('silhouette_probe') if previous else None,final)
    if 'dof_native_sequence' in r:
        from continuous_dof_native import review as review_dof
        review_dof(r['dof_native_sequence'],previous.get('dof_native_sequence') if previous else None,final)
    if 'compute_depth_pixels' in r:
        from continuous_compute_depth_pixels import review as review_depth_pixels
        review_depth_pixels(r['compute_depth_pixels'],r['compute_resources'],previous.get('compute_depth_pixels') if previous else None,final)
    if 'compute_resources' in r:
        from continuous_compute_resources import review as review_resources
        review_resources(r['compute_resources'],r['compute_dependencies'],previous.get('compute_resources') if previous else None,final)
    if 'dependency_binding_checks' in r:review_binding_checks(r['dependency_binding_checks'],previous.get('dependency_binding_checks') if previous else None)
    if 'dependency_refresh' in r:
        from continuous_dependency_refresh import review as review_refresh
        review_refresh(r['dependency_refresh'],previous.get('dependency_refresh') if previous else None)
    require(r.get('schema') in (1,2,3) and r.get('fault')==0 and r.get('failure')=='','scaled route health')
    for k in COUNTERS: require(type(r.get(k)) is int and 0<=r[k]<2**64-1,'scaled counter '+k)
    prepared=r['schema']==3
    if prepared:
        fields=('prepared_frames','spatial_frames','native_frames','early_draws','multilist_frames','submitted_frames','unresolved_frames','unsubmitted_frames')
        require(all(type(r.get(k)) is int and 0<=r[k]<2**64-1 for k in fields),'prepared frame counters')
        chosen=s['frames']+r['spatial_frames']+r['native_frames']
        require(r.get('selection')=='immutable-frame-predicate-v1' and r['prepared_frames']==r['claimed'],'prepared frame policy')
        require(r['complete']<=chosen<=r['claimed']<=r['complete']+32 and r['claimed']-chosen==r['unresolved_frames'],'prepared choice coverage')
        require(r['submitted_frames']<=r['complete'] and r['claimed']-r['submitted_frames']==r['unsubmitted_frames']<=32,'prepared submission coverage')
        require(r['multilist_frames']<=r['complete'] and r['early_draws']<=r['draws'],'prepared worker coverage')
        if previous:require(all(r[k]>=previous[k] for k in fields if k not in ('unresolved_frames','unsubmitted_frames')),'prepared counter rollback')
    else:require(r['complete']<=s['frames']<=r['claimed']<=r['complete']+1,'one in-flight output chain')
    hc=hd=ha=0
    if r['schema']>=2:
        ha,hc,hd=(r.get(k) for k in ('haze_claimed','haze_complete','haze_draws'))
        require(all(type(x) is int and 0<=x<2**64-1 for x in (ha,hc,hd)), 'haze counters')
        require(hc<=hd<=ha<=hc+(32 if prepared else 1) and hc<=r['complete'] and ha<=r['claimed'] and ha-hc<=r['claimed']-r['complete'], 'haze chain coverage')
        require(type(r.get('haze_admission_rejection')) is int and 0<=r['haze_admission_rejection']<=6, 'haze admission reason')
        if previous:require(all(r[k]>=previous[k] for k in ('haze_claimed','haze_complete','haze_draws')), 'haze counter rollback')
    oa=oc=0
    if 'overlays' in r or 'recording_states' in r:
        require(prepared,'overlay prepared-frame ownership')
        review_overlays(r,previous,final)
        oa=r['overlays']['prepared'];oc=r['overlays']['completed']
    require(r['complete']<=r['copies']<=r['claimed'] and 3*r['complete']+hc+oc<=r['draws']<=3*r['claimed']+ha+oa,'output chain coverage')
    require(r['claimed']<=r['built']<=r['published'],'published plan coverage')
    if 'final_captured' in r:
        require(type(r['final_captured']) is int and type(r.get('final_rejected')) is int and 0<=r['final_rejected']<2**64-1 and r['built']<=r['final_captured']<2**64-1,'final quad capture coverage')
        if previous and 'final_captured' in previous:
            require(all(r[k]>=previous[k] for k in ('final_captured','final_rejected')),'final quad counter rollback')
    require((r['retained']<=8*r['claimed'] if prepared else r['retained']==r['claimed']) and r['retained']==r['released']+r['live'] and r['live']<=r['peak']<=128,'bounded route references')
    require((s['render_width']*2,s['render_height']*2)==(s['width'],s['height']),'static half-resolution geometry')
    require(scale['fault']==0 and scale['shared_viewport'] is True and scale['render']==[s['render_width'],s['render_height']] and scale['display']==[s['width'],s['height']],'native resolution contract')
    require(p['mode']==0 and p['preset']==11 and p['captures']==0,'streaming Performance SDK mode')
    require(p['geometry_schema']==1 and p['allocation']==[s['width'],s['height']] and p['output_extent']==p['allocation'] and p['render']==[s['render_width'],s['render_height']] and p['optimal']==p['render'] and p['input_origin']==[0,0] and p['motion_units']==p['jitter_units']=='render_pixels','SDK render/output geometry')
    if previous:require(all(r[k]>=previous[k] for k in COUNTERS if k not in ('live','pending','fault')),'route counter rollback')
    if final:
        require(r['claimed']==r['copies']==r['complete']==(chosen if prepared else s['frames']) and r['draws']==3*r['complete']+hc+oc and ha==hc==hd,'every prepared frame completes its admitted effects')
        if prepared:require(r['submitted_frames']==r['complete'] and r['unresolved_frames']==r['unsubmitted_frames']==0,'all prepared frames submitted')
        require(r['warmup_draws']>=3 and r['pending']==0 and r['active'] is False and r['hooks_disabled'] is True,'route stopped')
        require(scale['requested'] is False and scale['restored'] is True and scale['hooks_disabled'] is True and scale['pending']==0,'full render scale restored')
    return dict(r)
