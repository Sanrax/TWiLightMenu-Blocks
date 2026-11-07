#!/usr/bin/env python3
"""Compact NFTR character maps without changing glyphs, widths or font settings."""
import argparse
from collections import deque
from dataclasses import dataclass
import json
import os
from pathlib import Path
import struct
import tempfile


def u16(data, at):
    return struct.unpack_from('<H', data, at)[0]


def u32(data, at):
    return struct.unpack_from('<I', data, at)[0]


@dataclass
class Font:
    data: bytes
    chunks: list
    maps: list
    glyphs: int
    pairs: list
    inverse: list
    aliases: bool

    @property
    def map_reads(self):
        calls = 1  # FINF's first-map pointer
        for offset, size in self.maps:
            first, last, kind = struct.unpack_from('<HHI', self.data, offset + 8)
            if kind == 0:
                calls += 2
            elif kind == 1:
                calls += 1 + (last - first + 256) // 256
            else:
                calls += 2 + (u16(self.data, offset + 20) + 127) // 128
        return calls


def parse_font(data):
    if (len(data) < 48 or data[:6] != b'RTFN\xff\xfe'
            or u16(data, 6) not in (0x100, 0x102) or u16(data, 12) != 16
            or u32(data, 8) != len(data)):
        raise ValueError('unsupported or truncated NFTR header')
    chunks = []
    offset = 16
    while offset < len(data):
        if offset + 8 > len(data):
            raise ValueError('truncated chunk header')
        size = u32(data, offset + 4)
        if size < 8 or size % 4 or offset + size > len(data):
            raise ValueError('invalid chunk size')
        chunks.append((offset, size, data[offset:offset + 4]))
        offset += size
    if len(chunks) != u16(data, 14) or chunks[0][2] != b'FNIF' or chunks[0][1] < 28:
        raise ValueError('invalid block count or FINF layout')
    by_offset = {offset: (size, tag) for offset, size, tag in chunks}
    glyph_offset, width_offset = u32(data, 32) - 8, u32(data, 36) - 8
    if by_offset.get(glyph_offset, (0, b''))[1] != b'PLGC' or by_offset[glyph_offset][0] < 16:
        raise ValueError('invalid glyph chunk')
    if by_offset.get(width_offset, (0, b''))[1] != b'HDWC' or by_offset[width_offset][0] < 16:
        raise ValueError('invalid width chunk')
    cell_size = u16(data, glyph_offset + 10)
    if not cell_size:
        raise ValueError('zero glyph cell size')
    glyphs = (by_offset[glyph_offset][0] - 16) // cell_size
    if not 0 < glyphs <= 65536:
        raise ValueError('invalid glyph count')
    inverse = [0xFFFF] * glyphs
    forward, assigned = {}, {}
    aliases = False

    def record(character, glyph):
        nonlocal aliases
        if glyph >= glyphs:
            raise ValueError('glyph index outside the bitmap')
        if (character in forward and forward[character] != glyph
                or glyph in assigned and assigned[glyph] != character):
            aliases = True
        forward[character] = glyph
        assigned[glyph] = character
        inverse[glyph] = character

    maps, visited = [], set()
    offset = u32(data, 40)
    while offset:
        start = offset - 8
        if offset in visited or by_offset.get(start, (0, b''))[1] != b'PAMC':
            raise ValueError('invalid or cyclic map link')
        visited.add(offset)
        size = by_offset[start][0]
        if size < 20:
            raise ValueError('truncated map header')
        first, last, kind, next_map = struct.unpack_from('<HHII', data, offset)
        if kind in (0, 1) and first > last:
            raise ValueError('reversed character range')
        if kind == 0:
            if size < 24:
                raise ValueError('truncated direct map')
            base = u16(data, start + 20)
            if base + last - first + 1 > glyphs:
                raise ValueError('direct map exceeds glyph count')
            for character in range(first, last + 1):
                record(character, base + character - first)
        elif kind == 1:
            if 20 + 2 * (last - first + 1) > size:
                raise ValueError('truncated indexed map')
            for character in range(first, last + 1):
                glyph = u16(data, start + 20 + 2 * (character - first))
                if glyph != 0xFFFF:
                    record(character, glyph)
        elif kind == 2:
            if size < 24 or 22 + 4 * u16(data, start + 20) > size:
                raise ValueError('truncated explicit map')
            for i in range(u16(data, start + 20)):
                record(*struct.unpack_from('<HH', data, start + 22 + 4 * i))
        else:
            raise ValueError('unsupported map type')
        maps.append((start, size))
        offset = next_map
    return Font(data, chunks, maps, glyphs, sorted(forward.items()), inverse, aliases)


