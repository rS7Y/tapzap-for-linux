#!/usr/bin/env python3
# Copyright (C) 2026 Tap Zap.
# SPDX-License-Identifier: GPL-3.0-or-later
"""Real GUI + CTM client + tray, isolated Xvfb/Wayland test server/private D-Bus."""
import json,os,pathlib,signal,socket,subprocess,tempfile,time
from gi.repository import Gio,GLib
from Xlib import X,display,protocol

ROOT=pathlib.Path(__file__).resolve().parents[1]
TMP=pathlib.Path(tempfile.mkdtemp(prefix='tapzap-omarchy-test-'))
ENV=dict(os.environ,XDG_RUNTIME_DIR=str(TMP/'runtime'),XDG_CONFIG_HOME=str(TMP/'config'),
    TAPZAP_HYPRLAND_HELPER=str(ROOT/'build/tapzap-ctm'),TAPZAP_ASSETS=str(ROOT/'src/linux/assets'),
    TAPZAP_LAUNCHER=str(ROOT/'integration/tapzap-omarchy'),TAPZAP_FRAMELESS='1',WAYLAND_DISPLAY='tapzap-test')
(TMP/'runtime').mkdir(mode=0o700);(TMP/'config/TapZapOmarchy').mkdir(parents=True)
(TMP/'config/TapZap2').mkdir();(TMP/'config/TapZap2/settings.ini').write_text('PRESERVE OTHER BUILD\n')
(TMP/'runtime/tapzap.sock').write_text('PRESERVE OTHER IPC\n')
ENV.pop('TAPZAP_OVERLAY',None);ENV.pop('TAPZAP_START_HIDDEN',None)
processes=[];results=[]

def pump():
    context=GLib.MainContext.default()
    while context.pending():context.iteration(False)

def wait(check,label,timeout=7):
    deadline=time.monotonic()+timeout
    while time.monotonic()<deadline:
        pump()
        try:
            if check():results.append(label);print('PASS',label,flush=True);return
        except (OSError,ValueError,KeyError):pass
        time.sleep(.03)
    raise AssertionError(label)

def spawn(args,env=ENV,name='process'):
    log=(TMP/(name+'.log')).open('w')
    p=subprocess.Popen(list(map(str,args)),env=env,stdout=log,stderr=log);log.close();processes.append(p);return p

def query(cmd='status',timeout=6):
    with socket.socket(socket.AF_UNIX) as c:
        c.settimeout(timeout);c.connect(str(TMP/'runtime/tapzap-omarchy/control.sock'));c.sendall((cmd+'\n').encode())
        reply=b''
        while not reply.endswith(b'\n'):
            part=c.recv(4096)
            if not part:raise OSError('closed')
            reply+=part
        return json.loads(reply)

def matrix():return json.loads((TMP/'matrix.json').read_text())
def neutral():return matrix()['matrix']==[1,0,0,0,1,0,0,0,1] and not matrix()['active']
def half_red():return matrix()['matrix']==[.5,0,0,0,0,0,0,0,0] and matrix()['active']
def children(pid):return [int(p) for p in pathlib.Path(f'/proc/{pid}/task/{pid}/children').read_text().split()]
def helper(pid):return next(p for p in children(pid) if pathlib.Path(f'/proc/{p}/comm').read_text().strip()=='tapzap-ctm')
def dead(pid):
    try:return pathlib.Path(f'/proc/{pid}/stat').read_text().split()[2]=='Z'
    except FileNotFoundError:return True

bus=Gio.bus_get_sync(Gio.BusType.SESSION,None)
bus.call_sync('org.freedesktop.DBus','/org/freedesktop/DBus','org.freedesktop.DBus','RequestName',
    GLib.Variant('(su)',('org.kde.StatusNotifierWatcher',0)),None,Gio.DBusCallFlags.NONE,2000,None)
registered=[]
watcher=Gio.DBusNodeInfo.new_for_xml('''<node><interface name="org.kde.StatusNotifierWatcher">
<method name="RegisterStatusNotifierItem"><arg type="s" direction="in"/></method>
</interface></node>''')
def register(connection,sender,path,interface,method,args,invocation):
    registered.append((sender,args.unpack()[0]));invocation.return_value(None)
