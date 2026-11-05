#!/usr/bin/env python3
"""Check the NDS headers and copied ARM9 routines after a complete SDK build."""
from pathlib import Path
import os
import re
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]
COMPONENTS = {
    '3dssplash': '3dssplash', 'booter': 'booter', 'booter_fc': 'booter_fc',
    'gbapatcher': 'gbapatcher', 'imageview': 'imageview', 'manual': 'manual',
    'quickmenu': 'mainmenu', 'romsel_aktheme': 'romsel_aktheme',
    'romsel_dsimenutheme': 'romsel_dsimenutheme', 'romsel_r4theme': 'romsel_r4theme',
    'settings': 'settings', 'slot1launch': 'slot1launch', 'title': 'title',
}

def crc16(data):
    crc = 0xffff
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ (0xa001 if crc & 1 else 0)
    return crc

images = [ROOT/directory/f'{name}.nds' for directory, name in COMPONENTS.items()]
images += [ROOT/'booter_fc'/f'booter_{variant}.nds'
           for variant in ('r4ig', 'r4idsn', 'acekard2', 'r4ils')]
images.append(ROOT/'booter_fc/booter_fc.dsi')
for path in images:
    data = path.read_bytes()
    assert len(data) >= 0x200, path
    assert crc16(data[:0x15e]) == struct.unpack_from('<H', data, 0x15e)[0], path
    for offset in (0x20, 0x30):
        start, entry, address, size = struct.unpack_from('<4I', data, offset)
        assert size and start + size <= len(data), path
        assert address <= (entry & ~1) < address + size, path
    start, _, _, size = struct.unpack_from('<4I', data, 0x20)
    arm9 = data[start:start + size]
    dldi = arm9.find(b'\xed\xa5\x8d\xbf Chishm\0')
    assert dldi >= 0 and arm9[dldi + 15] == 15, f'{path}: preserve the 32 KiB DLDI allocation'

# OAM 4bpp sprites own one 16-color palette bank. Oversized Grit palettes
# overwrite neighboring banks; a three-color GL palette triggers the SDK's
# short byte-copy path, which cannot write VRAM.
for path in (ROOT/'title/arm9/build').glob('icon_*.h'):
    palette = re.search(r'#define\s+\w+PalLen\s+(\d+)', path.read_text())
    assert palette and int(palette[1]) == 32, f'{path}: use a 16-color OAM palette'
cursor = (ROOT/'quickmenu/arm9/build/cursor.h').read_text()
assert re.search(r'#define\s+cursorPalLen\s+32\b', cursor), 'Pad the cursor palette for VRAM copies'

toolchain = Path(os.environ.get('WONDERFUL_TOOLCHAIN', '/opt/wonderful'))
readelf = toolchain/'toolchain/gcc-arm-none-eabi/bin/arm-none-eabi-readelf'
objdump = toolchain/'toolchain/gcc-arm-none-eabi/bin/arm-none-eabi-objdump'

# The old SDK's region 6 is executable RAM in BlocksDS. Overwriting it
# removes code access and causes a prefetch abort on every DSi cold boot.
mpu = subprocess.check_output([str(objdump), '-d',
    '--disassemble=CP15_ExtendUncachedRam', str(ROOT/'title/arm9/title.elf')], text=True)
assert re.search(r'\bmcr\s+15,\s*0,\s*r0,\s*cr6,\s*cr5,\s*\{0\}', mpu), 'Extend uncached MPU region 5'
assert '.word\t0x0c000031' in mpu, 'Map the 32 MiB uncached RAM mirror'
vblank = subprocess.check_output([str(objdump), '-d',
    '--disassemble=_Z13vBlankHandlerv', str(ROOT/'romsel_dsimenutheme/arm9/romsel_dsimenutheme.elf')], text=True)
assert '<_Z13vBlankHandlerv>:' in vblank
assert not re.search(r'<(?:mmEffect\w*|mmWaitForHandle|_ZN12SoundControl8playStop\w*)>', vblank), \
    'VBlank must queue sound effects instead of waiting for a Maxmod FIFO reply'
loaders = ['universal/bootloader_menu', 'universal/bootloader_app']
loaders += [f'{name}/bootloader' for name in ('booter', 'booter_fc', 'gbapatcher', 'settings', 'title')]
required = {'resetMemory2_ARM9', 'startBinary_ARM9'}
for loader in loaders:
    path = ROOT/loader/'load.elf'
    symbols = subprocess.check_output([str(readelf), '-sW', str(path)], text=True)
    found = set()
    for line in symbols.splitlines():
        fields = line.split()
        if len(fields) == 8 and fields[3] == 'FUNC' and fields[-1].endswith('_ARM9'):
            # ELF marks Thumb functions with bit zero. These functions are
            # copied by ARM7 to fixed addresses and entered in ARM state.
            assert int(fields[1], 16) & 1 == 0, f'{path}: {fields[-1]} is Thumb'
            assert int(fields[2]) > 0, path
            found.add(fields[-1])
    assert required <= found, (path, found)
print(f'{len(images)} NDS headers, DLDI allocations, title MPU mapping and {len(loaders)} ARM9 loader images passed')
