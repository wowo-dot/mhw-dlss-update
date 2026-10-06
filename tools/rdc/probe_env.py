# Run inside RenderDoc: qrenderdoc.exe --python tools/rdc/probe_env.py
# RenderDoc's embedded interpreter does not define __file__; the output path
# comes from MHW_RDC_OUT. Always exits the process.
import os, sys, json, traceback

result = dict(started=True, python=sys.version)
try:
    import renderdoc as rd
    result.update(renderdoc=rd.GetVersionString(), names=[n for n in dir(rd) if n.startswith(('Open', 'Init', 'Shutdown', 'Execute'))])
except Exception:
    result['error'] = traceback.format_exc()
finally:
    folder = os.environ.get('MHW_RDC_OUT', os.environ.get('TEMP', '.'))
    os.makedirs(folder, exist_ok=True)
    with open(os.path.join(folder, 'probe_env.json'), 'w') as f:
        json.dump(result, f, indent=1)
    os._exit(0)
