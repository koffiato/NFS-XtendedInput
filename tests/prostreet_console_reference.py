"""Bounded interpreter for the original ProStreet Xbox effect callbacks and sampler.
Reads the user's local expanded image; emits numeric test vectors only.
This checks a static reconstruction, not physical console output.
"""
import struct,math,re,random
from pathlib import Path
from capstone import Cs,CS_ARCH_PPC,CS_MODE_32,CS_MODE_BIG_ENDIAN
BASE=0x82000000
def f32(v):return struct.unpack('>f',struct.pack('>f',v))[0]
class Reference:
 def __init__(self,image):
  self.mem=bytearray(image.read_bytes())
  self.stack=bytearray(65536)
  cs=Cs(CS_ARCH_PPC,CS_MODE_32|CS_MODE_BIG_ENDIAN)
  self.code={}
  for a,n in [(0x825A75D8,0x108),(0x825A8CE0,0x410),(0x825A9F18,0x7F0),(0x825AA710,0x478),(0x825B68F0,0x88),(0x82141608,0x50),(0x825736A0,0x48)]:
   for pc in range(a,a+n,4):
    for i in cs.disasm(bytes(self.mem[pc-BASE:pc-BASE+4]),pc):self.code[i.address]=(i.mnemonic,[x.strip() for x in i.op_str.split(',') if x.strip()])
  self.hooks={}
  self.hooks[0x82a21be8]=lambda cpu:setattr(cpu,'f',cpu.f[:1]+[math.sin(cpu.f[1])]+cpu.f[2:])
  self.hooks[0x82a21fb0]=lambda cpu:setattr(cpu,'f',cpu.f[:1]+[math.pow(cpu.f[1],cpu.f[2])]+cpu.f[2:])
 def read(self,a,n):
  return bytes(self.mem[a-BASE:a-BASE+n] if a>=BASE else self.stack[a-0x100000:a-0x100000+n])
 def write(self,a,b):
  if a>=BASE:self.mem[a-BASE:a-BASE+len(b)]=b
  else:self.stack[a-0x100000:a-0x100000+len(b)]=b
 def execute(self,start,registers=None,floats=None):
  self.r=[0]*32;self.f=[0.0]*32;self.cr={};self.lr=0
  self.r[1]=0x108000;self.ctr=0;self.frames=[]
  for k,v in (registers or {}).items():self.r[k]=v
  for k,v in (floats or {}).items():self.f[k]=v
  pc=start
  def reg(s):return int(s[1:])
  def ea(s):
   m=re.fullmatch(r'(-?(?:0x[\da-f]+|\d+))\(r(\d+)\)',s)
   assert m,s
   return (self.r[int(m[2])]+int(m[1],0))&0xffffffff
  def signed(v):return (v&0xffffffff)-0x100000000 if v&0x80000000 else v&0xffffffff
  for step in range(15000):
   if pc==0:return
   if pc in self.hooks:self.hooks[pc](self);pc=self.lr;continue
   if pc in (0x82a280d8,0x82a280dc):
    self.frames.append((self.r[12],self.r[26:32].copy()));pc=self.lr;continue
   if pc in (0x82a28128,0x82a2812c):
    pc,saved=self.frames.pop();self.r[26:32]=saved;continue
   if pc in (0x82a28788,):pc=self.lr;continue
   if pc in (0x82a2873c,):pc=self.lr;continue
   if pc not in self.code:raise ValueError(hex(pc))
   op,a=self.code[pc];nextpc=pc+4
   if op=='mflr':self.r[reg(a[0])]=self.lr
   elif op=='mtlr':self.lr=self.r[reg(a[0])]
   elif op=='mtctr':self.ctr=self.r[reg(a[0])]
   elif op=='bctrl':self.lr=nextpc;nextpc=self.ctr
   elif op in ('li','lis'):self.r[reg(a[0])]=(int(a[1],0)*(65536 if op=='lis' else 1))&0xffffffff
   elif op=='mr':self.r[reg(a[0])]=self.r[reg(a[1])]
   elif op=='addi':self.r[reg(a[0])]=(self.r[reg(a[1])]+int(a[2],0))&0xffffffff
   elif op in ('add','mulli','ori','xor','xori','clrlwi','srawi'):
    x=self.r[reg(a[1])]
    y=self.r[reg(a[2])] if op in ('add','xor') else int(a[2],0)
    v={'add':lambda:x+y,'mulli':lambda:signed(x)*y,'ori':lambda:x|y,'xor':lambda:x^y,
       'xori':lambda:x^y,
       'clrlwi':lambda:x&((1<<(32-y))-1),'srawi':lambda:signed(x)>>y}[op]()
    self.r[reg(a[0])]=v&0xffffffff
   elif op=='mfcr':
    value=0
    for name,c in self.cr.items():
     n=int(name[2:]);bits=1 if c is None else 8 if c<0 else 4 if c>0 else 2
     value|=bits<<(28-n*4)
    self.r[reg(a[0])]=value
   elif op=='cntlzw':
    self.r[reg(a[0])]=32-self.r[reg(a[1])].bit_length()
   elif op=='rlwinm':
    x=self.r[reg(a[1])];shift,mb,me=map(lambda x:int(x,0),a[2:]);v=((x<<shift)|(x>>(32-shift)))&0xffffffff
    mask=sum(1<<(31-i) for i in range(32) if (mb<=i<=me if mb<=me else i>=mb or i<=me))
    self.r[reg(a[0])]=v&mask
   elif op=='lfsx':self.f[reg(a[0])]=struct.unpack('>f',self.read((self.r[reg(a[1])]+self.r[reg(a[2])])&0xffffffff,4))[0]
   elif op=='fneg':self.f[reg(a[0])]=-self.f[reg(a[1])]
   elif op=='fsel':self.f[reg(a[0])]=self.f[reg(a[2])] if self.f[reg(a[1])]>=0 else self.f[reg(a[3])]
   elif op in ('lwz','lbz','ld'):
    self.r[reg(a[0])]=int.from_bytes(self.read(ea(a[1]),{'lwz':4,'lbz':1,'ld':8}[op]),'big')
   elif op in ('stw','stwu','std','stb','stbx'):
    at=(self.r[reg(a[1])]+self.r[reg(a[2])])&0xffffffff if op=='stbx' else ea(a[1]);size=8 if op=='std' else 1 if op in ('stb','stbx') else 4
    self.write(at,(self.r[reg(a[0])] & ((1<<(size*8))-1)).to_bytes(size,'big'))
    if op=='stwu':self.r[1]=at
   elif op in ('lfs','lfd'):self.f[reg(a[0])]=struct.unpack('>f' if op=='lfs' else '>d',self.read(ea(a[1]),4 if op=='lfs' else 8))[0]
   elif op in ('stfs','stfd','stfsx'):
    at=(self.r[reg(a[1])]+self.r[reg(a[2])])&0xffffffff if op=='stfsx' else ea(a[1])
    self.write(at,struct.pack('>d' if op=='stfd' else '>f',self.f[reg(a[0])]))
   elif op in ('fcmpu','cmpwi','cmplwi','cmpw'):
    x=self.f[reg(a[1])] if op=='fcmpu' else self.r[reg(a[1])]
    y=self.f[reg(a[2])] if op=='fcmpu' else self.r[reg(a[2])] if op=='cmpw' else int(a[2],0)
    if op in ('cmpwi','cmpw'):x=signed(x);y=signed(y)
    self.cr[a[0]]=None if math.isnan(x) or math.isnan(y) else (x>y)-(x<y)
   elif op in ('fmr','frsp'):
    v=self.f[reg(a[1])];self.f[reg(a[0])]=f32(v) if op=='frsp' else v
   elif op in ('fadds','fsubs','fmuls','fdivs','fmadds','fnmsubs'):
    x,y=self.f[reg(a[1])],self.f[reg(a[2])]
    if op=='fadds':v=x+y
    elif op=='fsubs':v=x-y
    elif op=='fmuls':v=x*y
    elif op=='fdivs':v=x/y
    else:
     z=self.f[reg(a[3])]
     v=x*y+z if op=='fmadds' else z-x*y
    self.f[reg(a[0])]=f32(v)
   elif op=='fsqrts':self.f[reg(a[0])]=f32(math.sqrt(self.f[reg(a[1])]))
   elif op=='bl':self.lr=nextpc;nextpc=int(a[0],0)
   elif op=='blr':nextpc=self.lr
   elif op=='b':nextpc=int(a[0],0)
   elif op in ('ble','bgt','blt','bge','bso','beq','bne','beqlr','bgelr','blelr','bnelr'):
    c=self.cr[a[0]]
    branch=op[:-2] if op.endswith('lr') else op
    yes={'ble':c is not None and c<=0,'bgt':c is not None and c>0,'blt':c is not None and c<0,'bge':c is not None and c>=0,'bso':c is None,'beq':c==0,'bne':c!=0}[branch]
    if yes:nextpc=self.lr if op.endswith('lr') else int(a[1],0)
   else:raise ValueError((hex(pc),op,a))
   pc=nextpc
  raise RuntimeError('instruction bound exceeded')


