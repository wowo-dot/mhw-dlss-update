"""Hidden-surface silhouette shader allowlist from the private shader census.

  python -B tools/gen_silhouette_allowlist.py candidates --census <census dir> --out <list.txt>
  python -B tools/gen_silhouette_allowlist.py allowlist --census <census dir> --fixture <result.json>...
      [--out src/scaled_temporal/silhouette_allowlist.inc]

candidates: every ps_5_0 program in <census dir>/blobs whose reflection marks
CBMhMaterialGlobal.fMaterialHiddenSurfaceDrawIntensity as used and which reads
SV_Position. Writes one blob path per line, the input of
`silhouette_shader_fixture <out dir> --candidates @<list.txt>`.

allowlist: the candidates the fixture passed (its result.json "shaders"), as
C++ initialisers {SHA-256 bytes, size} with the technique family as a comment.
Only identities are written, never shader code. The census itself (game shader
bytecode) stays in the ignored local/ folder.
"""
import argparse
import json
import struct
import sys
from pathlib import Path

VARIABLE = 'fMaterialHiddenSurfaceDrawIntensity'


def chunks(blob):
    if blob[:4] != b'DXBC':
        raise ValueError('not a DXBC container')
    count = struct.unpack_from('<I', blob, 28)[0]
    out = {}
    for i in range(count):
        at = struct.unpack_from('<I', blob, 32 + 4 * i)[0]
        size = struct.unpack_from('<I', blob, at + 4)[0]
        out[blob[at:at + 4].decode('latin1')] = blob[at + 8:at + 8 + size]
    return out


def text(data, at):
    return data[at:data.index(b'\0', at)].decode('latin1')


def variable_used(rdef, name):
    count, offset = struct.unpack_from('<II', rdef, 0)
    for i in range(count):
        _, variables, variable_offset = struct.unpack_from('<III', rdef, offset + 24 * i)
        for j in range(variables):
            name_at, _, _, flags = struct.unpack_from('<IIII', rdef, variable_offset + 40 * j)
            if text(rdef, name_at) == name:
                return bool(flags & 2)
    return False


def reads_position(signature):
    count = struct.unpack_from('<I', signature, 0)[0]
    return any(text(signature, struct.unpack_from('<I', signature, 8 + 24 * i)[0]).lower() == 'sv_position'
               for i in range(count))


def is_candidate(blob):
    try:
        c = chunks(blob)
        program = c.get('SHEX')
        if not program or struct.unpack_from('<I', program, 0)[0] >> 16 != 0:  # pixel shader
            return False
        return variable_used(c['RDEF'], VARIABLE) and reads_position(c.get('ISGN', b'\0' * 8))
    except (ValueError, KeyError, struct.error):
        return False


def families(census):
    by_ps = {}
    for technique in json.loads((census / 'techniques.json').read_text(encoding='utf-8')):
        ps = technique.get('stages', {}).get('ps')
        if ps:
            by_ps.setdefault(ps['sha256'], set()).add('_'.join(technique['name'].split('_')[:4]))
    return by_ps


def candidates(census):
    return sorted(p for p in (census / 'blobs').glob('*.dxbc') if is_candidate(p.read_bytes()))


def initialiser(digest, size, comment):
    data = bytes.fromhex(digest)
    return '{{' + ','.join(f'0x{b:02x}' for b in data) + '},' + str(size) + '}, // ' + comment


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('action', choices=('candidates', 'allowlist'))
    p.add_argument('--census', type=Path, required=True)
    p.add_argument('--fixture', type=Path, nargs='+', help='one or more candidate-run result.json files')
    p.add_argument('--out', type=Path)
    a = p.parse_args()
    found = candidates(a.census)
    if a.action == 'candidates':
        if not a.out:
            sys.exit('--out required')
        a.out.write_text(''.join(f'{path.resolve()}\n' for path in found), encoding='utf-8')
        print(json.dumps({'candidates': len(found), 'list': str(a.out)}))
        return
    if not a.fixture:
        sys.exit('--fixture required')
    verdicts = {}
    for path in a.fixture:
        result = json.loads(path.read_text(encoding='utf-8'))
        if not result.get('candidates'):
            sys.exit(f'{path} is not a candidate run')
        for shader in result['shaders']:
            if verdicts.setdefault(shader['sha256'], shader) != shader:
                sys.exit(f"conflicting verdicts for {shader['sha256']}")
    names = families(a.census)
    lines, passed = [], 0
    for path in found:
        digest = path.stem
        verdict = verdicts.get(digest)
        if not verdict:
            sys.exit(f'candidate {digest} missing from the fixture result')
        if not verdict['passed']:
            continue
        size = path.stat().st_size
        if verdict.get('size') != size:
            sys.exit(f'candidate {digest} size differs from the fixture result')
        lines.append(initialiser(digest, size, ', '.join(sorted(names.get(digest, {'?'})))))
        passed += 1
    out = a.out or Path(__file__).resolve().parents[1] / 'src' / 'scaled_temporal' / 'silhouette_allowlist.inc'
    header = ('// Generated by tools/gen_silhouette_allowlist.py: hidden-surface pixel shaders\n'
              f'// ({passed} of {len(found)} census candidates) that passed silhouette_shader_fixture.\n'
              '// {SHA-256, size}; identities only, no shader code.\n')
    out.write_text(header + '\n'.join(lines) + '\n', encoding='utf-8', newline='\n')
    print(json.dumps({'candidates': len(found), 'allowed': passed, 'out': str(out)}))


if __name__ == '__main__':
    main()
