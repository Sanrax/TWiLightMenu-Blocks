#!/usr/bin/env python3
"""Host regression tests; no SDK, BIOS, ROMs or network access required."""
from pathlib import Path
import importlib.util
import hashlib
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]

def run(*args):
    subprocess.run(args, cwd=ROOT, check=True)

with tempfile.TemporaryDirectory(prefix='twlm-tests-') as temp:
    output = Path(temp)
    run('cc', '-std=gnu11', '-O2', '-Wall', '-Wextra', '-Werror', '-no-pie',
        '-D__BLOCKSDS__', '-Itests/include', 'tests/bootfs.c', '-o', str(output/'bootfs'))
    run(str(output/'bootfs'), str(output/'bootfs.nds'))
    run('c++', '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror',
        '-ffunction-sections', '-fdata-sections', '-Wl,--gc-sections', '-Iuniversal/include',
        'tests/launch.cpp', 'universal/source/launch/launchPlanner.cpp',
        'universal/source/common/launcherArgs.cpp', 'universal/source/common/stringtool.cpp',
        '-o', str(output/'launch'))
    run(str(output/'launch'))

    run('c++', '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror',
        '-Iuniversal/include', 'tests/inifile.cpp', 'universal/source/common/inifile.cpp',
        '-o', str(output/'inifile'))
    run(str(output/'inifile'), str(output/'settings.ini'))

    run('c++', '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror',
        '-Itests/include', '-Iuniversal/include', 'tests/picolaunch.cpp',
        '-o', str(output/'picolaunch'))
    run(str(output/'picolaunch'))

    run('c++', '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror',
        '-Iuniversal/include', 'tests/ndsmetadata.cpp', '-o', str(output/'ndsmetadata'))
    run(str(output/'ndsmetadata'))

    run('c++', '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror',
        '-Iuniversal/include', 'tests/soundbank.cpp', '-o', str(output/'soundbank'))
    run(str(output/'soundbank'), 'romsel_dsimenutheme/nitrofiles/sound/defaultfx.bin',
        'romsel_dsimenutheme/nitrofiles/sound/saturnfx.bin')

    run('c++', '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror', '-fno-exceptions',
        '-Itests/include', '-Iuniversal/include', '-Iromsel_dsimenutheme/arm9/source/graphics',
        'tests/texture.cpp', 'romsel_dsimenutheme/arm9/source/graphics/Texture.cpp',
        'universal/source/lodepng/lodepng.cpp', '-o', str(output/'texture'))
    run(str(output/'texture'), str(output), 'romsel_dsimenutheme/nitrofiles')

    run('c++', '-O2', 'tools/r4denc/r4denc.cpp', '-o', str(output/'r4denc'))
    data = bytes((i * 37 + i // 256) & 255 for i in range(1283))
    (output/'input.nds').write_bytes(data)
    # Vectors from unmodified devkitPro/dstools 807da5d1; spans sector
    # boundaries and a partial sector for both supported autoboot keys.
    for key, digest in (
        ('0x484a', '15116faf6ae1db0059cdfd519ba41e4c6b97163be9ae8036f5b65d683a9541ef'),
        ('0x4002', 'e493eb4806278a1b9c817904155c4dd971e0e1741bd577eb84cfa9117194c174'),
    ):
        encoded, decoded = output/f'{key}.dat', output/f'{key}.nds'
        run(str(output/'r4denc'), '-k', key, str(output/'input.nds'), str(encoded))
        assert hashlib.sha256(encoded.read_bytes()).hexdigest() == digest
        run(str(output/'r4denc'), '-k', key, str(encoded), str(decoded))
        assert decoded.read_bytes() == data
    print('R4/Ace3DS+ encoder compatibility vectors and round trips passed')

spec = importlib.util.spec_from_file_location('banner', ROOT/'tools/make_banner.py')
banner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(banner)
image = banner.make_banner((ROOT/'gbapatcher/icon.bmp').read_bytes(), 'GBA patcher;Test')
assert len(image) == 0x840
assert struct.unpack_from('<H', image)[0] == 1
assert struct.unpack_from('<H', image, 2)[0] == banner.crc16(image[0x20:])
assert image[0x240:0x240+32].decode('utf-16le').rstrip('\0') == 'GBA patcher\nTest'
try:
    banner.make_banner(b'BM', 'Test')
except ValueError:
    pass
else:
    raise AssertionError('Truncated BMP was accepted')
print('16-color banner encoding and malformed input checks passed')
