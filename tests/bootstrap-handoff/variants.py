#!/usr/bin/env python3
"""Build disposable diagnostic copies of the pinned official bootstrap.

Nothing in production builds calls this script. The input is never overwritten.
Only loadi0 (DS games on DSi) changes; the bootstrap frontend is kept byte exact.
"""
import argparse
from collections import defaultdict, deque
import hashlib
import json
from pathlib import Path
import struct
import subprocess

OFFICIAL_SHA256 = '39a3a5e62ea8d1f700c3735199424b1acdd0ccfcfcd31d61bc69984cb2e10b4c'
LOADER_SHA256 = '6410a844c9d18ee5de66a4d2da04dd6700777e6095b38465cb242acb8fd080e2'
SOURCE = 'https://github.com/DS-Homebrew/nds-bootstrap/tree/b9044ff2e66d5a522f9678c185f421a603f97d7a'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def unpack32(data, offset):
    return struct.unpack_from('<I', data, offset)[0]


def loader_entry(nds):
    """Find the root loadi0 file; return its FAT entry and byte interval."""
    fnt, fnt_size, fat, fat_size = struct.unpack_from('<4I', nds, 0x40)
    assert fnt + fnt_size <= len(nds) and fat + fat_size <= len(nds)
    offset, file_id = struct.unpack_from('<IH', nds, fnt)
    pos = fnt + offset
    while True:
        assert pos < fnt + fnt_size
        size = nds[pos]
        pos += 1
        if not size:
            raise ValueError('Pinned bootstrap has no root loadi0.lz77')
        name = nds[pos:pos + (size & 127)]
        pos += size & 127
        if size & 128:
            pos += 2
            continue
        entry = fat + file_id * 8
        assert entry + 8 <= fat + fat_size
        if name == b'loadi0.lz77':
            start, end = struct.unpack_from('<2I', nds, entry)
            assert start < end <= len(nds)
            return entry, start, end
        file_id += 1


def decompress(data):
    if not data or data[0] != 0x10:
        raise ValueError('Expected Nintendo LZ10')
    size = int.from_bytes(data[1:4], 'little')
    out = bytearray()
    pos = 4
    while len(out) < size:
        flags = data[pos]
        pos += 1
        for bit in range(7, -1, -1):
            if len(out) == size:
                break
            if flags & (1 << bit):
                pair = int.from_bytes(data[pos:pos + 2], 'big')
                pos += 2
                count, distance = (pair >> 12) + 3, (pair & 4095) + 1
                assert distance <= len(out) and len(out) + count <= size
                for _ in range(count):
                    out.append(out[-distance])
            else:
                out.append(data[pos])
                pos += 1
    return bytes(out)


def compress(data):
    """Greedy bounded-window LZ10; every generated stream is checked below."""
    assert 0 < len(data) < (1 << 24)
    out = bytearray(b'\x10' + len(data).to_bytes(3, 'little'))
    previous = defaultdict(deque)
    pos = 0
    while pos < len(data):
        flag_pos = len(out)
        out.append(0)
        for bit in range(7, -1, -1):
            if pos >= len(data):
                break
            candidates = previous[data[pos:pos + 3]]
            while candidates and candidates[0] < pos - 4096:
                candidates.popleft()
            best, distance = 0, 0
            for candidate in reversed(candidates):
                count = 0
                while count < min(18, len(data) - pos) and data[candidate + count] == data[pos + count]:
                    count += 1
                if count > best:
                    best, distance = count, pos - candidate
                    if best == 18:
                        break
            if best >= 3:
                out[flag_pos] |= 1 << bit
                out.extend((((best - 3) << 12) | (distance - 1)).to_bytes(2, 'big'))
                used = best
            else:
                out.append(data[pos])
                used = 1
            for i in range(pos, pos + used):
                previous[data[i:i + 3]].append(i)
            pos += used
    out.extend(bytes((-len(out)) % 4))
    assert decompress(out) == data
    return bytes(out)


def thumb_bl(address, target):
    displacement = target - (address + 4)
    assert displacement % 2 == 0 and -(1 << 22) <= displacement < (1 << 22)
    return struct.pack('<HH', 0xf000 | ((displacement >> 12) & 2047),
                       0xf800 | ((displacement >> 1) & 2047))


def crc16(data):
    value = 0xffff
    for byte in data:
        value ^= byte
        for _ in range(8):
            value = (value >> 1) ^ (0xa001 if value & 1 else 0)
    return value


