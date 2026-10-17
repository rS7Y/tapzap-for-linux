#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Coordinate the dedicated Pop VM RTC tests without injecting a wake event."""
import argparse
import json
from pathlib import Path
import queue
import re
import subprocess
import threading
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--ssh-config', type=Path, required=True, help='Config with the dedicated dell and pop aliases')
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--run', required=True, help='Fresh lab run name beginning pop-rtc-')
parser.add_argument('--mode', choices=['s2idle', 's3'], required=True)
args = parser.parse_args()
assert re.fullmatch(r'pop-rtc-[a-z0-9-]+', args.run), 'Unsafe lab run name'
args.output.mkdir(exist_ok=False)

def ssh(host, command, **kwargs):
    return subprocess.run(['ssh', '-F', str(args.ssh_config), host, command], text=True, timeout=20, **kwargs)

flag = '--s2idle' if args.mode == 's2idle' else '--suspend'
directory = '/home/lab/' + args.run
command = ('set -o pipefail; python3 /home/lab/prototype/vm_lifecycle_tests.py '
           '--directory ' + directory + ' --binary /opt/tapzap-cosmic-prototype/bin/tapzap-cosmic '
           '--ipc-name tapzap-cosmic-prototype.sock --rtc-wake ' + flag + ' 2>&1 | tee ' + directory + '.log')
test = subprocess.Popen(['ssh', '-F', str(args.ssh_config), 'pop', command], stdout=subprocess.PIPE, text=True, bufsize=1)
lines = queue.Queue()

def read_output():
    with (args.output / 'guest.log').open('w') as log:
        for line in test.stdout:
            log.write(line)
            log.flush()
            lines.put(line)

reader = threading.Thread(target=read_output, daemon=True)
reader.start()
try:
    for phase, state in [('WAITING-HOST-DISCONNECT', 'disconnected'),
                         ('WAITING-HOST-RECONNECT', 'connected'), ('SUSPEND-REQUESTED', None)]:
        deadline = time.monotonic() + 150
        while True:
            assert time.monotonic() < deadline, 'Guest did not reach ' + phase
            try:
                line = lines.get(timeout=1)
            except queue.Empty:
                assert test.poll() is None and time.monotonic() < deadline, 'Guest did not reach ' + phase
                continue
            if phase in line:
                break
        if state:
            reply = ssh('dell', 'python3 /home/foton/TAPZAP-POP-20261017/monitor-connection.py ' + state, capture_output=True)
            reply.check_returncode()
            (args.output / ('monitor-' + state + '.json')).write_text(reply.stdout)
    # No QMP wake, WakeUp key, power button or fallback watcher is sent here.
    assert test.wait(timeout=90) == 0, 'Guest lifecycle failed'
    reader.join(timeout=5)
    result = ssh('pop', 'cat ' + directory + '/results.json', capture_output=True)
    result.check_returncode()
    data = json.loads(result.stdout)
    assert data['completed'] and data['wake_method'] == 'guest RTC alarm', data
    assert len(data['cases']) == 13 and all(case['passed'] for case in data['cases']), data
    (args.output / 'results.json').write_text(result.stdout)
    (args.output / 'host.json').write_text(json.dumps({'completed': True, 'host_injected_wake_events': 0,
        'run': args.run, 'mode': args.mode}, indent=2) + '\n')
    print(json.dumps({'completed': True, 'cases': len(data['cases']), 'sleep_mode': data['sleep_mode'],
        'sleep_timing': data['sleep_timing'], 'kernel_sleep_trace_seconds': data.get('kernel_sleep_trace_seconds')}))
finally:
    if test.poll() is None:
        test.terminate()
        test.wait(timeout=5)