def main():
 import argparse,subprocess,hashlib
 repo=Path(__file__).resolve().parents[1]
 parser=argparse.ArgumentParser(description=__doc__)
 parser.add_argument('image',type=Path,help='Locally expanded ProStreet Xbox 360 default.exe')
 parser.add_argument('--adapter',type=Path,default=repo/'build/tests/prostreet.exe')
 args=parser.parse_args()
 if hashlib.sha256(args.image.read_bytes()).hexdigest()!='49692cddb8a8f666c4f1acec3f8c7df933298d538acb480f2a1f9d877b6d37bd':
  raise ValueError('Unsupported Xbox image: use the expanded retail image documented in docs/ps-rumble-x360.md')
 rng=random.Random(45410822);ref=Reference(args.image)
 state=0x82B97D00;device=0x82BA002C;surface=0x82BA1000;packet=0x82BA1100;zero=0x82BA1200
 threshold=0x82BA1300;gain=threshold+4
 ref.write(device-8,struct.pack('>I',0));ref.write(surface,struct.pack('>I',surface+4))
 ref.write(packet,struct.pack('>8f',0,0,0,0,1,1,1,1))
 ref.write(zero,struct.pack('>8f',0,0,0,0,0,0,0,1))
 def attribute(cpu):
  cpu.r[3]=threshold if cpu.r[4]==0xE289A6B0 else gain if cpu.r[4]==0x2D1022A0 else packet if cpu.r[5]==0 else zero
 ref.hooks[0x8234D918]=attribute
 rows=[]
 def reset():
  b=bytearray(0x254);b[0x250]=1;struct.pack_into('>f',b,0x248,1);ref.write(state,b)
 callbacks=[('R',0x825A9FA0,1),('S',0x825AA1E0,2),('K',0x825AA098,4),('D',0x825AA710,2),('F',0x825AA870,3)]
 for kind,entry,n in callbacks:
  cases=[[f32(rng.uniform(-1,2)) for _ in range(n)] for i in range(200)]
  if kind=='K':cases=[[f32(rng.uniform(-.5,3.2)),f32(rng.uniform(.01,.4)),f32(rng.uniform(.1,.6)),f32(rng.random())] for i in range(200)]
  if kind=='F':cases=[[f32(rng.uniform(-3.2,3.2)),f32(rng.uniform(.01,.4)),f32(rng.uniform(.1,.6))] for i in range(200)]
  for values in cases:
   reset();registers={3:device,4:1,5:surface} if kind=='R' else {3:device,4:surface}
   ref.execute(entry,registers,{i+1:v for i,v in enumerate(values)})
   expected=struct.unpack('>f',ref.read(state+(8 if kind=='R' else 0x88 if kind in ('S','D') else 0x48),4))[0]
   rows.append(' '.join(map(str,[kind,*values,expected])))
 # Car callbacks were not covered by the original isolated tyre/bump vectors.
 # Seed previous event state and active ramp contents, including paused calls.
 for slot,entry in [(10,0x825AA310),(11,0x825AA568),(12,0x825AA4B0),(13,0x825AA620)]:
  for i in range(200):
   reset();enabled=rng.randrange(2);previous=rng.randrange(7 if slot==10 else 2)
   value=rng.randrange(7 if slot==10 else 2);before=[]
   ref.write(state+0x250,bytes([enabled]));ref.write(device+0x738,bytes(8))
   ref.write(device+(0x73C if slot==10 else {11:0x738,12:0x739,13:0x73A}[slot]),
             struct.pack('>I',previous) if slot==10 else bytes([previous]))
   for ramp in range(12):
    duration=f32(rng.uniform(.01,3))
    fields=[f32(rng.random()*duration),duration,f32(rng.random()),f32(rng.random())] if ramp not in (1,5) else [0,0,0,0]
    before+=fields;ref.write(state+0x180+ramp*16,struct.pack('>4f',*fields))
   ref.execute(entry,{3:device,4:value})
   after=struct.unpack('>48f',ref.read(state+0x180,0xC0))
   offset=0x73C if slot==10 else {11:0x738,12:0x739,13:0x73A}[slot]
   cached=int.from_bytes(ref.read(device+offset,4 if slot==10 else 1),'big')
   rows.append(' '.join(map(str,['C',slot,enabled,previous,value,*before,*after,cached])))
 for i in range(200):
  reset();wheel=rng.randrange(4);signal=f32(rng.uniform(0,2));lo=f32(rng.uniform(0,.8));scale=f32(rng.uniform(0,2))
  ref.write(threshold,struct.pack('>2f',lo,scale))
  ref.execute(0x825AAA18,{3:device,4:wheel,6:surface},{1:signal})
  values=struct.unpack('>4f',ref.read(state+0x220+(wheel%2)*16,16))
  rows.append(' '.join(map(str,['B',wheel,signal,lo,scale,*values])))
 for i in range(500):
  reset();waves=[];ramps=[]
  for w in range(24):
   wave=[f32(rng.random()),f32(rng.random()),f32(rng.uniform(0,15)),f32(rng.uniform(-.5,.7))];waves+=wave
   ref.write(state+w*16,struct.pack('>4f',*wave))
  for r in range(12):
   dur=f32(rng.uniform(.01,3));ramp=[f32(rng.random()*dur),dur,f32(rng.random()),f32(rng.random())]
   if r in (1,5):ramp=[0,0,0,0] # No retail callback writes these right-hand ramps.
   ramps+=ramp
   ref.write(state+0x180+r*16,struct.pack('>4f',*ramp))
  dt=f32(rng.uniform(.001,.04));ref.execute(0x825A8CE0,{3:state},{1:dt*1000})
  out=struct.unpack('>2f',ref.read(state+0x240,8))
  rows.append(' '.join(map(str,['M',dt,*waves,*ramps,*out])))
 path=repo/'build/tests/ps-console-reference-vectors.txt';path.write_text('\n'.join(rows)+'\n')
 print(f'Executed {len(rows)} original Xbox 360 callback/mixer cases; comparing compiled adapter',flush=True)
 subprocess.run([str(args.adapter.resolve()),'--console-reference',str(path.resolve())],check=True,cwd=repo)
 print('Sine and pow use host math; hardware output and Xbox surface assets are not modeled.')

if __name__=='__main__':main()
