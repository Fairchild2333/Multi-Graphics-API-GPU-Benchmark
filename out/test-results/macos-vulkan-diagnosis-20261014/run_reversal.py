from pathlib import Path
import json,os,time,subprocess
from datetime import datetime
root=Path(__file__).resolve().parent;repo=root.parents[2]
worker=repo/'out/macos/arm64/Release/Mangekyo.app/Contents/Helpers/gpu_benchmark'
previous=json.loads((root/'wrapped-render-status.json').read_text())
end=datetime.fromisoformat(previous['thermalAfter']['timestamp'].replace('Z','+00:00')).timestamp()
time.sleep(max(0,end+60-time.time()))
probe=root/'thermal-probe'
def thermal():return json.loads(subprocess.check_output([str(probe)],text=True))
before=thermal();assert before['thermalState']==0 and not before['lowPowerMode']
args=[str(worker),'--gui-worker','--gpu','0','--time','15','--no-renderdoc','--backend','vulkan','--allow-translated-api','--workload','stream','--particles','1048576','--flights','2']
status=json.loads((root/'status.json').read_text());data=root/'data/results/results.json'
old={r['id'] for r in json.loads(data.read_text())}
e={'job':'vulkan-render-flight2-reversal','headless':False,'flights':2,'classification':'baseline_reversal','command':args,'thermalBefore':before,'afterWrappedResultID':previous['resultID']}
status.append(e);(root/'status.json').write_text(json.dumps(status,indent=2)+'\n')
with (root/'vulkan-render-flight2-reversal.log').open('w') as log:
 p=subprocess.run(args,env=dict(os.environ,GPU_BENCH_DATA_DIR=str(root/'data')),stdout=log,stderr=subprocess.STDOUT,timeout=150)
e['returnCode']=p.returncode;e['thermalAfter']=thermal()
fresh=[r for r in json.loads(data.read_text()) if r['id'] not in old];assert p.returncode==0 and len(fresh)==1
r=fresh[0];reasons=[]
if r['throttlePct']>5:reasons.append('throughput fell by more than5%')
if r['stableScore']<=0:reasons.append('stable throughput not reached')
e.update(resultID=r['id'],score=r['score'],stableScore=r['stableScore'],throughputDropPct=r['throttlePct'],reviewReasons=reasons)
(root/'status.json').write_text(json.dumps(status,indent=2)+'\n');print(json.dumps(e,indent=2),flush=True)
