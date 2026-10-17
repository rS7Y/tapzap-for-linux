"""Dedicated Pop VM lifecycle checks; host wake watcher required for suspend."""
import argparse,io,json,os,pathlib,pwd,socket,subprocess,time,urllib.request,uuid
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--directory',type=pathlib.Path,default=pathlib.Path('/home/lab/pop-worker-lifecycle'))
parser.add_argument('--binary',default='/home/lab/out/tapzap-cosmic')
parser.add_argument('--ipc-name',default='tapzap.sock')
sleep=parser.add_mutually_exclusive_group()
sleep.add_argument("--suspend",action="store_true",help="S3; requires the host wake watcher")
sleep.add_argument("--s2idle",action="store_true",help="Freeze; requires an enabled lab PS/2 keyboard and host WakeUp key")
options=parser.parse_args()
if socket.gethostname()!="tapzap-pop-vm" or pwd.getpwuid(os.getuid()).pw_name!="lab":
 raise RuntimeError("Run only as lab inside the dedicated Pop VM")
from PIL import Image
p=options.directory.resolve()
assert p.parent==pathlib.Path('/home/lab'), 'Keep evidence inside the dedicated lab home'
p.mkdir(exist_ok=False)
capture_id=uuid.uuid4().hex
capture_number=0
env=json.loads(pathlib.Path('/home/lab/pop-worker-kms/session-env.json').read_text())
env=dict(os.environ,**env,XDG_CONFIG_HOME=str(p/'config'),TAPZAP_ASSETS='/home/lab/source/tapzap/src/sway/assets',TAPZAP_DEBUG_RENDER='1')
report={'completed':False,'capture':'QEMU virtio virtual scanout for each head','physical_hardware_validated':False,'cases':[]}
baseline={}
sample_points=[(64,96),(128,160),(256,224),(128,320),(64,480)]
def qmp(command):
 with socket.create_connection(('10.0.2.2',22251),timeout=5) as s:
  f=s.makefile('rwb',buffering=0);f.readline()
  for c in [{'execute':'qmp_capabilities'},command]:
   f.write(json.dumps(c).encode()+b'\n')
   while True:
    r=json.loads(f.readline())
    if 'error' in r:raise RuntimeError(r['error'])
    if 'return' in r:break
  return r['return']
def frame(head):
 global capture_number
 capture_number+=1
 filename='resume-'+capture_id+'-'+str(capture_number)+'-'+str(head)+'.ppm'
 qmp({'execute':'screendump','arguments':{'filename':'/home/foton/TAPZAP-POP-20261017/captures/'+filename,'device':'tapzap-display','head':head}})
 # virtio-9p can leave reads blocked in the kernel across S3. Use bounded
 # read-only HTTP over the restored NIC; each completed dump has a new URL.
 with urllib.request.urlopen('http://10.0.2.2:22252/'+filename,timeout=5) as reply:
  payload=reply.read()
 with Image.open(io.BytesIO(payload)) as im:return im.convert('RGB')
def samples(im,head,red):
 return [{'point':[x,y],'expected':[baseline[head].getpixel((x,y))[0],0,0] if red else list(baseline[head].getpixel((x,y))),'actual':list(im.getpixel((x,y)))} for x,y in sample_points]
def measure(name,red,heads=(0,1),size=None,alive=True):
 deadline=time.monotonic()+20;last=[]
 while True:
  assert (gui.poll() is None) if alive else gui.returncode==0,(p/'gui.log').read_text()
  log=(p/'gui.log').read_text();assert 'Filter error:' not in log and 'gamma recovery:' not in log,log
  good=True;last=[]
  try:
   for head in heads:
    im=frame(head);area=im.crop((48,48,im.width-48,im.height-48));rgb=[e[1] for e in area.getextrema()]
    match=rgb[0]>32 and max(rgb[1:])<=2 if red else min(rgb[1:])>32
    expected_size=size if size is not None else baseline[head].size
    match=match and im.size==expected_size
    points=samples(im,head,red) if im.size==baseline[head].size else []
    match=match and all(max(abs(a-b) for a,b in zip(point['actual'],point['expected']))<=2 for point in points)
    good=good and match;last.append({'head':head,'size':list(im.size),'max_rgb':rgb,'samples':points});im.save(p/(name+'-head-'+str(head)+'.png'))
  except OSError as error:
   # The guest NIC is restored after userspace wakes. Retry capture transport,
   # while still checking the live GUI and any filter error on every iteration.
   report.setdefault('capture_transport_retries',[]).append({'case':name,'errno':error.errno})
   good=False
  if good:break
  assert time.monotonic()<deadline,(name,last)
  time.sleep(.1)
 report['cases'].append({'case':name,'heads':last,'passed':True});print(json.dumps(report['cases'][-1]),flush=True)
