"""Check NFTR repacking against original glyph/width/map identities."""
from pathlib import Path
import hashlib
import json
import random
import struct
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from compact_nftr import compact_font, parse_font, plan_maps


def font_data(glyphs, nodes):
    bitmap = bytes(i % 256 for i in range(glyphs * 4))
    glyph_chunk = struct.pack('<4sIBBHBBBB', b'PLGC', 16 + len(bitmap), 4, 4, 4, 0, 4, 2, 0) + bitmap
    widths = b'\0\4\4' * glyphs
    widths += b'\0' * (-len(widths) % 4)
    width_chunk = struct.pack('<4sIHHI', b'HDWC', 16 + len(widths), 0, glyphs - 1, 0) + widths
    info = bytearray(32)
    struct.pack_into('<4sI', info, 0, b'FNIF', 32)
    struct.pack_into('<III', info, 16, 56, 48 + len(glyph_chunk) + 8,
                     48 + len(glyph_chunk) + len(width_chunk) + 8)
    chunks = []
    at = 48 + len(glyph_chunk) + len(width_chunk)
    for i, (kind, first, last, values) in enumerate(nodes):
        payload = struct.pack('<' + 'H' * len(values), *values)
        size = (20 + len(payload) + 3) & ~3
        next_map = at + size + 8 if i + 1 < len(nodes) else 0
        chunk = struct.pack('<4sIHHII', b'PAMC', size, first, last, kind, next_map) + payload
        chunks.append(chunk + b'\0' * (size - len(chunk)))
        at += size
    header = struct.pack('<4sHHIHH', b'RTFN', 0xFEFF, 0x102, at, 16, 3 + len(nodes))
    return header + info + glyph_chunk + width_chunk + b''.join(chunks)


def preserved(test, source, result):
    old, new = parse_font(source), parse_font(result)
    test.assertEqual(old.glyphs, new.glyphs)
    test.assertEqual(old.pairs, new.pairs)
    test.assertEqual(old.inverse, new.inverse)
    old_payload = [(tag, source[at:at + size]) for at, size, tag in old.chunks if tag not in (b'FNIF', b'PAMC')]
    new_payload = [(tag, result[at:at + size]) for at, size, tag in new.chunks if tag not in (b'FNIF', b'PAMC')]
    test.assertEqual(old_payload, new_payload)
    # FINF retains its settings and glyph/width locations. Only the map link
    # and the outer size/block count can change.
    test.assertEqual(source[16:40], result[16:40])
    test.assertEqual(source[44:48], result[44:48])
    test.assertLessEqual(len(result), len(source))
    test.assertLessEqual(new.map_reads, old.map_reads)


