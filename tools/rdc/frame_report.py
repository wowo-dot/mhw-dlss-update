# Headless frame analysis inside RenderDoc's embedded Python:
#   set MHW_RDC_CAPTURE=<file.rdc>  MHW_RDC_OUT=<folder>
#   qrenderdoc.exe --python tools/rdc/frame_report.py
# Writes report.json, actions.txt and PNG dumps to MHW_RDC_OUT, then exits.
# RenderDoc's interpreter has no __file__; all paths come from the environment.
import os, json, traceback
import renderdoc as rd

CAPTURE = os.environ['MHW_RDC_CAPTURE']
OUT = os.environ['MHW_RDC_OUT']
os.makedirs(OUT, exist_ok=True)
report = dict(capture=CAPTURE, errors=[])


def ok(result):
    if hasattr(result, 'OK'):
        return result.OK()
    return result == rd.ResultCode.Succeeded


def safe(label, fn, default=None):
    try:
        return fn()
    except Exception:
        report['errors'].append(dict(where=label, error=traceback.format_exc(limit=3)))
        return default


def flags(value, enum):
    return str(enum(value)) if not isinstance(value, enum) else str(value)


def main(controller):
    sf = controller.GetStructuredFile()
    textures = {t.resourceId: t for t in controller.GetTextures()}
    names = {r.resourceId: r.name for r in controller.GetResources()}

    def tex(rid):
        t = textures.get(rid)
        if t is None:
            return dict(id=str(int(rid)), name=names.get(rid, ''), texture=False)
        return dict(id=str(int(rid)), name=names.get(rid, ''), w=t.width, h=t.height, mips=t.mips, arraysize=t.arraysize,
                    format=t.format.Name(), category=flags(t.creationFlags, rd.TextureCategory))

    actions = []

    def walk(items, depth):
        for a in items:
            actions.append((a, depth))
            walk(a.children, depth + 1)
    walk(controller.GetRootActions(), 0)
    with open(os.path.join(OUT, 'actions.txt'), 'w', encoding='utf-8') as f:
        for a, depth in actions:
            outs = [str(int(o)) for o in a.outputs if o != rd.ResourceId.Null()]
            f.write('%7d %s%s outs=%s depth=%s copy=%s->%s\n' % (
                a.eventId, '  ' * depth, a.GetName(sf), ','.join(outs),
                int(a.depthOut) if a.depthOut != rd.ResourceId.Null() else 0,
                int(a.copySource) if a.copySource != rd.ResourceId.Null() else 0,
                int(a.copyDestination) if a.copyDestination != rd.ResourceId.Null() else 0))
    report['action_count'] = len(actions)
    swap = [rid for rid, t in textures.items() if t.creationFlags & rd.TextureCategory.SwapBuffer]
    report['swap_buffers'] = [tex(r) for r in swap]
    presents = [a for a, _ in actions if a.flags & rd.ActionFlags.Present]
    report['presents'] = [dict(eid=a.eventId, name=a.GetName(sf), dest=str(int(a.copyDestination))) for a in presents]

    def usage(rid):
        return [dict(eid=u.eventId, usage=str(u.usage).split('.')[-1]) for u in controller.GetUsage(rid)]
    report['swap_usage'] = {str(int(r)): safe('usage', lambda r=r: usage(r), []) for r in swap}

    write_kinds = ('ColorTarget', 'CopyDst', 'Clear', 'ResolveDst', 'GenMips', 'CS_RWResource', 'PS_RWResource', 'CopyDst')
    by_eid = {a.eventId: a for a, _ in actions}

    def describe_event(eid, label):
        controller.SetFrameEvent(eid, True)
        pipe = controller.GetPipelineState()
        d12 = controller.GetD3D12PipelineState()
        a = by_eid.get(eid)
        info = dict(label=label, eid=eid, name=a.GetName(sf) if a else '')
        info['outputs'] = safe('outputs', lambda: [tex(o.resource if hasattr(o, 'resource') else o.resourceId)
                                                   for o in pipe.GetOutputTargets()
                                                   if (o.resource if hasattr(o, 'resource') else o.resourceId) != rd.ResourceId.Null()], [])
        def inputs():
            result = []
            for used in pipe.GetReadOnlyResources(rd.ShaderStage.Pixel):
                desc = used.descriptor if hasattr(used, 'descriptor') else used
                rid = desc.resource if hasattr(desc, 'resource') else desc.resources[0].resourceId
                if rid != rd.ResourceId.Null():
                    result.append(tex(rid))
            return result
        info['ps_inputs'] = safe('inputs', inputs, [])
        info['blend'] = safe('blend', lambda: [dict(enabled=b.enabled, src=str(b.colorBlend.source).split('.')[-1],
                                                     dst=str(b.colorBlend.destination).split('.')[-1],
                                                     op=str(b.colorBlend.operation).split('.')[-1],
                                                     asrc=str(b.alphaBlend.source).split('.')[-1],
                                                     adst=str(b.alphaBlend.destination).split('.')[-1],
                                                     mask=b.writeMask) for b in pipe.GetColorBlends()[:1]], [])
        info['depth'] = safe('depth', lambda: dict(enable=d12.outputMerger.depthStencilState.depthEnable,
                                                   write=d12.outputMerger.depthStencilState.depthWrites,
                                                   func=str(d12.outputMerger.depthStencilState.depthFunction).split('.')[-1]), {})
        def viewport():
            v = pipe.GetViewport(0)
            return dict(x=v.x, y=v.y, w=v.width, h=v.height)
        info['viewport'] = safe('viewport', viewport, {})
        def shader(stage):
            refl = pipe.GetShaderReflection(stage)
            if refl is None:
                return None
            targets = controller.GetDisassemblyTargets(True)
            text = controller.DisassembleShader(pipe.GetGraphicsPipelineObject(), refl, targets[0])
            return dict(id=str(int(pipe.GetShader(stage))), entry=refl.entryPoint, target=targets[0], text=text)
        for stage, key in ((rd.ShaderStage.Pixel, 'ps'), (rd.ShaderStage.Vertex, 'vs')):
            s = safe('shader-' + key, lambda stage=stage: shader(stage))
            if s:
                with open(os.path.join(OUT, 'eid%d_%s.txt' % (eid, key)), 'w', encoding='utf-8') as f:
                    f.write(s.pop('text'))
                info[key] = s
        return info

    def save(rid, eid, label):
        controller.SetFrameEvent(eid, True)
        ts = rd.TextureSave()
        ts.resourceId = rid
        ts.alpha = rd.AlphaMapping.Discard
        ts.mip = 0
        ts.slice.sliceIndex = 0
        ts.destType = rd.FileType.PNG
        path = os.path.join(OUT, '%s_eid%d_res%d.png' % (label, eid, int(rid)))
        result = controller.SaveTexture(ts, path)
        return dict(label=label, eid=eid, resource=str(int(rid)), path=path, ok=ok(result) if result is not None else None)

    report['events'] = []
    report['images'] = []
    report['writers'] = {}
    # The last write to each presented swap buffer before the last Present.
    last_present = presents[-1].eventId if presents else max(by_eid)
    for r in swap:
        writes = [u for u in report['swap_usage'][str(int(r))] if u['usage'] in write_kinds and u['eid'] <= last_present]
        if not writes:
            continue
        final = writes[-1]['eid']
        info = describe_event(final, 'backbuffer-write')
        report['events'].append(info)
        report['images'].append(safe('save', lambda r=r, final=final: save(r, final, 'backbuffer')))
        for inp in info.get('ps_inputs', []):
            rid = [k for k in textures if str(int(k)) == inp['id']]
            if not rid:
                continue
            rid = rid[0]
            uses = safe('usage-input', lambda rid=rid: usage(rid), [])
            report['writers'][inp['id']] = [u for u in uses if u['eid'] <= final]
            report['images'].append(safe('save-input', lambda rid=rid, final=final: save(rid, final, 'input')))
    with open(os.path.join(OUT, 'report.json'), 'w', encoding='utf-8') as f:
        json.dump(report, f, indent=1)


try:
    rd.InitialiseReplay(rd.GlobalEnvironment(), [])
    cap = rd.OpenCaptureFile()
    result = cap.OpenFile(CAPTURE, '', None)
    if not ok(result):
        raise RuntimeError('open file: %s' % result)
    if not cap.LocalReplaySupport():
        raise RuntimeError('no local replay support')
    result, controller = cap.OpenCapture(rd.ReplayOptions(), None)
    if not ok(result):
        raise RuntimeError('open capture: %s' % result)
    try:
        main(controller)
    finally:
        controller.Shutdown()
        cap.Shutdown()
except Exception:
    report['fatal'] = traceback.format_exc()
    with open(os.path.join(OUT, 'report.json'), 'w', encoding='utf-8') as f:
        json.dump(report, f, indent=1)
finally:
    os._exit(0)
