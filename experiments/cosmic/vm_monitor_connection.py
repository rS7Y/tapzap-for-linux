"""QEMU 10.2.2 lab-only monitor event through the normal display UI API.

GDB calls dpy_set_ui_info; no compositor memory or RGB values are modified.
The QEMUUIInfo layout is from the exact QEMU 10.2.2 public console.h.
"""
import argparse,json,pathlib,re,subprocess,tempfile,time
p=pathlib.Path('/home/foton/TAPZAP-POP-20261017')
parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('state',choices=['connected','disconnected']);a=parser.parse_args()
pid=int((p/'qemu.pid').read_text());argv=pathlib.Path('/proc',str(pid),'cmdline').read_bytes().split(b'\0')
assert any(str(p/'writable.img').encode() in x for x in argv), 'Not the dedicated Pop VM'
assert b'tcp:127.0.0.1:22251,server=on,wait=off' in argv
executable=pathlib.Path('/proc',str(pid),'exe').resolve()
assert executable.name=='qemu-system-x86_64',executable
assert re.search(r'\bversion 10\.2\.2(?:\s|$)',subprocess.check_output([str(executable),'--version'],text=True))
w,h=(1280,800) if a.state=='connected' else (0,0)
commands=f'''set pagination off
set confirm off
set $tz_info = (char *) calloc(1, 24)
set *(unsigned short *) ($tz_info + 0) = {320 if w else 0}
set *(unsigned short *) ($tz_info + 2) = {200 if h else 0}
set *(int *) ($tz_info + 4) = {w}
set *(unsigned int *) ($tz_info + 12) = {w}
set *(unsigned int *) ($tz_info + 16) = {h}
set *(unsigned int *) ($tz_info + 20) = {60000 if w else 0}
set $tz_head = (void *) qemu_console_lookup_by_index(1)
call (int) dpy_set_ui_info($tz_head, $tz_info, 0)
call (void) free($tz_info)
detach
'''
with tempfile.NamedTemporaryFile('w',suffix='.gdb',dir=p) as f:
 f.write(commands);f.flush();start=time.monotonic()
 r=subprocess.run(['gdb','-nx','-batch','-iex','set debuginfod enabled off','-iex','set auto-solib-add off','-p',str(pid),'-x',f.name],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=15)
 elapsed=time.monotonic()-start
(p/('monitor-'+a.state+'.log')).write_text(r.stdout)
assert r.returncode==0 and '$1 = 0' in r.stdout,r.stdout
result={'state':a.state,'api':'QEMU dpy_set_ui_info on head 1 via GDB','width':w,'height':h,'debugger_elapsed_seconds':round(elapsed,3),'completed':True}
(p/('monitor-'+a.state+'.json')).write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result),flush=True)
