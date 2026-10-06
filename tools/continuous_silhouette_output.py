"""Review correction/ownership receipts; pixel alignment is a separate check."""
from arm_continuous_dlaa import require

COUNTERS=('planned','recorded','native','corrected','restored','pipelines')
def review(channel,previous=None,final=False):
    require(channel.get('schema')==1 and channel.get('policy')=='indexed-native-coordinate-adapter-v1','silhouette output policy')
    for key in COUNTERS:
        require(type(channel.get(key)) is int and 0<=channel[key]<2**64-1,'silhouette output counter '+key)
    require(channel['native']+channel['corrected']==channel['recorded']<=channel['planned'],'silhouette output coverage')
    require(channel['restored']==channel['corrected'] and channel['pipelines']<=32,'silhouette state/cache bounds')
    rejections=channel.get('rejections')
    require(type(rejections) is list and len(rejections)==12 and all(type(x) is int and 0<=x<2**64-1 for x in rejections),'silhouette rejection receipts')
    require(sum(rejections)==channel['native'],'silhouette native branch accounting')
    if previous:
        require(all(channel[k]>=previous[k] for k in COUNTERS),'silhouette output rollback')
        require(all(x>=y for x,y in zip(rejections,previous['rejections'])),'silhouette rejection rollback')
    if final:require(channel['planned']==channel['recorded'],'all late indexed nodes recorded')
    return dict(channel)
