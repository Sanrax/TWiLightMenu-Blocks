#!/usr/bin/env python3
"""Host regression tests; no SDK, BIOS, ROMs or network access required."""
from pathlib import Path
import importlib.util
import hashlib
import shutil
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

    run('cc', '-std=gnu11', '-O2', '-Wall', '-Wextra', '-Werror',
        '-Wno-pointer-to-int-cast', '-Wno-int-to-pointer-cast',
        '-Itests/include/sdmmc', '-Itests/include', '-Iuniversal/sdmmc/arm7/include',
        'tests/sdmmc.c', '-o', str(output/'sdmmc'))
    run(str(output/'sdmmc'))

    run('cc', '-std=gnu11', '-O2', '-Wall', '-Wextra', '-Werror',
        '-Itests/include/sdmmc', '-Itests/include', '-Iuniversal/arm7/include',
        'tests/powerbutton.c', '-o', str(output/'powerbutton'))
    run(str(output/'powerbutton'))

    run('c++', '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror',
        '-Itests/include/i2c', '-Itests/include/sdmmc', '-Itests/include',
        'tests/i2c.cpp', '-o', str(output/'i2c'))
    run(str(output/'i2c'))

    for loader in ('booter/bootloader', 'booter_fc/bootloader', 'settings/bootloader',
                   'title/bootloader', 'universal/bootloader_menu', 'universal/bootloader_app'):
        run('cc', '-std=gnu11', '-O2', '-Wall', '-Wextra', '-Werror', '-Wno-sign-compare',
            '-Itests/include', '-Iuniversal/include',
            f'-DBOOT_FAT_SOURCE="../{loader}/source/fat.c"', 'tests/loaderfat.c',
            '-o', str(output/'loaderfat'))
        run(str(output/'loaderfat'))
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
        '-Iuniversal/include', 'tests/language.cpp', '-o', str(output/'language'))
    run(str(output/'language'), *(f'{app}/nitrofiles/languages' for app in
        ('romsel_dsimenutheme', 'quickmenu', 'settings', 'title', 'manual', 'imageview')))

    run('c++', '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror',
        '-Iuniversal/include', 'tests/fontmap.cpp', '-Wl,--wrap=fread', '-o', str(output/'fontmap'))
    fonts = [font for app in ('title', 'settings', 'quickmenu', 'romsel_dsimenutheme',
                             'romsel_aktheme', 'romsel_r4theme', 'manual', 'imageview')
             for font in (ROOT/app/'nitrofiles/graphics/font').glob('*.nftr')]
    fonts += list((ROOT/'7zfile/_nds/TWiLightMenu/extras/fonts').glob('*/*.nftr'))
    run(str(output/'fontmap'), *map(str, sorted(fonts)))

    run('c++', '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror',
        '-Itests/include', '-Iuniversal/include', 'tests/picolaunch.cpp',
        '-o', str(output/'picolaunch'))
    run(str(output/'picolaunch'))

    run('c++', '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror',
        '-Iuniversal/include', 'tests/ndsmetadata.cpp', '-o', str(output/'ndsmetadata'))
    run(str(output/'ndsmetadata'))

    run('c++', '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror',
        '-Iuniversal/include', 'tests/bootpath.cpp', '-o', str(output/'bootpath'))
    run(str(output/'bootpath'))

    run('c++', '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror',
        '-Iuniversal/include', 'tests/soundbank.cpp', '-o', str(output/'soundbank'))
    run(str(output/'soundbank'), 'romsel_dsimenutheme/nitrofiles/sound/defaultfx.bin',
        'romsel_dsimenutheme/nitrofiles/sound/saturnfx.bin')

    run('c++', '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror',
        '-Iuniversal/include', 'tools/photo_index.cpp', 'universal/source/lodepng/lodepng.cpp',
        '-o', str(output/'photo-index'))
    nitro = ROOT/'romsel_dsimenutheme/nitrofiles'
    for png in sorted((nitro/'themes').rglob('*.png')):
        indexed = (output/'nitro:'/png.relative_to(nitro)).with_suffix('.pht')
        indexed.parent.mkdir(parents=True, exist_ok=True)
        run(str(output/'photo-index'), str(png), str(indexed))

    run('c++', '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror', '-fno-exceptions',
        '-Itests/include', '-Iuniversal/include', '-Iromsel_dsimenutheme/arm9/source/graphics',
        'tests/texture.cpp', 'romsel_dsimenutheme/arm9/source/graphics/Texture.cpp',
        'universal/source/lodepng/lodepng.cpp', '-o', str(output/'texture'))
    run(str(output/'texture'), str(output), 'romsel_dsimenutheme/nitrofiles')
    for name in ('unsupported-large', 'unsupported-colors'):
        indexed = output/(name + '.pht')
        run(str(output/'photo-index'), str(output/(name + '.png')), str(indexed))
        assert struct.unpack_from('<H', indexed.read_bytes(), 8)[0] == 0
    for photo in sorted((ROOT/'romsel_dsimenutheme/nitrofiles/languages').glob('*/photo_default.png')):
        run(str(output/'photo-index'), str(photo), str(output/(photo.parent.name + '.pht')))
    run('c++', '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror',
        '-Iuniversal/include', 'tests/photo.cpp', 'universal/source/lodepng/lodepng.cpp',
        '-o', str(output/'photo'))
    run(str(output/'photo'), str(output), 'romsel_dsimenutheme/nitrofiles/languages')

    classic = ROOT/'quickmenu/nitrofiles/graphics'
    classic_pngs = [classic/name for name in ('topbg.png', 'bottombg.png', 'clock.png')]
    classic_pngs += list((classic/'calendar').glob('calendar*.png'))
    classic_pngs += list((classic/'battery').glob('*.png'))
    classic_pngs += list((classic/'icons').glob('*.png'))
    classic_test = output/'classic'
    for png in classic_pngs:
        target = classic_test/'nitro:/graphics'/png.relative_to(classic)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(png, target)
        run(str(output/'photo-index'), str(png), str(target.with_suffix('.pht')), '--screen')
    run('c++', '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror',
        '-Iuniversal/include', 'tests/menuimage.cpp', 'universal/source/lodepng/lodepng.cpp',
        '-o', str(output/'menuimage'))
    run(str(output/'menuimage'), str(classic_test))

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
