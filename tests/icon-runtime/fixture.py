#!/usr/bin/env python3
"""Generate synthetic banner-only files; these fixtures are not launchable games."""
from pathlib import Path
import argparse
import struct,subprocess,shutil
p=argparse.ArgumentParser(description='Create a disposable 32 MiB DLDI homebrew-icon fixture')
p.add_argument('--source',type=Path,default=Path(__file__).resolve().parents[2])
p.add_argument('--theme',type=int,choices=[0,1,4,5],default=0)
a=p.parse_args()
root=a.source.resolve()
base=root/'build/research/icon-r24'
base.mkdir(parents=True,exist_ok=True)
stage=base/'sdroot'; dest=stage/'_nds/TWiLightMenu'; dest.mkdir(parents=True,exist_ok=True)
icons=stage/'icons';icons.mkdir(exist_ok=True)
for app,name in [('title','main.srldr'),('settings','settings.srldr'),('romsel_dsimenutheme','dsimenu.srldr')]:shutil.copy2(root/app/(app+'.nds'),dest/name)
shutil.copy2(root/'booter_fc/booter_fc.nds',stage/'BOOT.NDS')
(dest/'settings.ini').write_text('[SRLOADER]\nROM_FOLDER=fat:/icons\nSECONDARY_ROM_FOLDER=fat:/icons\nTHEME=0\nDSI_SPLASH=0\nSHOWLOGO=0\nSHOW_MAIN_MENU=0\nLANGUAGE_SET=1\nREGION_SET=1\nADDON_UPDATE_NOTICE_SHOWN=4\nLANGUAGE=1\nSHOW_BOX_ART=0\nANIMATE_DSI_ICONS=1\nDSI_MUSIC=0\n[NDS-BOOTSTRAP]\nREGION=1\n')
p=dest/'settings.ini';p.write_text(p.read_text().replace('THEME=0',f'THEME={a.theme}'))
def crc16(data):
 crc=0xffff
 for byte in data:
  crc^=byte
  for _ in range(8):crc=(crc>>1)^(0xa001 if crc&1 else 0)
 return crc
for num in range(18):
 animated=num%2!=0
 banner=bytearray(0x23c0 if animated else 0x840)
 struct.pack_into('<H',banner,0,0x103 if animated else 1)
 frames=8 if animated else 1
 for frame in range(frames):
  tile=bytearray(512)
  for y in range(32):
   for x in range(32):
    color=((x//4+y//4+num+frame)%15)+1
    idx=(y//8*4+x//8)*32+y%8*4+(x%8)//2
    tile[idx]|=color<<(4*(x%2))
  if frame==0:banner[0x20:0x220]=tile
  if animated:banner[0x1240+512*frame:0x1440+512*frame]=tile
  palette=[0]+[((i*2+num)%32)|(((i*5+frame)%32)<<5)|(((i*11+num)%32)<<10) for i in range(1,16)]
  if frame==0:struct.pack_into('<16H',banner,0x220,*palette)
  if animated:struct.pack_into('<16H',banner,0x2240+32*frame,*palette)
 for lang in range(8 if animated else 6):
  title=f'Pattern {num:02d}'.encode('utf-16le')
  banner[0x240+256*lang:0x240+256*lang+len(title)]=title
 if animated:
  struct.pack_into('<9H',banner,0x2340,*[(f<<11)|(f<<8)|1 for f in range(8)],0)
  struct.pack_into('<H',banner,8,crc16(banner[0x1240:]))
 struct.pack_into('<H',banner,2,crc16(banner[0x20:0x840]))
 header=bytearray(0x400)
 header[:12]=b'ICONPROBE   ';header[12:16]=b'####'
 struct.pack_into('<4I',header,0x20,0x200,0x02000000,0x02000000,0x80)
 struct.pack_into('<4I',header,0x30,0x300,0x02380000,0x02380000,0x80)
 struct.pack_into('<I',header,0x68,0x400)
 struct.pack_into('<4I',header,0x200,0xE3A00301,0xE5800208,0xE3A00013,0xE129F000)
 struct.pack_into('<H',header,0x15e,crc16(header[:0x15e]))
 (icons/f'{num:02d} pattern.nds').write_bytes(header+banner)
shutil.copy2(root/'7zfile/Flashcard users/_nds/GBARunner2_arm9dldi_ds.nds',icons/'18 GBARunner2.nds')
image=base/'dldi.img'
with image.open('wb') as f:f.truncate(32*1024*1024)
bin=root/'build/emulator/tools/mtools/usr/bin'
def tool(name):
 return shutil.which(name) or str(bin/name)
subprocess.run([tool('mformat'),'-i',str(image),'::'],check=True)
subprocess.run([tool('mcopy'),'-i',str(image),'-s',*[str(p) for p in stage.iterdir()],'::/'],check=True)
print(image)
