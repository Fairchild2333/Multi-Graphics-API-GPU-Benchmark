from pathlib import Path
import json,os,time,datetime,shutil
report=Path(__file__).resolve().parent
manifest=json.loads((report/'run-manifest.json').read_text())
root=Path(manifest['resultsFile']).parent
baseline=report/'before-complete-suite-status.tsv'
base_lines=len(baseline.read_text().splitlines()) if baseline.exists() else 0
pid=9516
while True:
    try:
        rows=json.loads((root/'results.json').read_text())
        fresh=[r for r in rows if r.get('id') not in manifest['baselineIds']]
        lines=(root/'complete-suite-status.tsv').read_text().splitlines()[base_lines:]
        active=True
        try: os.kill(pid,0)
        except ProcessLookupError: active=False
        state={'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'completed':len(lines),'planned':31,'savedResults':len(fresh),'active':active,'latest':lines[-1] if lines else ''}
        with (report/'progress.jsonl').open('a') as f: f.write(json.dumps(state)+'\n')
        (report/'new-results.json').write_text(json.dumps(fresh,ensure_ascii=False,indent=2)+'\n')
        (report/'new-suite-status.tsv').write_text('\n'.join(lines)+'\n')
        print(json.dumps(state,ensure_ascii=False),flush=True)
        if not active:
            shutil.copy2(root/'results.json',report/'after-results.json')
            shutil.copy2(root/'complete-suite-status.tsv',report/'after-complete-suite-status.tsv')
            break
    except (OSError,json.JSONDecodeError) as e:
        print(type(e).__name__,str(e),flush=True)
    time.sleep(40)
