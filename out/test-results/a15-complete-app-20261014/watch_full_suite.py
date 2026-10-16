import json
import time
from pathlib import Path
from run_targeted import copy, ROOT

folder = ROOT / 'ui-full'
folder.mkdir(exist_ok=True)
last = None
deadline = time.monotonic() + 2600
while time.monotonic() < deadline:
    time.sleep(10)
    try:
        copy('Documents/suite-runs/latest.json', folder / 'latest.json')
    except Exception as exc:
        print('COPY WAIT', str(exc)[:250], flush=True)
        continue
    report = json.loads((folder / 'latest.json').read_text())
    if len(report['jobs']) != 31:
        continue
    state = (report['status'], tuple((j['id'], j['status']) for j in report['jobs']))
    if state != last:
        current = [j['id'] for j in report['jobs'] if j['status'] == 'running']
        passed = sum(j['status'] == 'passed' for j in report['jobs'])
        print('UI SUITE', report['id'], report['status'], f'{passed}/31', current, flush=True)
        last = state
        (folder / 'progress.json').write_text(json.dumps(report, indent=2) + '\n')
    if report['status'] != 'running':
        copy('Documents', folder / 'documents')
        print('ARCHIVED', report['status'], [(j['id'], j['status'], j['error']) for j in report['jobs'] if j['status'] != 'passed'], flush=True)
        break
else:
    raise RuntimeError('UI complete suite did not finalize within acceptance window')
