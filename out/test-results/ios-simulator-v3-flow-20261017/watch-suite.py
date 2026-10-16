import json,time,subprocess,shutil
from pathlib import Path
root=Path(__file__).resolve().parent
udid='46753076-75AF-49C0-90A2-3A30ECC498AC'
last=None
for _ in range(250):
 p=subprocess.run(['xcrun','simctl','get_app_container',udid,'com.fairchild.Mangekyo','data'],capture_output=True,text=True)
 if p.returncode==0:
  docs=Path(p.stdout.strip())/'Documents'
  try: data=json.loads((docs/'suite-runs/latest.json').read_text())
  except (OSError,ValueError): data=None
  if data:
   (root/'latest.json').write_text(json.dumps(data,indent=2))
   (root/'container-path.txt').write_text(str(docs.parent))
   completed=[j for j in data['jobs'] if j['status'] not in ['pending','running']]
   state=(data['status'],len(completed),tuple((j['id'],j['status']) for j in data['jobs'] if j['status']!='pending'))
   if state!=last:
    print(json.dumps({'status':data['status'],'completed':len(completed),'total':len(data['jobs']),'running':[j['id'] for j in data['jobs'] if j['status']=='running'],'reviews':[(j['id'],j.get('error')) for j in data['jobs'] if j['status']=='review'],'failed':[(j['id'],j.get('error')) for j in data['jobs'] if j['status']=='failed']}),flush=True)
    last=state
   if data['status']!='running':
    shutil.copytree(docs,root/'final-documents')
    print('Archived final native Documents (SIMULATOR ONLY)',flush=True)
    break
 time.sleep(10)
else: raise SystemExit('No completed simulator suite within acceptance window')
