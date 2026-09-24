#!/usr/bin/env python3
"""Package the Win32 plugin with its configuration and dependency licenses."""
import argparse
import hashlib
import os
from pathlib import Path
import re
import shutil
import struct
import zipfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', required=True, type=Path)
    parser.add_argument('--output', default=Path('dist'), type=Path)
    parser.add_argument('--tag', default='')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    header = (root / 'SniperElite3.FreeCamera/Version.h').read_text()
    version = re.search(r'^#define CAMERA_TWEAKS_VERSION "([0-9]+\.[0-9]+(?:\.[0-9]+)?)"$', header, re.M)
    if not version:
        parser.error('Version.h does not define a valid release version')
    version = version.group(1)
    if args.tag and args.tag != f'v{version}':
        parser.error(f'Tag {args.tag!r} does not match source version v{version}')
    binary = args.binary.read_bytes()
    if len(binary) < 64 or binary[:2] != b'MZ':
        parser.error('The ASI is not a PE binary')
    pe = struct.unpack_from('<I', binary, 0x3c)[0]
    if pe + 26 > len(binary) or binary[pe:pe + 4] != b'PE\0\0':
        parser.error('The ASI has an invalid PE header')
    if (struct.unpack_from('<H', binary, pe + 4)[0] != 0x14c or
            struct.unpack_from('<H', binary, pe + 24)[0] != 0x10b or
            not struct.unpack_from('<H', binary, pe + 22)[0] & 0x2000):
        parser.error('The ASI must be a Windows x86/PE32 DLL')
    output = args.output
    output.mkdir(parents=True, exist_ok=True)
    asi = output / 'SniperElite3.CameraTweaks.asi'
    ini = output / 'SniperElite3.CameraTweaks.ini'
    if args.binary.resolve() != asi.resolve():
        shutil.copyfile(args.binary, asi)
    shutil.copyfile(root / ini.name, ini)
    archive = output / f'SniperElite3.CameraTweaks-v{version}-win32.zip'
    files = {
        asi.name: asi,
        ini.name: ini,
        'README.md': root / 'README.md',
        'licenses/Dear-ImGui.txt': root / 'third_party/imgui/LICENSE.txt',
        'licenses/MinHook.txt': root / 'third_party/minhook/LICENSE.txt',
    }
    with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as package:
        for name, path in files.items():
            package.write(path, name)
    checksums = ''.join(f'{hashlib.sha256(path.read_bytes()).hexdigest()}  {path.name}\n'
                        for path in (asi, ini, archive))
    (output / 'SHA256SUMS.txt').write_text(checksums, encoding='ascii')
    if os.environ.get('GITHUB_OUTPUT'):
        with open(os.environ['GITHUB_OUTPUT'], 'a', encoding='utf-8') as handle:
            handle.write(f'version={version}\n')
    print(f'Packaged {archive}')


if __name__ == '__main__':
    main()
