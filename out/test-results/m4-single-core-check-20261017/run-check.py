import json,subprocess,os
from pathlib import Path
r=Path(__file__).resolve().parent
e=dict(os.environ,GPU_BENCH_DATA_DIR=str(r/'data'))
with (r/'cpu-single-confirmation.log').open('w') as log:
 p=subprocess.run(json.loads((r/'command.json').read_text()),env=e,stdout=log,stderr=subprocess.STDOUT,timeout=180)
(r/'exit.json').write_text(json.dumps({'returnCode':p.returncode})+'\n')
raise SystemExit(p.returncode)
