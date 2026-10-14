"""tools/package_release.py on synthetic SDK folders (run from tests/ with tools/ on sys.path)."""
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import package_release as pr  # noqa: E402


def write(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)
    return hashlib.sha256(data).hexdigest()


class PackageRelease(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        root = Path(self.tmp.name)
        self.sl, self.dlss, self.out = root / 'sl', root / 'dlss', root / 'out'
        self.saved = dict(pr.STREAMLINE), dict(pr.NGX)
        for name in list(pr.STREAMLINE):
            pr.STREAMLINE[name] = write(self.sl / 'bin' / 'x64' / name, f'nvidia {name}'.encode())
        for name in list(pr.NGX):
            pr.NGX[name] = write(self.dlss / 'lib' / 'Windows_x86_64' / 'rel' / name, f'nvidia {name}'.encode())
        for src, _ in pr.LICENSES['streamline']:
            write(self.sl / src, f'license {src}'.encode())
        for src, _ in pr.LICENSES['dlss']:
            write(self.dlss / src, f'license {src}'.encode())
        self.dll = root / 'mhw_dlss_alpha.dll'
        write(self.dll, b'MZ ... {"event":"start","schema":1,"version":"9.8.7","build":"alpha"} ...')

    def tearDown(self):
        pr.STREAMLINE.clear(); pr.STREAMLINE.update(self.saved[0])
        pr.NGX.clear(); pr.NGX.update(self.saved[1])
        self.tmp.cleanup()

    def run_tool(self, version='9.8.7'):
        sys.argv = ['package_release.py', '--dll', str(self.dll), '--version', version, '--streamline-sdk', str(self.sl),
                    '--dlss-sdk', str(self.dlss), '--out', str(self.out), '--allow-dirty']
        pr.main()

    def test_package_layout_manifest_and_zip(self):
        self.run_tool()
        folder = self.out / 'mhw-dlss-9.8.7'
        data = folder / 'AsteraLoader' / 'plugins' / 'mhw_dlss'
        self.assertEqual(sorted(p.name for p in folder.iterdir()), ['AsteraLoader'])
        self.assertEqual(sorted(p.name for p in (folder / 'AsteraLoader' / 'plugins').iterdir()), ['mhw_dlss'])
        self.assertEqual(pr.sha(data / 'mhw_dlss.dll'), pr.sha(self.dll))
        for name in ['mhw_dlss.ini', 'README.txt', 'LICENSE.txt', 'THIRD-PARTY-NOTICES.txt', 'manifest.json',
                     *pr.STREAMLINE, *pr.NGX, *(f'licenses/{dst}' for _, dst in pr.LICENSES['streamline'] + pr.LICENSES['dlss']),
                     *(f'licenses/{dst}' for _, _, dst in pr.BUILT_IN)]:
            self.assertTrue((data / name).is_file(), name)
        for name, digest in {**pr.STREAMLINE, **pr.NGX}.items():
            self.assertEqual(pr.sha(data / name), digest)
        manifest = json.loads((data / 'manifest.json').read_text(encoding='utf-8'))
        self.assertEqual(manifest['version'], '9.8.7')
        for entry in manifest['files']:
            self.assertEqual(pr.sha(folder / entry['path']), entry['sha256'])
        ini = (data / 'mhw_dlss.ini').read_text(encoding='utf-8')
        self.assertIn('runtime_dir = .', ini); self.assertIn('log_dir = logs', ini)
        self.assertIn('dlss_mode = performance', ini)
        readme = (data / 'README.txt').read_text(encoding='utf-8')
        self.assertIn('not affiliated with, sponsored or endorsed by NVIDIA', readme)
        self.assertIn('Uninstall: delete the AsteraLoader/plugins/mhw_dlss folder.', readme)
        self.assertIn('an Astera Loader plugin', readme)
        self.assertIn('With CMFM installed', readme)
        self.assertNotIn('winmm', readme)
        self.assertIn('MIT License (LICENSE.txt)', readme)
        self.assertIn('MIT License', (data / 'LICENSE.txt').read_text(encoding='utf-8'))
        notices = (data / 'THIRD-PARTY-NOTICES.txt').read_text(encoding='utf-8')
        for component, _, dst in pr.BUILT_IN:
            self.assertIn(component, notices); self.assertIn(f'licenses/{dst}', notices)
        self.assertIn('Copyright (C) 2009-2017 Tsuda Kageyu', (data / 'licenses' / 'MinHook-LICENSE.txt').read_text(encoding='utf-8-sig'))
        with zipfile.ZipFile(self.out / 'mhw-dlss-9.8.7.zip') as z:
            self.assertEqual(sorted(z.namelist()),
                             sorted(f'mhw-dlss-9.8.7/{p}' for p in [e['path'] for e in manifest['files']] + ['AsteraLoader/plugins/mhw_dlss/manifest.json']))

    def test_refuses_changed_nvidia_file(self):
        write(self.sl / 'bin' / 'x64' / 'sl.common.dll', b'tampered')
        with self.assertRaises(SystemExit):
            self.run_tool()
        self.assertFalse(self.out.exists())

    def test_refuses_wrong_version_and_existing_output(self):
        with self.assertRaises(SystemExit):
            self.run_tool('9.8.8')
        self.run_tool()
        with self.assertRaises(SystemExit):
            self.run_tool()


if __name__ == '__main__':
    unittest.main()
