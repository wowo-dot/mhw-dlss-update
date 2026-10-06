"""Validate bounded indexed observation receipts; never grant rendering admission."""
import math
from arm_continuous_dlaa import require

HASHES={
    'be174b5f4d68ae60081aa5ffb18e3ab244a481aeda176dadf44a2fddf280072d',
    '66b94be1558657b6230dca31b21bfda583699289d33f898f711dfc3980c27077',
    'edb7df787b52918e4c4fbaff351ea97ffae3ad686b6452b05d1f559d58b4f620',
    '750c2b00167fbd6e791dbface9b295d6ac9cbe3eb73648560f9e84786e964eec',
    '5f2e2363c0839c93982e20b9ffb57df1cf03c4a4ec898fa85583e154172e2ea1',
}
def integer(value):return type(value) is int and 0<=value<2**64
def words(value,n):
    require(type(value) is list and len(value)==n and all(integer(x) for x in value),'silhouette words')
def review(channel,previous=None,final=False):
    require(channel['schema']==1 and channel['mode']=='read-only-indexed-contract','silhouette schema')
    for key in ('visits','errors','candidate_rejections','sample_count'):
        require(integer(channel[key]),'silhouette counter '+key)
    require(channel['errors']==0 and channel['visits']<=4096,'silhouette recorder health')
    samples=channel['samples']
    require(type(samples) is list and len(samples)==channel['sample_count']<=16 and len(samples)<=channel['visits'],'silhouette sample bounds')
    for s in samples:
        for key in ('frame','epoch','tick','key','technique','calls','caller','state','rejection'):
            require(integer(s[key]),'silhouette integer '+key)
        require(0x7ffffff1<s['key']<0x80000000 and s['pixel_sha256'] in HASHES,'silhouette native identity')
        require(s['rejection'] in (0,1,2,3,99) and s['calls']<=1,'silhouette call receipt')
        for key in ('returned','stable','api_seen','root_ready','raster_ready'):
            require(type(s[key]) is bool,'silhouette boolean '+key)
        require(s['api_seen']==bool(s['calls']) and (not s['stable'] or s['returned'] and s['api_seen'] and not s['rejection']),'silhouette return receipt')
        if final:require(s['returned'] or s['rejection']==1,'silhouette callback not retired')
        for key,n in (('packet_arguments',3),('packet_bindings',4),('extra_stages',3),('binding_owners',12),('screen_words',28),('material_words',5),('color_state',2),('native_viewport',4),('native_scissor',4)):
            words(s[key],n)
        require(type(s['arguments']) is list and len(s['arguments'])==5,'silhouette API arguments')
        for i,x in enumerate(s['arguments']):require(type(x) is int and (-2**31<=x<2**31 if i==3 else 0<=x<2**32),'silhouette API range')
        require(type(s['descriptions']) is list and len(s['descriptions'])==8,'silhouette descriptions')
        for d in s['descriptions']:words(d,14);require(d[1] in (0,1),'silhouette resource observation')
        r=s['raster'];words(r['index'],3);require(len(r['vertices'])==4,'silhouette vertex streams')
        for v in r['vertices']:words(v,3)
        require(type(r['index_known']) is bool and type(r['output_known']) is bool,'silhouette raster flags')
        require(integer(r['vertex_mask']) and r['vertex_mask']<2**32,'silhouette vertex mask')
        require(len(r['viewport'])==6 and all(type(v) in (int,float) and math.isfinite(v) for v in r['viewport']),'silhouette viewport')
        if s['returned'] and not s['rejection']:
            a=s['arguments'];require(s['packet_arguments']==[a[0],a[2],a[3]&0xffffffff] and a[1]==1 and a[4]==0,'silhouette native/API arguments match')
    if previous:
        for key in ('visits','errors','candidate_rejections','sample_count'):require(channel[key]>=previous[key],'silhouette rollback')
        for old,new in zip(previous['samples'],samples):
            if old['returned']:require(old==new,'retired silhouette sample changed')
    return channel
