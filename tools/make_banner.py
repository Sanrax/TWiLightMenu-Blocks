#!/usr/bin/env python3
"""Build a DS banner from an existing 32x32, uncompressed, 4bpp BMP.

Retains the BMP's palette indices, including transparent index zero, as legacy
ndstool did. This also accepts icons that use all 16 palette entries.
"""
import argparse
import struct
from pathlib import Path


def crc16(data):
    crc = 0xffff
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ (0xa001 if crc & 1 else 0)
    return crc


def make_banner(bitmap, title):
    if len(bitmap) < 54 or bitmap[:2] != b'BM':
        raise ValueError('Expected a BMP file')
    offset, = struct.unpack_from('<I', bitmap, 10)
    header, width, height, planes, depth, compression = struct.unpack_from('<IiiHHI', bitmap, 14)
    if (header, width, abs(height), planes, depth, compression) != (40, 32, 32, 1, 4, 0):
        raise ValueError('Expected a 32x32 uncompressed 4bpp BMP with a 40-byte header')
    if offset < 54 + 64 or offset + 512 > len(bitmap):
        raise ValueError('Truncated BMP palette or pixel data')
    banner = bytearray(0x840)
    struct.pack_into('<H', banner, 0, 1)
    for y in range(32):
        row = y if height < 0 else 31 - y
        for x in range(32):
            pixel = (bitmap[offset + row * 16 + x // 2] >> (4 if x % 2 == 0 else 0)) & 15
            dest = 0x20 + (y // 8 * 4 + x // 8) * 32 + y % 8 * 4 + x % 8 // 2
            banner[dest] |= pixel << (4 if x % 2 else 0)
    for index in range(16):
        blue, green, red, _ = bitmap[54 + index * 4:58 + index * 4]
        struct.pack_into('<H', banner, 0x220 + index * 2, (red >> 3) | ((green >> 3) << 5) | ((blue >> 3) << 10))
    text = title.replace(';', '\n').encode('utf-16le')
    if len(text) > 254:
        raise ValueError('Banner title exceeds 127 UTF-16 code units')
    for language in range(6):
        start = 0x240 + language * 256
        banner[start:start + len(text)] = text
    struct.pack_into('<H', banner, 2, crc16(banner[0x20:]))
    return banner


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('bitmap', type=Path)
    parser.add_argument('title')
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    args.output.write_bytes(make_banner(args.bitmap.read_bytes(), args.title))
