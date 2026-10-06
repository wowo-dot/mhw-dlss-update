# Headless constant-buffer and root-signature report inside RenderDoc's
# embedded Python:
#   set MHW_RDC_CAPTURE=<file.rdc>  MHW_RDC_OUT=<folder>  MHW_RDC_SAVE=<EID,EID,...>
#   qrenderdoc.exe --python tools/rdc/cbuffer_report.py   (or tools/rdc/run.py)
# For each event: the pixel shader's constant blocks (names, registers, variable
# offsets), the bound buffer/offset for each block, decoded variable values, and
# the D3D12 root signature elements. Writes cbuffers.json, then exits.
import os, json, traceback
import renderdoc as rd

CAPTURE = os.environ['MHW_RDC_CAPTURE']
OUT = os.environ['MHW_RDC_OUT']
EIDS = [int(x) for x in os.environ.get('MHW_RDC_SAVE', '').split(',') if x.strip()]
os.makedirs(OUT, exist_ok=True)
report = dict(capture=CAPTURE, errors=[], events=[])


def ok(result):
    if hasattr(result, 'OK'):
        return result.OK()
    return result == rd.ResultCode.Succeeded


def plain(obj, depth=3):
    """Public data attributes of a RenderDoc object, recursively and bounded."""
    if isinstance(obj, (bool, int, float, str)) or obj is None:
        return obj
    if depth <= 0:
        return str(obj)
    # SWIG arrays (rdcarray) are sequences without being list/tuple.
    if isinstance(obj, (list, tuple)) or (hasattr(obj, '__len__') and hasattr(obj, '__getitem__')):
        return [plain(x, depth - 1) for x in list(obj)[:64]]
    if type(obj).__name__ == 'ResourceId':
        return 'res' + str(int(obj))
    out = {}
    for name in dir(obj):
        if name.startswith('_'):
            continue
        try:
            value = getattr(obj, name)
        except Exception:
            continue
        if callable(value):
            continue
        out[name] = plain(value, depth - 1)
    return out if out else str(obj)


def values(var):
    v = var.value
    rows, cols = max(var.rows, 1), max(var.columns, 1)
    n = rows * cols
    try:
        f = list(v.f32v)[:n]
        u = list(v.u32v)[:n]
    except Exception:
        f, u = [], []
    entry = dict(name=var.name, rows=var.rows, columns=var.columns, f32=f, u32=u)
    if len(var.members):
        entry['members'] = [values(m) for m in var.members]
    return entry


def main(controller):
    for eid in EIDS:
        ev = dict(eid=eid)
        try:
            controller.SetFrameEvent(eid, True)
            pipe = controller.GetPipelineState()
            stage = rd.ShaderStage.Pixel
            refl = pipe.GetShaderReflection(stage)
            ev['ps'] = str(int(pipe.GetShader(stage)))
            blocks = []
            if refl is not None:
                for i, cb in enumerate(refl.constantBlocks):
                    block = dict(index=i, name=cb.name, byte_size=cb.byteSize,
                                 variables=[dict(name=v.name, offset=v.byteOffset, type=plain(v.type, 2))
                                            for v in cb.variables])
                    for attr in ('fixedBindNumber', 'bindPoint', 'fixedBindSetOrSpace', 'bufferBacked'):
                        if hasattr(cb, attr):
                            block[attr] = getattr(cb, attr)
                    try:
                        bound = pipe.GetConstantBlock(stage, i, 0)
                        desc = getattr(bound, 'descriptor', bound)
                        block['bound'] = plain(desc, 2)
                        res = getattr(desc, 'resource', None)
                        off = getattr(desc, 'byteOffset', 0)
                        size = getattr(desc, 'byteSize', 0)
                        contents = controller.GetCBufferVariableContents(
                            pipe.GetGraphicsPipelineObject(), refl.resourceId, stage,
                            pipe.GetShaderEntryPoint(stage), i, res, off, size)
                        block['values'] = [values(v) for v in contents]
                    except Exception:
                        block['bound_error'] = traceback.format_exc(limit=2)
                    blocks.append(block)
            ev['constant_blocks'] = blocks
            # Vertex stage blocks too: root CBV order spans both stages.
            vrefl = pipe.GetShaderReflection(rd.ShaderStage.Vertex)
            if vrefl is not None:
                ev['vs_blocks'] = []
                for i, cb in enumerate(vrefl.constantBlocks):
                    entry = dict(index=i, name=cb.name, register=getattr(cb, 'fixedBindNumber', None))
                    try:
                        bound = pipe.GetConstantBlock(rd.ShaderStage.Vertex, i, 0)
                        desc = getattr(bound, 'descriptor', bound)
                        entry['offset'] = getattr(desc, 'byteOffset', None)
                    except Exception:
                        entry['bound_error'] = traceback.format_exc(limit=1)
                    ev['vs_blocks'].append(entry)
            try:
                d3d = controller.GetD3D12PipelineState()
                ev['d3d12_root_attributes'] = [a for a in dir(d3d) if 'root' in a.lower()]
                if hasattr(d3d, 'rootSignature'):  # RenderDoc 1.46
                    ev['root_signature'] = plain(d3d.rootSignature, 6)
                else:
                    ev['root_signature'] = 'res' + str(int(d3d.rootSignatureResourceId))
                    ev['root_elements'] = plain(d3d.rootElements, 5)
            except Exception:
                ev['root_error'] = traceback.format_exc(limit=2)
        except Exception:
            ev['error'] = traceback.format_exc(limit=3)
        report['events'].append(ev)


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
    with open(os.path.join(OUT, 'cbuffers.json'), 'w') as f:
        json.dump(report, f, indent=1)
    os._exit(0)
