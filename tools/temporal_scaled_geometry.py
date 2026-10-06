"""Separate active temporal rectangles from backing allocation dimensions."""
from temporal_shader import require

def depth_draw_states(header,states):
    allowed=(0x80,0xc0) if header['version'] in ('1.23.2','1.23.3','1.23.4','1.23.5','1.23.6','1.23.7','1.23.8','1.23.9','1.23.10','1.23.11','1.23.12','1.24.0','1.24.1','1.24.2','1.24.3','1.24.4','1.24.5') else (0xc0,)
    return any(states==[dict(known=True,conflict=False,value=source),
                       dict(known=True,conflict=False,value=4)] for source in allowed)


def active_rectangle(header,cpu,width,height):
    if header['version'] not in ('1.23.1','1.23.2','1.23.3','1.23.4','1.23.5','1.23.6','1.23.7','1.23.8','1.23.9','1.23.10','1.23.11','1.23.12','1.24.0','1.24.1','1.24.2','1.24.3','1.24.4','1.24.5'):return [0,0,width,height]
    s=cpu['scale_state']
    require(s['stable'] is True and s['frame']==cpu['frame'] and s['view']==cpu['view'],'scaled frame/view')
    require(s['active']==.5 and s['content']==1 and s['display']==[width,height] and
            s['render']==[width//2,height//2],'scaled allocation/active extent')
    return [0,0,*s['render']]

def depth_operation(header,consumer,request):
    operation=consumer.get('operation','copy')
    if operation=='copy':return False
    require(header['version'] in ('1.23.1','1.23.2','1.23.3','1.23.4','1.23.5','1.23.6','1.23.7','1.23.8','1.23.9','1.23.10','1.23.11','1.23.12','1.24.0','1.24.1','1.24.2','1.24.3','1.24.4','1.24.5') and header.get('depth_draw_capture') is True and operation=='draw','depth draw version')
    d=consumer['draw'];w=consumer['destination']['width'];h=consumer['destination']['height']
    require(d['stable'] is True and d['nested']==d['recorded_packet']==request['nested']>0 and d['pipeline']>0,'depth draw stability')
    require(len(d['bindings'])==4 and all(type(x) is int and 0<=x<2**64 for x in d['bindings']) and
            d['bindings'][0]==d['rectangle_owner']>0,'depth draw bindings')
    require(d['rectangle']==request['rectangle'][:4]==[0,0,w//2,h//2] and request['rectangle'][4:]==[0,0],'depth draw rectangle')
    a=d['arguments'];require(len(a)==4 and all(type(x) is int and 0<=x<=0xffffffff for x in a) and
        0<a[0]<=6 and a[1]==1 and a[3]==0 and d['packet_arguments']==[a[0],a[2]],'depth draw arguments')
    return True
