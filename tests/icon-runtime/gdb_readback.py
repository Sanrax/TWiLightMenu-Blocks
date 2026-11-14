#!/usr/bin/env python3
from pathlib import Path
import socket, struct, json, argparse, subprocess
p=argparse.ArgumentParser(description='Read halted melonDS icon banks and animation palettes')
p.add_argument('--source',type=Path,default=Path(__file__).resolve().parents[2])
p.add_argument('--output',type=Path,required=True)
p.add_argument('--port',type=int,default=4333)
p.add_argument('--saturn',action='store_true')
a=p.parse_args()
root=a.source.resolve()
nm='/opt/wonderful/toolchain/gcc-arm-none-eabi/bin/arm-none-eabi-nm'
lines=subprocess.check_output([nm,'-n',str(root/'romsel_dsimenutheme/romsel_dsimenutheme.arm9.elf')],text=True)
symbols={v[2]:int(v[0],16) for line in lines.splitlines() if len(v:=line.split())==3}
s = socket.create_connection(('127.0.0.1', a.port), 2)
s.settimeout(4)
def receive():
    while True:
        byte=s.recv(1)
        if not byte: raise ConnectionError('Debugger disconnected')
        if byte==b'$': break
    data = b''
    while True:
        b = s.recv(1)
        if not b: raise ConnectionError('Debugger disconnected')
        if b == b'#': break
        data += b
    s.recv(2)
    s.sendall(b'+')
    return data.decode()
def command(text):
    data = text.encode()
    s.sendall(b'$' + data + b'#' + f'{sum(data) & 255:02x}'.encode())
    return receive()
def read(address, count):
    output = bytearray()
    for offset in range(0, count, 512):
        output += bytes.fromhex(command(f'm{address + offset:x},{min(512, count-offset):x}'))
    return bytes(output)
def write(address, data):
    assert command(f'M{address:x},{len(data):x}:' + data.hex()) == 'OK'
s.sendall(b'+')
s.sendall(b'\x03')
stop = receive()
assert stop.startswith(('S', 'T')), stop
command('g')
report = []
try:
    nesting=struct.unpack('<I',read(symbols['irq_nesting_level'],4))[0]
    assert nesting==0, 'Stopped during an IRQ; retry at a settled selection'
    pointers = struct.unpack('<8I', read(symbols['_ZL10iconPixels'], 32))
    assert all(0x06800000 <= p < 0x06880000 for p in pointers), pointers
    pending = read(symbols['_ZL12pendingValid'], 8)
    loaded = read(symbols['_ZL10bankLoaded'], 8)
    animated = read(symbols['_ZL10pendingTwl'], 8)
    tiles = read(symbols['_ZL12pendingTiles'], 8*4096)
    mapping = read(0x04000240, 4)
    try:
        write(0x04000240, b'\x80'*4)
        for index, pointer in enumerate(pointers):
            count = 4096 if animated[index] else 512
            expected = tiles[index*4096:index*4096+count]
            actual = read(pointer, count)
            errors = [i for i in range(count) if loaded[index] and expected[i] != actual[i]]
            report.append({'bank':index,'pointer':hex(pointer),'bytes':count,'loaded':bool(loaded[index]),'pending':pending[index],'mismatches':len(errors),'first_mismatch':errors[0] if errors else None})
    finally:
        write(0x04000240, mapping)
    palette_cache = read(symbols['_ZL12paletteCache'], 8*32)
    palette_pointers = struct.unpack('<8I',read(symbols['_ZL12iconPalettes'],32))
    mapping = read(0x04000244,3)
    try:
        write(0x04000244,b'\x80'*3)
        for index,pointer in enumerate(palette_pointers):
            assert 0x06880000 <= pointer < 0x06898000, hex(pointer)
            report.append({'palette_bank':index,'vram_matches_cache':read(pointer,32)==palette_cache[index*32:(index+1)*32]})
    finally:
        write(0x04000244,mapping)
    pal_lines = struct.unpack('<41i', read(symbols['bnriconPalLine'], 41*4))
    pal_loaded = struct.unpack('<41i', read(symbols['bnriconPalLoaded'], 41*4))
    is_animated = read(symbols['bnriconisDSi'], 41)
    xpos = struct.unpack('<2i', read(symbols['titleboxXpos'], 8))[1]
    centre = round(xpos/58)
    count = struct.unpack('<I', read(symbols['file_count'],4))[0]
    report.append({'scanline':struct.unpack('<H',read(0x04000006,2))[0], 'selected_slot':centre, 'irq_nesting_level':struct.unpack('<I',read(symbols['irq_nesting_level'],4))[0]})
    for slot in range(max(0,centre-3), min(centre+4,count,40)):
        if a.saturn and slot != centre: continue
        if not -64 < 96 + slot*58 - xpos < 256: continue
        if not is_animated[slot]: continue
        expected = read(symbols['bnriconTile'] + slot*0x23c0 + 0x2240 + pal_lines[slot]*32,32)
        actual = palette_cache[(slot%7)*32:(slot%7+1)*32]
        report.append({'animated_slot':slot,'selected_palette':pal_lines[slot],'loaded_flag':pal_loaded[slot],'palette_matches_frame':actual==expected})
finally:
    try: command('D')
    finally: s.close()
target = a.output
target.parent.mkdir(parents=True,exist_ok=True)
target.write_text(json.dumps(report, indent=2)+'\n')
print(json.dumps(report, indent=2))
