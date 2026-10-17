#!/usr/bin/env python3
"""Install the login regression fixture only in the dedicated Pop!_OS lab VM.

This seeds a separate test app config and uses the real XDG autostart helper.
The generated launcher only records timing and execs the GUI: no sleeps/retries.
"""
import argparse
import os
from pathlib import Path
import pwd
import socket


LOGIN_LAUNCHER = '''#!/usr/bin/python3
import json,os,pathlib,time
p=pathlib.Path('/home/lab/login-worker')
boot=pathlib.Path('/proc/sys/kernel/random/boot_id').read_text().strip()
out=p/'boots'/boot;out.mkdir(parents=True,exist_ok=True)
record={'boot_id':boot,'pid':os.getpid(),'parent_pid':os.getppid(),'uptime_seconds':float(pathlib.Path('/proc/uptime').read_text().split()[0]),'environment':{k:os.environ.get(k) for k in ['DISPLAY','WAYLAND_DISPLAY','XDG_RUNTIME_DIR']}}
(out/('start-'+str(os.getpid())+'.json')).write_text(json.dumps(record,indent=2))
os.environ.update(XDG_CONFIG_HOME=str(p/'config'),TAPZAP_DESKTOP_CONFIG_HOME='/home/lab/.config',TAPZAP_LAUNCHER=str(p/'launch.sh'),TAPZAP_ASSETS='/home/lab/source/tapzap/src/sway/assets',TAPZAP_DEBUG_RENDER='1')
fd=os.open(out/'gui.log',os.O_WRONLY|os.O_CREAT|os.O_APPEND,0o600)
os.dup2(fd,1);os.dup2(fd,2);os.close(fd)
os.execv('/home/lab/out/tapzap-cosmic',['tapzap-cosmic','gui'])
'''

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--install", action="store_true", required=True)
    parser.parse_args()
    if socket.gethostname() != "tapzap-pop-vm" or pwd.getpwuid(os.getuid()).pw_name != "lab":
        raise RuntimeError("Run only as lab inside the dedicated tapzap-pop-vm guest")
    if not Path("/home/lab/out/tapzap-cosmic").is_file():
        raise RuntimeError("Build the prototype in the dedicated VM first")
    import json,pathlib,subprocess
    p=pathlib.Path('/home/lab/login-worker');p.mkdir(exist_ok=True)
    wrapper=p/'launch.py'
    compile(LOGIN_LAUNCHER, str(wrapper), "exec")
    wrapper.write_text(LOGIN_LAUNCHER)
    (p/'launch.sh').write_text('#!/bin/sh\nexec /usr/bin/python3 /home/lab/login-worker/launch.py\n')
    (p/'setup.cpp').write_text('#include "platform.hpp"\nint main(){ tapzap::Config c; c.enabled=true;c.launchAtLogin=true;tapzap::saveConfig(c); return tapzap::setLaunchAtLogin(true)?0:1; }\n')
    subprocess.run(['c++','-std=c++17','-I','/home/lab/source/tapzap/src/sway',str(p/'setup.cpp'),'-o',str(p/'setup')],check=True)
    env=dict(os.environ,XDG_CONFIG_HOME=str(p/'config'),TAPZAP_DESKTOP_CONFIG_HOME='/home/lab/.config',TAPZAP_LAUNCHER=str(p/'launch.sh'))
    subprocess.run([str(p/'setup')],env=env,check=True)
    print(pathlib.Path('/home/lab/.config/autostart/tapzap.desktop').read_text())


if __name__ == "__main__":
    main()