def plan_maps(pairs, block_cost):
    """Linear-time partition into direct ranges, indexed tables and pairs.

    Charge a small fixed cost for each seek/header, alongside encoded bytes.
    Keeping minima for pair counts and each table parity avoids quadratic
    comparisons on fonts containing tens of thousands of characters.
    """
    costs = [0] * (len(pairs) + 1)
    parents = [None] * len(costs)
    explicit = deque()
    indexed = [None, None]
    direct = 0
    for i, (character, glyph) in enumerate(pairs):
        while explicit and costs[explicit[-1]] - 4 * explicit[-1] >= costs[i] - 4 * i:
            explicit.pop()
        explicit.append(i)
        while explicit[0] < i + 1 - 65535:
            explicit.popleft()
        start = explicit[0]
        best = (costs[start] + block_cost + 24 + 4 * (i - start + 1), start, 2)
        if i == 0 or pairs[i - 1] != (character - 1, glyph - 1):
            direct = i
        elif costs[i] < costs[direct]:
            direct = i
        best = min(best, (costs[direct] + block_cost + 24, direct, 0))
        # 0xFFFF denotes a missing entry in an indexed table, so the last
        # possible glyph ID must use a direct or explicit map instead.
        if (i and character == pairs[i - 1][0]) or glyph == 65535:
            indexed = [None, None]
        if glyph != 65535:
            parity = character & 1
            old = indexed[parity]
            if old is None or costs[i] - 2 * character < costs[old] - 2 * pairs[old][0]:
                indexed[parity] = i
            for start in indexed:
                if start is not None:
                    table_size = 20 + ((2 * (character - pairs[start][0] + 1) + 3) & ~3)
                    best = min(best, (costs[start] + block_cost + table_size, start, 1))
        costs[i + 1], start, kind = best
        parents[i + 1] = (start, kind)
    nodes, end = [], len(pairs)
    while end:
        start, kind = parents[end]
        nodes.append((start, end, kind))
        end = start
    return list(reversed(nodes))


def compact_font(data, block_cost=64):
    if block_cost < 0:
        raise ValueError('block cost must be nonnegative')
    font = parse_font(data)
    if not font.maps or not font.pairs or font.aliases:
        return data
    first_map = min(offset for offset, _ in font.maps)
    tail = [(offset, size) for offset, size, tag in font.chunks if offset >= first_map and tag == b'PAMC']
    # Preserve unfamiliar layouts, unlinked maps and map/bitmap interleaving.
    if (set(tail) != set(font.maps)
            or any(tag != b'PAMC' for offset, _, tag in font.chunks if offset >= first_map)):
        return data
    prefix = bytearray(data[:first_map])
    nodes = []
    for start, end, kind in plan_maps(font.pairs, block_cost):
        pairs = font.pairs[start:end]
        first, last = pairs[0][0], pairs[-1][0]
        if kind == 0:
            payload = struct.pack('<H', pairs[0][1])
        elif kind == 1:
            table = [0xFFFF] * (last - first + 1)
            for character, glyph in pairs:
                table[character - first] = glyph
            payload = struct.pack('<' + 'H' * len(table), *table)
        else:
            payload = struct.pack('<H', len(pairs)) + b''.join(struct.pack('<HH', *pair) for pair in pairs)
        size = (20 + len(payload) + 3) & ~3
        node = bytearray(struct.pack('<4sIHHII', b'PAMC', size, first, last, kind, 0) + payload)
        node.extend(b'\0' * (size - len(node)))
        nodes.append(node)
    offset = len(prefix)
    for i, node in enumerate(nodes):
        if i + 1 < len(nodes):
            struct.pack_into('<I', node, 16, offset + len(node) + 8)
        offset += len(node)
    struct.pack_into('<I', prefix, 8, offset)
    struct.pack_into('<H', prefix, 14, len(font.chunks) - len(font.maps) + len(nodes))
    struct.pack_into('<I', prefix, 40, len(prefix) + 8)
    result = bytes(prefix) + b''.join(nodes)
    if len(result) >= len(data):
        return data
    check = parse_font(result)
    if check.pairs != font.pairs or check.inverse != font.inverse:
        raise AssertionError('compaction changed character mappings')
    if check.map_reads > font.map_reads:
        return data
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    data = args.source.read_bytes()
    try:
        result = compact_font(data)
    except ValueError as error:
        parser.error(str(error))
    old, new = parse_font(data), parse_font(result)
    # Also allow explicit in-place use without truncating the original file
    # if validation or writing fails.
    with tempfile.NamedTemporaryFile(dir=args.output.parent, delete=False) as file:
        temporary = Path(file.name)
        try:
            file.write(result)
            file.close()
            temporary.chmod(args.source.stat().st_mode & 0o777)
            os.replace(temporary, args.output)
        finally:
            temporary.unlink(missing_ok=True)
    print(json.dumps({'source': str(args.source), 'bytes': [len(data), len(result)],
                      'map_blocks': [len(old.maps), len(new.maps)],
                      'map_reads': [old.map_reads, new.map_reads], 'glyphs': old.glyphs}))


if __name__ == '__main__':
    main()