def memory_payload(directory):
    """Build the opt-in ARM7 probe outside bootstrap's original BSS/stack."""
    source = Path(__file__).resolve().parent / 'memory-probe'
    prefix = '/opt/wonderful/toolchain/gcc-arm-none-eabi/bin/arm-none-eabi-'
    directory.mkdir(parents=True, exist_ok=True)
    elf, binary = directory / 'memory.elf', directory / 'memory.bin'
    subprocess.run([prefix + 'gcc', '-mcpu=arm7tdmi', '-mthumb', '-Os',
                    '-ffreestanding', '-fno-builtin', '-fno-unwind-tables',
                    '-fno-asynchronous-unwind-tables', '-nostdlib',
                    '-Wl,--build-id=none', '-T', str(source / 'link.ld'),
                    str(source / 'hooks.s'), str(source / 'probe.c'),
                    '-o', str(elf)], check=True)
    subprocess.run([prefix + 'objcopy', '-O', 'binary', str(elf), str(binary)], check=True)
    symbols = {}
    output = subprocess.check_output([prefix + 'nm', '-n', str(elf)], text=True)
    for line in output.splitlines():
        parts = line.split()
        if len(parts) == 3:
            symbols[parts[2]] = int(parts[0], 16)
    assert all(0x06036200 <= symbols[name] < 0x06037000
               for name in ('before_clear', 'after_clear', 'finish'))
    assert symbols['payload_end'] == 0x06036200 + len(binary.read_bytes())
    return binary.read_bytes(), symbols


def patched_loader(original, mode, payload=None):
    assert sha(original) == LOADER_SHA256
    changes = []
    image = bytearray(original)

    def patch(offset, before, after, reason):
        assert image[offset:offset + len(before)] == before
        assert len(before) == len(after)
        image[offset:offset + len(before)] = after
        changes.append(dict(offset=hex(offset), before=before.hex(), after=after.hex(), reason=reason))

    if mode == 'cpu':
        # loadBinary_ARM7 begins with sdmmc_set_ndma_slot(0). Use its existing
        # CPU path (slot 4) instead. Its normal final set-slot-4 is unchanged.
        assert original[0x97f6:0x97fa] == thumb_bl(0x97f6, 0x20dfc)
        patch(0x97f2, bytes.fromhex('0020'), bytes.fromhex('0420'),
              'sdmmc_set_ndma_slot(4) at game-binary/header loading; no NDMA reads in this phase')
    elif mode == 'state':
        # Checkpoint after the first patch-cache header read, before the
        # original cache-version test. It is unconditional and terminal, so
        # an existing valid cache cannot skip it and no game executes here.
        assert original[0x9a76:0x9a7a] == thumb_bl(0x9a76, 0x1fd3c)
        patch(0x9a7a, bytes.fromhex('3368792b'), thumb_bl(0x9a7a, 0x8500),
              'Terminal diagnostic checkpoint after patch-cache header read')
        # Replace pleaseWaitOutput (24 bytes) with:
        # push {r4,lr}; ldr r0,=arm9_stateFlag; ldr r4,[r0];
        # mov r0,r4; bl dbg_hexa; mov r0,r4; bl dbg_hexa; b .; .word state
        # Keep one captured state in r4 and log it twice. The second SD command
        # also publishes the first write in emulators buffering host image I/O.
        # The stack stays 8-byte aligned. This probe deliberately stops.
        before = bytes.fromhex('044b1a68032afcd103321a601a68032afcd17047d0fb3b02')
        after = (bytes.fromhex('10b5044804682000') + thumb_bl(0x8508, 0x1f1d4)
                 + bytes.fromhex('2000') + thumb_bl(0x850e, 0x1f1d4)
                 + bytes.fromhex('fee7') + struct.pack('<I', 0x023bfbd0))
        patch(0x8500, before, after,
              'Log one ARM9 state twice as eight hex digits, then stop; normal SD reads retained')
    elif mode == 'memory':
        assert payload is not None
        code, symbols = payload
        # Original BSS ends at 0x06036178; ARM7 stacks start at 0x0603FDA0.
        # The new code/records fit wholly in this unused interval. No ARM9
        # instructions, native SD routines or original globals change.
        assert len(original) == 0x33ecc and len(code) <= 0xe00
        image.extend(bytes(0x37000 + 3 * 14 * 4 - len(image)))
        image[0x36200:0x36200 + len(code)] = code
        changes.append(dict(offset='0x33ecc', before='', after=bytes(image[0x33ecc:]).hex(),
                            reason='Append ARM7 snapshot code and zero records beyond original BSS'))
        patch(0x928a, bytes.fromhex('e64bd14a'),
              thumb_bl(0x0600928a, symbols['before_clear'] & ~1),
              'Snapshot after ARM9 START wait, before ARM7 RAM clear; replay both instructions')
        patch(0x94f2, bytes.fromhex('0023704a'),
              thumb_bl(0x060094f2, symbols['after_clear'] & ~1),
              'Snapshot after ARM7 RAM clear; replay both instructions')
        assert original[0x9a76:0x9a7a] == thumb_bl(0x9a76, 0x1fd3c)
        patch(0x9a7a, bytes.fromhex('3368792b'),
              thumb_bl(0x06009a7a, symbols['finish'] & ~1),
              'Final snapshot/log after patch-cache header read, then stop')
        assert image[0x240:0xe2c] == original[0x240:0xe2c]
    else:
        raise ValueError(mode)
    assert len(image) == len(original) or mode == 'memory'
    allowed = {i for c in changes for i in range(int(c['offset'], 16), int(c['offset'], 16) + len(bytes.fromhex(c['before'])))}
    assert all(a == b or i in allowed for i, (a, b) in enumerate(zip(original, image)))
    return bytes(image), changes


