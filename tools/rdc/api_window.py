# API calls (structured chunk names and, for root/list calls, their arguments)
# for an EID window, inside RenderDoc's embedded Python:
#   set MHW_RDC_CAPTURE=<file.rdc>  MHW_RDC_OUT=<folder>  MHW_RDC_SAVE=<first>,<last>
#   qrenderdoc.exe --python tools/rdc/api_window.py   (or tools/rdc/run.py)
# Writes api_window.json: one row per event (eid, chunk name, selected
# arguments) so command-list boundaries and binding order can be read offline.
import os, json, traceback
import renderdoc as rd

CAPTURE = os.environ['MHW_RDC_CAPTURE']
OUT = os.environ['MHW_RDC_OUT']
FIRST, LAST = [int(x) for x in os.environ['MHW_RDC_SAVE'].split(',')]
os.makedirs(OUT, exist_ok=True)
report = dict(capture=CAPTURE, window=[FIRST, LAST], errors=[], rows=[])
KEEP = ('RootParameterIndex', 'BufferLocation', 'NumCommandLists', 'pRootSignature', 'pPipelineState',
        'VertexCountPerInstance', 'InstanceCount', 'NumRenderTargetDescriptors', 'pDst', 'pSrc',
        'pDstResource', 'pSrcResource', 'DstBuffer', 'SrcBuffer')


def ok(result):
    return result.OK() if hasattr(result, 'OK') else result == rd.ResultCode.Succeeded


def simple(obj):
    """Scalar or short summary of a structured object."""
    try:
        if obj.NumChildren() == 0:
            data = obj.data
            basic = data.basic
            t = obj.type.basetype
            if t == rd.SDBasic.Resource:
                return 'res' + str(int(basic.id))
            if t in (rd.SDBasic.UnsignedInteger, rd.SDBasic.Enum):
                return int(basic.u)
            if t == rd.SDBasic.SignedInteger:
                return int(basic.i)
            if t == rd.SDBasic.Float:
                return float(basic.d)
            if t == rd.SDBasic.String:
                return str(data.str)
            return str(t)
        return {obj.GetChild(i).name: simple(obj.GetChild(i)) for i in range(min(obj.NumChildren(), 8))}
    except Exception:
        return '?'


def walk(actions, out):
    for a in actions:
        out.append(a)
        walk(a.children, out)


def main(controller):
    sf = controller.GetStructuredFile()
    flat = []
    walk(controller.GetRootActions(), flat)
    seen = set()
    for a in flat:
        for ev in a.events:
            if ev.eventId < FIRST or ev.eventId > LAST or ev.eventId in seen:
                continue
            seen.add(ev.eventId)
            chunk = sf.chunks[ev.chunkIndex]
            row = dict(eid=ev.eventId, call=chunk.name)
            for i in range(chunk.NumChildren()):
                c = chunk.GetChild(i)
                if c.name in KEEP:
                    row[c.name] = simple(c)
            if ev.eventId == a.eventId:
                row['action'] = a.GetName(sf)
                row['flags'] = int(a.flags)
            report['rows'].append(row)
    report['rows'].sort(key=lambda r: r['eid'])


try:
    rd.InitialiseReplay(rd.GlobalEnvironment(), [])
    cap = rd.OpenCaptureFile()
    result = cap.OpenFile(CAPTURE, '', None)
    if not ok(result):
        raise RuntimeError('open capture: %s' % result)
    status, controller = cap.OpenCapture(rd.ReplayOptions(), None)
    if not ok(status):
        raise RuntimeError('open replay: %s' % status)
    try:
        main(controller)
    finally:
        controller.Shutdown()
        cap.Shutdown()
except Exception:
    report['errors'].append(traceback.format_exc())
finally:
    with open(os.path.join(OUT, 'api_window.json'), 'w') as f:
        json.dump(report, f, indent=0)
    os._exit(0)