def randr(*args):
 return subprocess.check_output(['wlr-randr',*args],env=env,text=True,stderr=subprocess.STDOUT,timeout=10)
def ipc(command):
 with socket.socket(socket.AF_UNIX) as s:s.settimeout(2);s.connect(env['XDG_RUNTIME_DIR']+'/'+options.ipc_name);s.sendall(command.encode())
raw=randr();(p/'outputs-before.txt').write_text(raw)
outputs=sorted(line.split()[0] for line in raw.splitlines() if line and not line[0].isspace())
assert len(outputs)==2,raw
connectors=list(pathlib.Path('/sys/class/drm').glob('card*-'+outputs[1]+'/status'))
assert len(connectors)==1,connectors
secondary_status=connectors[0]
drm_device='/dev/dri/'+secondary_status.parent.name.rsplit('-',2)[0]
assert pathlib.Path(drm_device).is_char_device(),drm_device
report['drm_device']=drm_device
report['outputs']=outputs
randr('--output',outputs[0],'--pos','0,0');randr('--output',outputs[1],'--pos','1280,0')
for head in (0,1):
 baseline[head]=frame(head)
 assert min(e[1] for e in baseline[head].getextrema())>32, 'Baseline was not neutral'
 assert all(min(baseline[head].getpixel(point))>=8 for point in sample_points), 'Baseline samples need visible RGB signal'
 baseline[head].save(p/('baseline-head-'+str(head)+'.png'))
with (p/'gui.log').open('w') as log:
 gui=subprocess.Popen([options.binary,'gui','1','1','1'],env=env,stdout=log,stderr=log)
