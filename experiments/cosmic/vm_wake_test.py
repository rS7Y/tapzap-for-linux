import json,pathlib,socket,time
p=pathlib.Path('/home/foton/TAPZAP-POP-20261017')
pid=int((p/'qemu.pid').read_text())
argv=pathlib.Path('/proc',str(pid),'cmdline').read_bytes().split(b'\0')
assert any(str(p/'writable.img').encode() in value for value in argv),'Not the dedicated Pop VM'
assert b'tcp:127.0.0.1:22251,server=on,wait=off' in argv
def qmp(cmd):
 with socket.create_connection(('127.0.0.1',22251),timeout=5) as s:
  f=s.makefile('rwb',buffering=0);f.readline()
  for c in [{'execute':'qmp_capabilities'},{'execute':cmd}]:
   f.write(json.dumps(c).encode()+b'\n')
   while True:
    r=json.loads(f.readline())
    if 'error' in r:raise RuntimeError(r['error'])
    if 'return' in r:break
  return r['return']
for _ in range(120):
 state=qmp('query-status')
 if state['status']=='suspended':
  start=time.time();time.sleep(5);reply=qmp('system_wakeup');result={'suspend_status':state,'host_wait_seconds':5,'suspended_at':start,'wakeup_reply':reply,'status_after':qmp('query-status')};(p/'suspend-result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result),flush=True);break
 time.sleep(1)
else:raise RuntimeError('Guest did not enter suspended state')
