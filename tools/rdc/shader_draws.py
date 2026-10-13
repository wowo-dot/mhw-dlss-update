# Headless: every draw that uses one pixel shader, with its output state and
# the pixels it changes. Inside RenderDoc's embedded Python:
#   set MHW_RDC_CAPTURE=<file.rdc>  MHW_RDC_OUT=<folder>  MHW_RDC_SHA=<pixel shader SHA-256>
#   qrenderdoc.exe --python tools/rdc/shader_draws.py   (or tools/rdc/run.py)
# The SHA-256 is over the DXBC container, as the mod hashes it
# (route.silhouette.unknown_ps_sha256). Writes shader_draws.json and the
# shader's disassembly to MHW_RDC_OUT, then exits.
import os, json, hashlib, traceback
import renderdoc as rd

CAPTURE = os.environ['MHW_RDC_CAPTURE']
OUT = os.environ['MHW_RDC_OUT']
SHA = os.environ['MHW_RDC_SHA'].lower()
os.makedirs(OUT, exist_ok=True)
report = dict(capture=CAPTURE, sha256=SHA, errors=[])


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


def name(value):
    return str(value).split('.')[-1]


def main(controller):
    sf = controller.GetStructuredFile()
    textures = {t.resourceId: t for t in controller.GetTextures()}
    names = {r.resourceId: r.name for r in controller.GetResources()}

    # The shader: hash every shader resource's bytecode.
    shaders = [r.resourceId for r in controller.GetResources() if r.type == rd.ResourceType.Shader]
    report['shaders_hashed'] = len(shaders)
    matches = []
    for sid in shaders:
        def digest(sid=sid):
            entries = controller.GetShaderEntryPoints(sid)
            if not entries:
                return None
            refl = controller.GetShader(rd.ResourceId.Null(), sid, entries[0])
            return hashlib.sha256(bytes(refl.rawBytes)).hexdigest(), refl
        d = safe('hash', digest)
        if d and d[0] == SHA:
            matches.append((sid, d[1]))
    report['shader_ids'] = [str(int(s)) for s, _ in matches]
    if not matches:
        return

    actions = {}

    def walk(items):
        for a in items:
            actions[a.eventId] = a
            walk(a.children)
    walk(controller.GetRootActions())
    ordered = sorted(actions)

    def tex(rid):
        t = textures.get(rid)
        if t is None:
            return dict(id=str(int(rid)), name=names.get(rid, ''))
        return dict(id=str(int(rid)), name=names.get(rid, ''), w=t.width, h=t.height, format=t.format.Name())

    def pixels(rid, eid):
        controller.SetFrameEvent(eid, True)
        sub = rd.Subresource(0, 0, 0)
        return bytes(controller.GetTextureData(rid, sub))

    def changed(before, after, t):
        stride = max(1, len(after) // max(1, t.width * t.height))
        count, box = 0, [t.width, t.height, -1, -1]
        for i in range(0, min(len(before), len(after)), stride):
            if before[i:i + stride] != after[i:i + stride]:
                count += 1
                p = i // stride
                x, y = p % t.width, p // t.width
                box = [min(box[0], x), min(box[1], y), max(box[2], x), max(box[3], y)]
        return dict(pixels=count, box=box if count else None, bytes_per_pixel=stride)

    draws = []
    for sid, refl in matches:
        with open(os.path.join(OUT, 'ps_%d.txt' % int(sid)), 'w', encoding='utf-8') as f:
            targets = controller.GetDisassemblyTargets(True)
            f.write(safe('disassemble', lambda: controller.DisassembleShader(rd.ResourceId.Null(), refl, targets[0]), ''))
        report.setdefault('outputs_signature', [dict(name=s.semanticName, index=s.semanticIndex, mask=s.regChannelMask)
                                                for s in refl.outputSignature])
        # Shaders have no usage records: scan every draw's pixel shader.
        uses = [e for e in ordered if actions[e].flags & rd.ActionFlags.Drawcall]
        report['draws_scanned'] = len(uses)
        for eid in uses:
            a = actions.get(eid)
            if a is None or not (a.flags & rd.ActionFlags.Drawcall):
                continue
            controller.SetFrameEvent(eid, True)
            pipe = controller.GetPipelineState()
            d12 = controller.GetD3D12PipelineState()
            if pipe.GetShader(rd.ShaderStage.Pixel) != sid:
                continue
            info = dict(eid=eid, name=a.GetName(sf), vertices=a.numIndices, instances=a.numInstances,
                        marker=safe('marker', lambda: ' / '.join(p.GetName(sf) for p in (a.parent,) if p), ''))
            outs = [o for o in pipe.GetOutputTargets() if (o.resource if hasattr(o, 'resource') else o.resourceId) != rd.ResourceId.Null()]
            info['outputs'] = [tex(o.resource if hasattr(o, 'resource') else o.resourceId) for o in outs]
            ds = pipe.GetDepthTarget()
            dsid = ds.resource if hasattr(ds, 'resource') else ds.resourceId
            info['depth_target'] = tex(dsid) if dsid != rd.ResourceId.Null() else None
            info['blend'] = safe('blend', lambda: [dict(enabled=b.enabled, src=name(b.colorBlend.source), dst=name(b.colorBlend.destination),
                                                        op=name(b.colorBlend.operation), asrc=name(b.alphaBlend.source),
                                                        adst=name(b.alphaBlend.destination), mask=b.writeMask)
                                                   for b in pipe.GetColorBlends()[:max(1, len(outs))]], [])
            om = d12.outputMerger
            info['depth'] = safe('depth', lambda: dict(enable=om.depthStencilState.depthEnable, write=om.depthStencilState.depthWrites,
                                                       func=name(om.depthStencilState.depthFunction),
                                                       stencil=om.depthStencilState.stencilEnable), {})
            info['raster'] = safe('raster', lambda: dict(cull=name(d12.rasterizer.state.cullMode), fill=name(d12.rasterizer.state.fillMode)), {})
            info['viewport'] = safe('viewport', lambda: (lambda v: dict(x=v.x, y=v.y, w=v.width, h=v.height))(pipe.GetViewport(0)), {})
            info['scissor'] = safe('scissor', lambda: (lambda s: dict(x=s.x, y=s.y, w=s.width, h=s.height))(pipe.GetScissor(0)), {})
            # The pixels it changes: the first output before and after the draw.
            prev = [e for e in ordered if e < eid]
            if outs and prev:
                rid = outs[0].resource if hasattr(outs[0], 'resource') else outs[0].resourceId
                t = textures.get(rid)
                if t is not None:
                    before = safe('before', lambda: pixels(rid, prev[-1]), b'')
                    after = safe('after', lambda: pixels(rid, eid), b'')
                    info['changed'] = changed(before, after, t) if before and after else None
            draws.append(info)
    report['draws'] = draws


try:
    rd.InitialiseReplay(rd.GlobalEnvironment(), [])
    cap = rd.OpenCaptureFile()
    result = cap.OpenFile(CAPTURE, '', None)
    if not ok(result):
        raise RuntimeError('open file: %s' % result)
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
finally:
    with open(os.path.join(OUT, 'shader_draws.json'), 'w', encoding='utf-8') as f:
        json.dump(report, f, indent=1)
    os._exit(0)
