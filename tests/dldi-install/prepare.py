from pathlib import Path
import sys
root=Path(sys.argv[1]);out=['// Generated from bundled drivers; magic is masked to avoid DLDI patchers.']
items=[]
for i,p in enumerate(sorted((root/'title/nitrofiles/dldi').glob('*'))):
 b=bytearray(p.read_bytes())
 if p.suffix=='.lz77':
  length=int.from_bytes(b[1:4],'little');src=4;decoded=bytearray()
  while len(decoded)<length:
   flags=b[src];src+=1
   for bit in (128,64,32,16,8,4,2,1):
    if len(decoded)>=length:break
    if flags&bit:
     a,z=b[src:src+2];src+=2;off=((a&15)<<8)+z+1
     for _ in range((a>>4)+3):decoded.append(decoded[-off])
    else:decoded.append(b[src]);src+=1
  b=decoded
 b[0]^=0xFF
 out.append('static const u8 driver_%d[]={%s};'%(i,','.join(map(str,b))))
 items.append('{"%s",driver_%d,sizeof(driver_%d)}'%(p.name,i,i))
out.append('static const struct {const char*name;const u8*data;size_t size;} drivers[]={'+','.join(items)+'};')
Path('source/drivers.h').write_text('\n'.join(out)+'\n')
