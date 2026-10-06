# Classify every event that touches one texture, inside RenderDoc's Python:
#   set MHW_RDC_CAPTURE=<file.rdc> MHW_RDC_OUT=<folder> MHW_RDC_RESOURCE=<id> [MHW_RDC_SAVE=eid,eid,...]
#   qrenderdoc.exe --python tools/rdc/target_timeline.py
# Writes timeline.json (+ PNGs of the resource after each listed event), then exits.
import os, json, traceback
import renderdoc as rd

CAPTURE = os.environ['MHW_RDC_CAPTURE']
OUT = os.environ['MHW_RDC_OUT']
TARGET = int(os.environ['MHW_RDC_RESOURCE'])
SAVE = [int(x) for x in os.environ.get('MHW_RDC_SAVE', '').split(',') if x]
os.makedirs(OUT, exist_ok=True)
report = dict(capture=CAPTURE, resource=TARGET, errors=[])


def ok(result):
    return result.OK() if hasattr(result, 'OK') else result == rd.ResultCode.Succeeded


def name_of(value):
    return str(value).split('.')[-1]


def main(controller):
    sf = controller.GetStructuredFile()
    rid = [r.resourceId for r in controller.GetResources() if int(r.resourceId) == TARGET][0]
    textures = {int(t.resourceId): t for t in controller.GetTextures()}
    actions = {}

    def walk(items):
        for a in items:
            actions[a.eventId] = a
            walk(a.children)
    walk(controller.GetRootActions())
    rows = []
    for use in controller.GetUsage(rid):
        row = dict(eid=use.eventId, usage=name_of(use.usage))
        a = actions.get(use.eventId)
        row['name'] = a.GetName(sf) if a else ''
        if a is not None and a.flags & rd.ActionFlags.Drawcall:
            try:
                controller.SetFrameEvent(use.eventId, True)
                pipe = controller.GetPipelineState()
                d12 = controller.GetD3D12PipelineState()
                b = pipe.GetColorBlends()[0]
                row.update(vertices=a.numIndices, instances=a.numInstances,
                           blend=[b.enabled, name_of(b.colorBlend.source), name_of(b.colorBlend.destination), b.writeMask],
                           depth=[d12.outputMerger.depthStencilState.depthEnable, d12.outputMerger.depthStencilState.depthWrites],
                           ps=int(pipe.GetShader(rd.ShaderStage.Pixel)), vs=int(pipe.GetShader(rd.ShaderStage.Vertex)),
                           outputs=[int(o) for o in a.outputs if o != rd.ResourceId.Null()],
                           dsv=int(a.depthOut) if a.depthOut != rd.ResourceId.Null() else 0)
                inputs = []
                for used in pipe.GetReadOnlyResources(rd.ShaderStage.Pixel):
                    desc = used.descriptor if hasattr(used, 'descriptor') else used
                    r = desc.resource if hasattr(desc, 'resource') else desc.resources[0].resourceId
                    if r != rd.ResourceId.Null():
                        t = textures.get(int(r))
                        inputs.append([int(r), t.width if t else 0, t.height if t else 0, t.format.Name() if t else ''])
                row['ps_inputs'] = inputs
            except Exception:
                report['errors'].append(dict(eid=use.eventId, error=traceback.format_exc(limit=2)))
        rows.append(row)
    report['timeline'] = rows
    t = textures.get(TARGET)
    report['texture'] = dict(w=t.width, h=t.height, format=t.format.Name()) if t else None
    report['saved'] = []
    for eid in SAVE:
        controller.SetFrameEvent(eid, True)
        ts = rd.TextureSave()
        ts.resourceId = rid
        ts.alpha = rd.AlphaMapping.Discard
        ts.destType = rd.FileType.PNG
        path = os.path.join(OUT, 'res%d_after_eid%d.png' % (TARGET, eid))
        report['saved'].append(dict(eid=eid, path=path, ok=ok(controller.SaveTexture(ts, path))))


try:
    rd.InitialiseReplay(rd.GlobalEnvironment(), [])
    cap = rd.OpenCaptureFile()
    if not ok(cap.OpenFile(CAPTURE, '', None)):
        raise RuntimeError('open file')
    result, controller = cap.OpenCapture(rd.ReplayOptions(), None)
    if not ok(result):
        raise RuntimeError('open capture')
    try:
        main(controller)
    finally:
        controller.Shutdown()
        cap.Shutdown()
except Exception:
    report['fatal'] = traceback.format_exc()
finally:
    with open(os.path.join(OUT, 'timeline_res%d.json' % TARGET), 'w', encoding='utf-8') as f:
        json.dump(report, f, indent=1)
    os._exit(0)
