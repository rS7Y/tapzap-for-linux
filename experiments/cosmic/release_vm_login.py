#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Verify the installed package's real desktop autostart, without launching it."""
import argparse,io,json,os,pathlib,pwd,socket,time,urllib.request,uuid
from PIL import Image
if socket.gethostname()!='tapzap-pop-vm' or pwd.getpwuid(os.getuid()).pw_name!='lab':
 raise RuntimeError('Run only as lab in the dedicated Pop VM')
parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--directory',type=pathlib.Path,required=True);args=parser.parse_args()
p=args.directory.resolve();assert p.parent==pathlib.Path('/home/lab');p.mkdir(exist_ok=False)
report={'completed':False,'app_launched_by_test':False,'boot_id':pathlib.Path('/proc/sys/kernel/random/boot_id').read_text().strip()}
uid=uuid.uuid4().hex;number=0;deadline=time.monotonic()+180
binary='/opt/tapzap-cosmic-prototype/libexec/tapzap-cosmic'
def wait(predicate):
 while True:
  value=predicate()
  if value:return value
  assert time.monotonic()<deadline,'Installed autostart did not reach the expected state'
  time.sleep(.1)
def find():
 found=[]
 for proc in pathlib.Path('/proc').glob('[0-9]*'):
  try:
   if proc.stat().st_uid==os.getuid() and os.readlink(proc/'exe')==binary:found.append(proc)
  except (FileNotFoundError,PermissionError):pass
 assert len(found)<=1,'More than one installed GUI'
 return found[0] if found else None
def frame():
 global number
 number+=1;name='resume-'+uid+'-'+str(number)+'-0.ppm'
 with socket.create_connection(('10.0.2.2',22251),timeout=5) as s:
  f=s.makefile('rwb',buffering=0);f.readline()
  for command in [{'execute':'qmp_capabilities'},{'execute':'screendump','arguments':{'filename':'/home/foton/TAPZAP-POP-20261017/captures/'+name,'device':'tapzap-display','head':0}}]:
   f.write(json.dumps(command).encode()+b'\n')
   while True:
    reply=json.loads(f.readline())
    if 'error' in reply:raise RuntimeError(reply['error'])
    if 'return' in reply:break
 with urllib.request.urlopen('http://10.0.2.2:22252/'+name,timeout=5) as reply:
  with Image.open(io.BytesIO(reply.read())) as im:return im.convert('RGB')
def capture(name,red):
 def good():
  im=frame();rgb=[v[1] for v in im.crop((48,48,im.width-48,im.height-48)).getextrema()]
  matches=abs(rgb[0]-178)<=2 and max(rgb[1:])<=2 if red else min(rgb)>32
  if matches:im.save(p/(name+'.png'));report[name]={'max_rgb':rgb,'passed':True};return im
 return wait(good)
def ipc(command):
 with socket.socket(socket.AF_UNIX) as s:
  s.settimeout(2);s.connect(env['XDG_RUNTIME_DIR']+'/tapzap-cosmic-prototype.sock');s.sendall(command.encode())
try:
 proc=wait(find);argv=[s.decode() for s in (proc/'cmdline').read_bytes().split(b'\0') if s]
 assert argv==[binary,'gui'],argv
 env={k.decode():v.decode() for row in (proc/'environ').read_bytes().split(b'\0') if b'=' in row for k,v in [row.split(b'=',1)] if k in [b'DISPLAY',b'WAYLAND_DISPLAY',b'XDG_RUNTIME_DIR']}
 assert len(env)==3;report['start']={'pid':int(proc.name),'argv':argv,'environment':env}
 on=capture('autostart-on',True);time.sleep(3);capture('heartbeat-retains-on',True)
 ipc('off');off=capture('off-restored',False)
 points=[(64,96),(128,160),(256,224),(128,320),(64,480)]
 assert all(min(off.getpixel(point))>=8 for point in points)
 observed=[]
 for point in points:
  expected=[off.getpixel(point)[0]*.7,0,0];actual=list(on.getpixel(point));assert max(abs(a-b) for a,b in zip(actual,expected))<=2
  observed.append({'point':list(point),'off':list(off.getpixel(point)),'expected_on':expected,'actual_on':actual})
 report['baseline_samples']=observed
 ipc('on');capture('on-again',True);ipc('quit');wait(lambda:not proc.exists());quit_image=capture('quit-restored',False)
 assert all(max(abs(a-b) for a,b in zip(quit_image.getpixel(point),off.getpixel(point)))<=2 for point in points)
 report['completed']=True
finally:
 (p/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2),flush=True)
