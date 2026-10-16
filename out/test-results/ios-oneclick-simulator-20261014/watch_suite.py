import json,time,subprocess,shutil
from pathlib import Path
root=Path(__file__).resolve().parent
udid='53987196-51C0-4920-B6E9-3620653551F9'
bundle='com.fairchild.Mangekyo'
before=root/'second-before/suite-runs/latest.json'
old=json.loads(before.read_text())['id'] if before.exists() else None
last=None
for _ in range(180):
    time.sleep(10)
    p=subprocess.run(['xcrun','simctl','get_app_container',udid,bundle,'data'],capture_output=True,text=True)
    if p.returncode: continue
    docs=Path(p.stdout.strip())/'Documents'
    try: j=json.loads((docs/'suite-runs/latest.json').read_text())
    except (OSError,ValueError): continue
    if j['id']==old: continue
    (root/'latest.json').write_text(json.dumps(j,indent=2))
    state=(j['id'],j['status'],tuple((x['id'],x['status']) for x in j['jobs']))
    if state!=last:
        complete=[x for x in j['jobs'] if x['status'] in ['passed','review','failed','cancelled']]
        current=[(x['id'],x['status'],x.get('error','')) for x in j['jobs'] if x['status'] not in ['pending','passed']]
        print('STATUS',j['status'],f'{len(complete)}/{len(j["jobs"])}',current,flush=True)
        last=state
    if j['status']!='running':
        shutil.copytree(docs,root/'final-documents',dirs_exist_ok=True)
        print('DONE archived final Documents',flush=True)
        break
else: raise SystemExit('No completed suite within 30 minutes')