try:
 deadline=time.monotonic()+20
 while '[redraw]' not in (p/'gui.log').read_text():
  assert gui.poll() is None;assert time.monotonic()<deadline;time.sleep(.1)
 measure('both-outputs-red',True)
 randr('--output',outputs[1],'--off');(p/'secondary-off.txt').write_text(randr())
 measure('secondary-disabled-primary-red',True,heads=(0,))
 randr('--output',outputs[1],'--on');measure('secondary-enabled-red',True)
 randr('--output',outputs[0],'--mode','1024x768');measure('resized-primary-red',True,heads=(0,),size=(1024,768))
 randr('--output',outputs[0],'--mode','1280x800');measure('restored-mode-red',True,heads=(0,),size=(1280,800))
 print('WAITING-HOST-DISCONNECT',flush=True)
 deadline=time.monotonic()+90
 while secondary_status.read_text().strip()!='disconnected':
  assert time.monotonic()<deadline,'Host did not disconnect virtual monitor';time.sleep(.1)
 measure('monitor-disconnected-primary-red',True,heads=(0,))
 print('WAITING-HOST-RECONNECT',flush=True)
 deadline=time.monotonic()+90
 while secondary_status.read_text().strip()!='connected':
  assert time.monotonic()<deadline,'Host did not reconnect virtual monitor';time.sleep(.1)
 measure('monitor-reconnected-both-red',True)
 if options.suspend or options.s2idle:
  report['sleep_mode']='s2idle/freeze' if options.s2idle else 'ACPI S3/deep'
  def acpi_registers():
   values=subprocess.check_output(['sudo','setpci','-s','00:1f.0','40.l','44.b'],text=True,timeout=5).splitlines()
   return {'pmbase':int(values[0],16),'acpi_control':int(values[1],16)}
  report['acpi_before']=acpi_registers()
  assert report['acpi_before']['pmbase']&0xff80 and report['acpi_before']['acpi_control']&0x80,report['acpi_before']
  success_before=int(pathlib.Path('/sys/power/suspend_stats/success').read_text())
  trace='/sys/kernel/tracing/instances/tapzap-'+capture_id
  subprocess.run(['sudo','sh','-c','mkdir '+trace+' && echo boot > '+trace+'/trace_clock && echo 1 > '+trace+'/events/power/suspend_resume/enable && echo 1 > '+trace+'/tracing_on'],check=True,timeout=5)
  report['wake_method']='host keyboard WakeUp key' if options.s2idle else 'host QMP system_wakeup'
  if options.s2idle:
   report['keyboard_wakeup']=pathlib.Path('/sys/devices/platform/i8042/serio0/power/wakeup').read_text().strip()
   assert report['keyboard_wakeup']=='enabled',report
  print('SUSPEND-REQUESTED',flush=True);(p/'suspend-requested').write_text(str(time.time()))
  # Let the existing SSH stdout stream deliver the host coordination marker
  # before freezing SSH itself. A new SSH request cannot observe a frozen guest.
  time.sleep(1)
  wall=time.time();mono=time.monotonic();boot=time.clock_gettime(time.CLOCK_BOOTTIME)
  expected='PM: suspend entry (s2idle)' if options.s2idle else 'PM: suspend entry (deep)'
  original_mode=pathlib.Path('/sys/power/mem_sleep').read_text().split('[')[1].split(']')[0]
  selected_mode='s2idle' if options.s2idle else 'deep'
  report['mem_sleep']={'before':original_mode,'selected':selected_mode}
  def sleep_journal():
   return subprocess.check_output(['sudo','journalctl','-k','--since','@'+str(wall),'--no-pager','-o','short-monotonic'],text=True,timeout=10)
  try:
   subprocess.run(['sudo','sh','-c','echo '+selected_mode+' > /sys/power/mem_sleep'],check=True,timeout=5)
   subprocess.run(['sudo','systemctl','suspend'],check=True,timeout=30)
   # systemctl suspend only enqueues the operation. Keep the GUI alive and
   # wait for the actual kernel entry/exit before sampling a resumed frame.
   deadline=time.monotonic()+30
   while True:
    journal=sleep_journal()
    if expected in journal and 'PM: suspend exit' in journal:break
    assert time.monotonic()<deadline, 'Kernel suspend/resume did not complete'
    time.sleep(.1)
  finally:
   elapsed=time.clock_gettime(time.CLOCK_BOOTTIME)-boot
   awake=time.monotonic()-mono
   report['sleep_timing']={'boottime_seconds':elapsed,'monotonic_seconds':awake,'suspended_seconds':elapsed-awake}
   journal=sleep_journal()
   (p/'sleep-kernel.log').write_text(journal)
   subprocess.run(['sudo','sh','-c','echo 0 > '+trace+'/tracing_on'],check=True,timeout=5)
   (p/'sleep-trace.txt').write_text(subprocess.check_output(['sudo','cat',trace+'/trace'],text=True,timeout=5))
   subprocess.run(['sudo','rmdir',trace],check=True,timeout=5)
   subprocess.run(['sudo','sh','-c','echo '+original_mode+' > /sys/power/mem_sleep'],check=True,timeout=5)
   report['mem_sleep']['restored']=pathlib.Path('/sys/power/mem_sleep').read_text().strip()
  report['suspend_success_before']=success_before
  report['suspend_success_after']=int(pathlib.Path('/sys/power/suspend_stats/success').read_text())
  assert report['suspend_success_after']==success_before+1,report
  if options.s2idle:
   # No cpuidle driver: monotonic can advance while userspace is frozen.
   import re
   trace_text=(p/'sleep-trace.txt').read_text()
   events=re.findall(r'(\d+\.\d+): suspend_resume: machine_suspend\[\d+\] (begin|end)',trace_text)
   assert len(events)==2 and [event[1] for event in events]==['begin','end'],trace_text
   report['kernel_sleep_trace_seconds']=float(events[1][0])-float(events[0][0])
   assert report['kernel_sleep_trace_seconds']>=3,report
  else:
   assert report['sleep_timing']['suspended_seconds']>=3, report['sleep_timing']
  assert expected in journal and 'PM: suspend exit' in journal, journal
  report['acpi_after']=acpi_registers()
  assert report['acpi_after']==report['acpi_before'],report
  if options.suspend or options.s2idle:
   # systemd suspend legitimately locks COSMIC. Restore this lab's desktop
   # scene before comparing against its desktop pixels; do not loosen RGB gates.
   rows=subprocess.check_output(['loginctl','list-sessions','--no-legend','--no-pager'],text=True,timeout=5).splitlines()
   sessions=[row.split()[0] for row in rows if len(row.split())>=4 and row.split()[1]==str(os.getuid()) and row.split()[3]=='seat0']
   assert len(sessions)==1,rows
   subprocess.run(['sudo','loginctl','unlock-session',sessions[0]],check=True,timeout=5)
   report['lab_scene_unlock_session']=sessions[0]
  # A live ON request and its lease should survive whole-guest suspend.
  measure('resume-retains-red',True)
  with (p/'drm-queue-probe.log').open('w') as probe:
   subprocess.run(['/home/lab/stock-s3-control/drm-queue-probe',drm_device],stdout=probe,stderr=subprocess.STDOUT,check=True,timeout=10)
  assert len((p/'drm-queue-probe.log').read_text().splitlines())==256
  report['drm_allocations_after_resume']=256
  randr('--output',outputs[0],'--mode','1024x768');measure('resume-resize-red',True,heads=(0,),size=(1024,768))
  randr('--output',outputs[0],'--mode','1280x800');measure('resume-restored-mode-red',True,heads=(0,),size=(1280,800))
 ipc('off');measure('off-restores-both',False)
 ipc('on');measure('on-again-both',True)
 ipc('quit');assert gui.wait(timeout=5)==0
 measure('quit-restores-both',False,alive=False)
 report['completed']=True
finally:
 if gui.poll() is None:
  gui.kill();gui.wait()
 (p/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2),flush=True)
