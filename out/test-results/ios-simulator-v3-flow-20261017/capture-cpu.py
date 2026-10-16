"""Capture the real simulator CPU progress pages; never send input to the app."""
from pathlib import Path
import json,subprocess,time
root=Path(__file__).resolve().parent
udid=json.loads((root/'environment.json').read_text())['simulator']
saved=[]
for _ in range(180):
    try:
        container=Path((root/'container-path.txt').read_text().strip())
        native=container/'Documents/suite-runs/latest.json'
        report=json.loads(native.read_text())
        jobs=[j['id'] for j in report['jobs'] if j['status']=='running']
    except (OSError,ValueError):
        time.sleep(5); continue
    for job in jobs:
        if job not in ['cpu_compute','cpu_memory'] or any(s['job']==job for s in saved): continue
        result=subprocess.run(['xcrun','simctl','io',udid,'screenshot',str(root/(job+'-progress.png'))],capture_output=True,text=True)
        after=json.loads(native.read_text())
        item={'job':job,'returnCode':result.returncode,'commandOutput':result.stdout+result.stderr,'runningAfterScreenshot':[j['id'] for j in after['jobs'] if j['status']=='running']}
        saved.append(item)
        (root/'cpu-screenshots.json').write_text(json.dumps(saved,indent=2)+'\n')
        print(job,result.returncode,flush=True)
    if report['status']!='running' or len(saved)==2: break
    time.sleep(5)