class CompactFonts(unittest.TestCase):
    def test_all_bundled_fonts(self):
        fonts = [p for app in ('title', 'settings', 'quickmenu', 'romsel_dsimenutheme',
                               'romsel_aktheme', 'romsel_r4theme', 'manual', 'imageview')
                 for p in (ROOT / app / 'nitrofiles/graphics/font').glob('*.nftr')]
        fonts += list((ROOT / '7zfile/_nds/TWiLightMenu/extras/fonts').glob('*/*.nftr'))
        self.assertEqual(len(fonts), 19)
        for path in fonts:
            with self.subTest(font=path):
                source = path.read_bytes()
                result = compact_font(source)
                preserved(self, source, result)
                self.assertEqual(compact_font(result), result)

    def test_original_default_font_identities(self):
        goldens = json.loads((ROOT / 'tests/default-font-identities.json').read_text())
        for name, expected in goldens.items():
            data = (ROOT / '7zfile/_nds/TWiLightMenu/extras/fonts/Default' / name).read_bytes()
            font = parse_font(data)
            chunks = {tag: data[at:at + size] for at, size, tag in font.chunks if tag in (b'PLGC', b'HDWC')}
            self.assertEqual(font.glyphs, expected['glyphs'])
            self.assertEqual(hashlib.sha256(chunks[b'PLGC']).hexdigest(), expected['glyph_chunk_sha256'])
            self.assertEqual(hashlib.sha256(chunks[b'HDWC']).hexdigest(), expected['width_chunk_sha256'])
            info = next(bytearray(data[at:at + size]) for at, size, tag in font.chunks if tag == b'FNIF')
            info[24:28] = b'\0' * 4
            self.assertEqual(hashlib.sha256(info).hexdigest(), expected['settings_sha256'])
            inverse = struct.pack('<' + 'H' * len(font.inverse), *font.inverse)
            self.assertEqual(hashlib.sha256(inverse).hexdigest(), expected['map_sha256'])

    def test_mapped_and_unmapped_characters(self):
        source = font_data(32, [(2, 0, 65535, [16] + [v for i in range(16) for v in (i + 32, i)])])
        result = compact_font(source)
        preserved(self, source, result)
        self.assertLess(len(result), len(source))
        # Indexed maps with holes and nonconsecutive glyph IDs.
        source = font_data(8, [(1, 32, 39, [0, 65535, 5, 65535, 7, 4, 65535, 2])])
        preserved(self, source, compact_font(source))
        # U+FFFF is distinct from a missing entry in the forward map.
        source = font_data(2, [(2, 0, 65535, [2, 65534, 0, 65535, 1])])
        result = compact_font(source)
        preserved(self, source, result)
        self.assertIn((65535, 1), parse_font(result).pairs)

    def test_last_glyph_and_aliases(self):
        source = font_data(65536, [(0, 32, 32, [65535]), (0, 33, 33, [17])])
        result = compact_font(source)
        preserved(self, source, result)
        self.assertIn((32, 65535), parse_font(result).pairs)
        for pairs in ([2, 32, 0, 160, 0], [2, 32, 0, 32, 1]):
            source = font_data(2, [(2, 0, 65535, pairs)])
            self.assertTrue(parse_font(source).aliases)
            self.assertEqual(compact_font(source), source)

    def test_full_bmp_and_explicit_count_limit(self):
        # One explicit map cannot hold 65,536 records. The glyph at index
        # 0xFFFF also needs a direct/explicit entry, not an indexed-table hole.
        pairs = [(cp, (cp * 3) & 65535) for cp in range(65536)]
        nodes = []
        for start in (0, 32768):
            values = [32768] + [value for pair in pairs[start:start + 32768] for value in pair]
            nodes.append((2, 0, 65535, values))
        source = font_data(65536, nodes)
        result = compact_font(source)
        preserved(self, source, result)
        self.assertEqual(len(parse_font(result).pairs), 65536)
        self.assertEqual(compact_font(result), result)

    def test_bad_and_unfamiliar_layouts(self):
        source = font_data(4, [(0, 32, 35, [0])])
        for end in range(len(source)):
            with self.assertRaises(ValueError):
                compact_font(source[:end])
        for at, value in ((8, 0), (40, 0xFFFFFFFC)):
            bad = bytearray(source); struct.pack_into('<I', bad, at, value)
            with self.assertRaises(ValueError):
                compact_font(bytes(bad))
        bad = bytearray(source); start = struct.unpack_from('<I', bad, 40)[0] - 8
        struct.pack_into('<I', bad, start + 16, start + 8)
        with self.assertRaises(ValueError):
            compact_font(bytes(bad))
        # Unknown trailing chunks and unlinked maps are retained verbatim.
        data = bytearray(source + struct.pack('<4sI', b'INFO', 8))
        struct.pack_into('<I', data, 8, len(data)); struct.pack_into('<H', data, 14, 5)
        self.assertEqual(compact_font(bytes(data)), data)
        data = bytearray(font_data(4, [(0, 32, 33, [0]), (0, 34, 35, [2])]))
        first = struct.unpack_from('<I', data, 40)[0]
        struct.pack_into('<I', data, first + 8, 0)
        self.assertEqual(compact_font(bytes(data)), data)

    def test_linear_planner_against_exhaustive_partition(self):
        randomizer = random.Random(0x4E465452)
        for _ in range(400):
            count = randomizer.randrange(1, 10)
            pairs = list(zip(sorted(randomizer.sample(range(32, 90), count)), randomizer.sample(range(16), count)))
            if randomizer.randrange(4) == 0:
                pairs[randomizer.randrange(count)] = (pairs[0][0], 65535)
                pairs.sort()
            penalty = randomizer.choice((0, 16, 64, 256, 512))
            best = [0]
            for end in range(1, count + 1):
                choices = []
                for start in range(end):
                    group = pairs[start:end]
                    sizes = [24 + 4 * len(group)]
                    if all(pair == (group[0][0] + i, group[0][1] + i) for i, pair in enumerate(group)):
                        sizes.append(24)
                    if len({cp for cp, _ in group}) == len(group) and all(tile != 65535 for _, tile in group):
                        sizes.append(20 + ((2 * (group[-1][0] - group[0][0] + 1) + 3) & ~3))
                    choices.append(best[start] + penalty + min(sizes))
                best.append(min(choices))
            cost = 0
            for start, end, kind in plan_maps(pairs, penalty):
                size = 24 if kind == 0 else 24 + 4 * (end - start) if kind == 2 else 20 + ((2 * (pairs[end - 1][0] - pairs[start][0] + 1) + 3) & ~3)
                cost += penalty + size
            self.assertEqual(cost, best[-1])


if __name__ == '__main__':
    unittest.main()
