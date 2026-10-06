"""Run a RenderDoc analysis script headlessly and wait for it.

Usage:
  python -B tools/rdc/run.py <script.py> --capture <file.rdc> --out <folder> [--resource ID] [--save EID,EID]

Needs MHW_RENDERDOC to point at a RenderDoc folder containing qrenderdoc.exe
(machine-local; never hard-coded). Scripts run inside RenderDoc's embedded
Python via `qrenderdoc --python` and receive their inputs as MHW_RDC_*
environment variables, because that interpreter does not define __file__.
"""
import argparse
import os
import subprocess
from pathlib import Path


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('script', type=Path)
    p.add_argument('--capture', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--resource')
    p.add_argument('--save', default='')
    p.add_argument('--timeout', type=int, default=900)
    a = p.parse_args()
    home = os.environ.get('MHW_RENDERDOC')
    if not home or not (Path(home) / 'qrenderdoc.exe').exists():
        raise SystemExit('set MHW_RENDERDOC to the folder containing qrenderdoc.exe')
    env = dict(os.environ, MHW_RDC_CAPTURE=str(a.capture.resolve()), MHW_RDC_OUT=str(a.out.resolve()),
               MHW_RDC_SAVE=a.save)
    if a.resource:
        env['MHW_RDC_RESOURCE'] = a.resource
    a.out.mkdir(parents=True, exist_ok=True)
    result = subprocess.run([str(Path(home) / 'qrenderdoc.exe'), '--python', str(a.script.resolve())],
                            env=env, timeout=a.timeout)
    print(f'exit {result.returncode}; outputs in {a.out}')


if __name__ == '__main__':
    main()