def build_variant(official, mode, payload=None):
    if sha(official) != OFFICIAL_SHA256:
        raise ValueError('Input must be the exact official nds-bootstrap 2.17.0 release binary')
    entry, start, end = loader_entry(official)
    image, changes = patched_loader(decompress(official[start:end]), mode, payload)
    compressed = compress(image)
    # Frontend fread uses a 0x30000-byte compressed-image buffer.
    assert len(compressed) <= 0x30000
    result = bytearray(official)
    result.extend(bytes((-len(result)) % 512))
    new_start = len(result)
    result.extend(compressed)
    struct.pack_into('<2I', result, entry, new_start, len(result))
    struct.pack_into('<I', result, 0x80, len(result))
    struct.pack_into('<I', result, 0x210, len(result))
    while (128 * 1024 << result[0x14]) < len(result):
        result[0x14] += 1
    struct.pack_into('<H', result, 0x15e, crc16(result[:0x15e]))
    _, verify_start, verify_end = loader_entry(result)
    assert decompress(result[verify_start:verify_end]) == image
    assert crc16(result[:0x15e]) == struct.unpack_from('<H', result, 0x15e)[0]
    # Check the frontend and both DSi segments byte for byte.
    for src_offset, len_offset in ((0x20, 0x2c), (0x30, 0x3c), (0x1c0, 0x1cc), (0x1d0, 0x1dc)):
        offset, length = unpack32(official, src_offset), unpack32(official, len_offset)
        assert result[offset:offset + length] == official[offset:offset + length]
    changed_metadata = set(range(entry, entry + 8)) | set(range(0x80, 0x84)) | set(range(0x210, 0x214)) | {0x14, 0x15e, 0x15f}
    assert all(a == b or i in changed_metadata for i, (a, b) in enumerate(zip(official, result)))
    info = dict(mode=mode, upstream_source=SOURCE, input_sha256=sha(official),
                output_sha256=sha(result), loader_sha256=sha(image),
                compressed_loader_bytes=len(compressed), changes=changes)
    if mode == 'memory':
        expected_hash = 2166136261
        for (word,) in struct.iter_unpack('<I', image[0x240:0x240 + 0xbc8]):
            expected_hash = ((expected_hash ^ word) * 16777619) & 0xffffffff
        info['memory_probe'] = dict(code_base='0x06036200', records_base='0x06037000',
                                    arm9_bss='0x023bfbc8', code_bytes=0xbc8,
                                    expected_code_hash=f'{expected_hash:08X}',
                                    symbols={name: hex(payload[1][name]) for name in
                                             ('before_clear', 'after_clear', 'capture', 'finish')})
    return bytes(result), info


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('official', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--memory', action='store_true', help='Also build the r18 ARM9 memory snapshots')
    args = parser.parse_args()
    if args.official.resolve().is_relative_to(args.output.resolve()):
        parser.error('Keep the official input outside the output directory')
    official = args.official.read_bytes()
    # Validate both copies before publishing anything.
    variants = {mode: build_variant(official, mode) for mode in ('cpu', 'state')}
    if args.memory:
        variants['memory'] = build_variant(official, 'memory', memory_payload(args.output / 'probe-build'))
    args.output.mkdir(parents=True, exist_ok=True)
    for mode, (image, info) in variants.items():
        name = f'bootstrap-{mode}.nds'
        (args.output / name).write_bytes(image)
        (args.output / f'{name}.json').write_text(json.dumps(info, indent=2) + '\n')
        print(name, len(image), 'bytes;', info['compressed_loader_bytes'], 'byte loader; verified')


if __name__ == '__main__':
    main()
