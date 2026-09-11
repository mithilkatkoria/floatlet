from pathlib import Path
import struct, math
root=Path(__file__).resolve().parents[1]
# An original capsule mark, antialiased at 4x, in multiple native icon sizes.
frames=[]
for n in [16,20,24,32,40,48,64,128,256]:
 pixels=bytearray()
 for y in range(n-1,-1,-1):
  for x in range(n):
   rgba=[0,0,0,0]
   for sy in range(4):
    for sx in range(4):
     u=(x+(sx+.5)/4)/n;v=(y+(sy+.5)/4)/n
     inside=(max(abs(u-.5)-.27,0)**2+max(abs(v-.5)-.27,0)**2)<.20**2
     capsule=(max(abs(u-.5)-.19,0)**2+(v-.5)**2)<.17**2
     dot=(u-.7)**2+(v-.5)**2<.065**2
     c=(243,242,240,255) if capsule and not dot else (15,16,19,255) if inside else (0,0,0,0)
     for k in range(4):rgba[k]+=c[k]
   pixels+=bytes([round(rgba[2]/16),round(rgba[1]/16),round(rgba[0]/16),round(rgba[3]/16)])
 mask=bytes(((n+31)//32)*4*n)
 frames.append((n,struct.pack('<IIIHHIIIIII',40,n,n*2,1,32,0,len(pixels),0,0,0,0)+pixels+mask))
offset=6+16*len(frames);directory=bytearray();data=bytearray()
for n,frame in frames:
 directory+=struct.pack('<BBBBHHII',n%256,n%256,0,0,1,32,len(frame),offset);data+=frame;offset+=len(frame)
(root/'src/app/island.ico').write_bytes(struct.pack('<HHH',0,1,len(frames))+directory+data)
(root/'src/app/resources.rc').write_text('''#include <windows.h>
101 ICON "island.ico"
1 VERSIONINFO
FILEVERSION 0,4,0,0
PRODUCTVERSION 0,4,0,0
FILEOS VOS_NT_WINDOWS32
FILETYPE VFT_APP
BEGIN
 BLOCK "StringFileInfo"
 BEGIN
  BLOCK "040904b0"
  BEGIN
   VALUE "FileDescription", "Floatlet\\0"
   VALUE "ProductName", "Floatlet\\0"
   VALUE "FileVersion", "0.4.0\\0"
   VALUE "ProductVersion", "0.4.0\\0"
   VALUE "OriginalFilename", "Floatlet.exe\\0"
  END
 END
 BLOCK "VarFileInfo"
 BEGIN
  VALUE "Translation", 0x409, 1200
 END
END
''',encoding='utf-8')
