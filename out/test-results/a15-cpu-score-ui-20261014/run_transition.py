import json
import subprocess
import time
from pathlib import Path
from run_targeted import copy, ROOT, DEVICE, BUNDLE

folder = ROOT / 'transition'
folder.mkdir(exist_ok=False)
console = (folder / 'console.log').open('w')
job_ids = ['stream-65536', 'cpu_compute']
proc = subprocess.Popen(['xcrun', 'devicectl', 'device', 'process', 'launch',
    '--device', DEVICE, '--terminate-existing', '--console', '--json-output',
    str(folder / 'launch.json'), BUNDLE, '--ios-allow-translated-api',
    '--ios-complete-suite', '--ios-suite-jobs', ','.join(job_ids)],
    stdout=console, stderr=subprocess.STDOUT)
captured = set()
deadline = time.monotonic() + 750
while time.monotonic() < deadline:
    time.sleep(3)
    if proc.poll() is not None:
        raise RuntimeError('Device app exited; see console.log')
    copy('Documents/suite-runs/latest.json', folder / 'latest.json')
    report = json.loads((folder / 'latest.json').read_text())
    if [j['id'] for j in report['jobs']] != job_ids:
        continue
    for j in report['jobs']:
        if j['status'] == 'running' and j['id'] not in captured:
            captured.add(j['id'])
            subprocess.run(['xcrun', 'devicectl', 'device', 'capture', 'screenshot',
                '--device', DEVICE, '--destination', str(folder / (j['id'] + '.png')),
                '--timeout', '20'], check=True)
            print('CAPTURED', j['id'], flush=True)
    if report['status'] != 'running':
        break
else:
    raise RuntimeError('Timed out')
copy('Documents', folder / 'documents')
assert report['status'] == 'passed', report
assert all(j['status'] == 'passed' for j in report['jobs']), report
print('DONE', [(j['id'], j['resultIDs']) for j in report['jobs']], flush=True)
console.close()