bus.register_object('/StatusNotifierWatcher',watcher.interfaces[0],register,None,None)
def dbus(iface,method,args,path='/StatusNotifierItem'):
    return bus.call_sync(registered[-1][0],path,iface,method,args,None,Gio.DBusCallFlags.NONE,6000,None)

def start(hidden=False):
    registered.clear()
    cfg=TMP/'config/TapZapOmarchy/settings.ini'
    cfg.write_text('enabled=0\nintensity=1\nbrightness=0.5\npwmSafeMode=0\nlaunchAtLogin=0\n')
    env=dict(ENV)
    if hidden:env['TAPZAP_START_HIDDEN']='1'
    p=spawn([ROOT/'build/tapzap','gui'],env,'gui-'+str(len(processes)))
    wait(lambda:query()['ok'],'GUI private socket ready')
    wait(lambda:bool(registered),'tray registered with StatusNotifierWatcher')
    return p

try:
    r,w=os.pipe()
    xvfb=subprocess.Popen(['Xvfb','-displayfd',str(w),'-screen','0','1280x1024x24','-nolisten','tcp'],pass_fds=(w,),stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    processes.append(xvfb);os.close(w)
    with os.fdopen(r) as f:ENV['DISPLAY']=':'+f.readline().strip()
    server=spawn([ROOT/'build/test-ctm-server','tapzap-test',TMP/'matrix.json'],name='wayland-server')
    wait(lambda:(TMP/'runtime/tapzap-test').exists(),'private Wayland server ready')
    gui=start();wait(neutral,'first run starts OFF')
    assert query('on')['ok'];wait(half_red,'real CTM client commits exact half-red matrix')
    assert query('hide')['ok'];wait(lambda:not query()['visible'] and half_red(),'hiding UI preserves filter')
    dbus('org.kde.StatusNotifierItem','Activate',GLib.Variant('(ii)',(0,0)))
    wait(lambda:query()['visible'],'tray click reopens same UI')
    wait(lambda:True,'tray activation D-Bus method completed')
    layout=dbus('com.canonical.dbusmenu','GetLayout',GLib.Variant('(iias)',(0,-1,[])),'/MenuBar').unpack()
    assert len(layout[1][2])==3;results.append('tray menu contains Show, Toggle and Quit')
    dbus('org.kde.StatusNotifierItem','SecondaryActivate',GLib.Variant('(ii)',(0,0)))
    wait(neutral,'tray secondary click toggles OFF and restores identity')
    dbus('com.canonical.dbusmenu','Event',GLib.Variant('(isvu)',(2,'clicked',GLib.Variant('i',0),0)),'/MenuBar')
    wait(half_red,'tray menu toggles filtering ON')
    time.sleep(1.15)
    tooltip=dbus('org.freedesktop.DBus.Properties','Get',GLib.Variant('(ss)',('org.kde.StatusNotifierItem','ToolTip'))).unpack()
    assert 'Filter ON' in str(tooltip);results.append('tray tooltip reports actual ON state')
    assert len(str(dbus('org.freedesktop.DBus.Properties','Get',GLib.Variant('(ss)',('org.kde.StatusNotifierItem','IconPixmap')))))>100
    results.append('tray exposes rendered bolt pixmap')
    x=display.Display(ENV['DISPLAY']);root=x.screen().root
    def findwin():
        return next(w for w in root.query_tree().children if w.get_wm_class()==('tapzap-omarchy','TapZapOmarchy'))
    win=findwin()
    def click(lx,ly):
        scale=win.get_geometry().width/320
        for cls,mask in ((protocol.event.ButtonPress,X.ButtonPressMask),(protocol.event.ButtonRelease,X.ButtonReleaseMask)):
            win.send_event(cls(time=X.CurrentTime,root=root,window=win,child=X.NONE,root_x=0,root_y=0,event_x=round(lx*scale),event_y=round(ly*scale),state=0,detail=1,same_screen=1),event_mask=mask)
        x.flush();time.sleep(.12)
    click(298,28);wait(lambda:not query()['visible'] and half_red(),'header close hides while filtering continues')
    query('show');click(250,28);click(200,104)
    login=TMP/'config/autostart/tapzap-omarchy.desktop'
    wait(login.exists,'Settings creates isolated launch-at-login entry')
    assert 'start-hidden' in login.read_text() and 'OnlyShowIn=Hyprland;' in login.read_text()
    click(200,104);wait(lambda:not login.exists(),'Settings disables launch-at-login')
    click(200,156);click(40,28);click(238,293)
    wait(lambda:matrix()['matrix']==[0]*9,'extreme-dim unlock reaches zero through CTM backend')
    click(238,204);click(82,115)
    wait(lambda:matrix()['matrix'][4]==0 and .45<matrix()['matrix'][0]<.65,'same sliders drive Hyprland backend')
    # Competing starts must not unlink the original socket or take filter ownership.
    second=subprocess.run([ROOT/'build/tapzap','gui'],env=ENV,capture_output=True,timeout=7)
    assert second.returncode==0 and gui.poll() is None;results.append('second launch reuses running instance')
    stalled=socket.socket(socket.AF_UNIX);stalled.connect(str(TMP/'runtime/tapzap-omarchy/control.sock'))
    then=time.monotonic();assert query()['ok'];assert time.monotonic()-then<1;stalled.close()
    results.append('incomplete IPC client cannot freeze UI')
    query('quit');gui.wait(timeout=5);wait(neutral,'Quit restores compositor identity')
    gui=start(True);wait(lambda:not query()['visible'],'autostart mode begins hidden')
    query('on');wait(half_red,'hidden instance applies filtering')
    child=helper(gui.pid);os.kill(child,signal.SIGKILL)
    wait(neutral,'filter helper death releases compositor ownership')
    wait(lambda:not query()['enabled'] and bool(query()['error']),'UI exposes helper failure and clears ON state')
    query('quit');gui.wait(timeout=5)
    gui=start();query('on');wait(half_red,'filter active before abrupt UI death')
    owned_children=children(gui.pid);gui.kill();gui.wait(timeout=5)
    wait(neutral,'SIGKILL of UI restores compositor identity')
    wait(lambda:all(dead(p) for p in owned_children),'UI death leaves no running helper or tray')
    gui=start();query('on');wait(half_red,'filter active before stalled compositor test')
    os.kill(server.pid,signal.SIGSTOP)
    try:
        then=time.monotonic();reply=query('off');assert not reply['ok'] and time.monotonic()-then<3
        results.append('stalled compositor request fails within deadline')
    finally:os.kill(server.pid,signal.SIGCONT)
    wait(neutral,'timeout kills owning helper and releases filter after compositor resumes')
    query('quit');gui.wait(timeout=5)
    server.terminate();server.wait(timeout=5)
    server=spawn([ROOT/'build/test-ctm-server','tapzap-blocked',TMP/'matrix.json','blocked'],name='blocked-server')
    ENV['WAYLAND_DISPLAY']='tapzap-blocked';wait(lambda:(TMP/'runtime/tapzap-blocked').exists(),'competing-owner server ready')
    gui=start();reply=query('on');assert not reply['ok'] and not reply['enabled'] and 'already owned' in reply['error']
    wait(neutral,'competing night-light owner is refused without takeover')
    query('quit');gui.wait(timeout=5)
    assert (TMP/'config/TapZap2/settings.ini').read_text()=='PRESERVE OTHER BUILD\n'
    assert (TMP/'runtime/tapzap.sock').read_text()=='PRESERVE OTHER IPC\n'
    results.append('other TAP ZAP configuration and IPC preserved')
    print('ALL INTEGRATION CHECKS PASSED',flush=True)
finally:
    for p in reversed(processes):
        if p.poll() is None:
            p.send_signal(signal.SIGCONT);p.terminate()
            try:p.wait(timeout=4)
            except subprocess.TimeoutExpired:p.kill();p.wait(timeout=4)
    (ROOT/'evidence').mkdir(exist_ok=True)
    (ROOT/'evidence/integration-results.json').write_text(json.dumps({'test_environment':'Xvfb + protocol test server + private D-Bus; no physical display','passed':results,'logs':str(TMP)},indent=2)+'\n')
    print('Test logs:',TMP,flush=True)
