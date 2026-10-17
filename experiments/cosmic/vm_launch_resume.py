import json,pathlib,subprocess
p=pathlib.Path('/home/foton/TAPZAP-POP-20261017');(p/'captures').mkdir(exist_ok=True)
assert not (p/'qemu.pid').exists(), 'An existing Pop VM pidfile needs investigation'
args=['qemu-system-x86_64','-name','TAP ZAP Pop OS COSMIC VM',
 '-machine','q35,accel=kvm,smm=on','-cpu','host','-smp','4','-m','4608',
 '-drive','if=pflash,format=raw,readonly=on,file=/usr/share/edk2/ovmf/OVMF_CODE.fd',
 '-drive',f'if=pflash,format=raw,file={p}/OVMF_VARS-resume.fd',
 '-drive',f'file={p}/writable.img,if=virtio,format=raw',
 '-drive','file=http://127.0.0.1:28480/pop.iso,media=cdrom,readonly=on,format=raw',
 '-kernel',str(p/'vmlinuz.efi'),'-initrd',str(p/'initrd-lab.img'),
 '-append','boot=casper live-media-path=/casper_pop-os_24.04_amd64_generic_debug_681 hostname=tapzap-pop-vm username=pop-os persistent noprompt noeject console=tty0 console=ttyS0,115200n8',
 '-device','pcie-root-port,id=tapzap-gpu-port,chassis=1',
 '-device','virtio-vga,bus=tapzap-gpu-port,id=tapzap-display,max_outputs=2,x-pcie-pm-no-soft-reset=true','-display','none',
 '-netdev','user,id=net0,hostfwd=tcp:127.0.0.1:22250-:22','-device','virtio-net-pci,netdev=net0',
 '-qmp','tcp:127.0.0.1:22251,server=on,wait=off',
 '-serial',f'file:{p}/serial.log','-monitor','none','-pidfile',str(p/'qemu.pid')]
(p/'resume-launch-arguments.json').write_text(json.dumps(args,indent=2)+'\n')
with (p/'qemu.log').open('ab') as log:
    proc=subprocess.Popen(args,stdin=subprocess.DEVNULL,stdout=log,stderr=log,start_new_session=True)
print(proc.pid)
