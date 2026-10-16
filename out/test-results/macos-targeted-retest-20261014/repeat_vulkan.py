from pathlib import Path
import os, subprocess, time, json
from datetime import datetime
root=Path(__file__).resolve().parent
status=json.loads((root/'run-status.json').read_text())
e=status[-1]
end=datetime.fromisoformat(e['thermalAfter']['timestamp'].replace('Z','+00:00')).timestamp()
time.sleep(max(0,end+60-time.time()))
probe=root/'thermal-probe'
def thermal(): return json.loads(subprocess.check_output([str(probe)],text=True))
before=thermal()
assert before['thermalState']==0 and not before['lowPowerMode']
results=root/'data/results/results.json';old={r['id'] for r in json.loads(results.read_text())}
entry={'job':'vulkan-stream1m-render-repeat','command':e['command'],'thermalBefore':before,'repeatOf':e['resultID'],'restPolicy':'at least60s since first Vulkan run'}
status.append(entry);(root/'run-status.json').write_text(json.dumps(status,indent=2)+'\n')
with (root/'vulkan-stream1m-render-repeat.log').open('w') as log:
 p=subprocess.run(entry['command'],env=dict(os.environ,GPU_BENCH_DATA_DIR=str(root/'data')),stdout=log,stderr=subprocess.STDOUT,timeout=150)
entry['returnCode']=p.returncode;entry['thermalAfter']=thermal()
fresh=[r for r in json.loads(results.read_text()) if r['id'] not in old]
assert p.returncode==0 and len(fresh)==1
r=fresh[0]
reasons=[]
if r['avgTotalGpuMs']>r['avgFrameTimeMs']*1.1: reasons.append('GPU timing span exceeds frame interval')
if r['avgComputeMs']+r['avgRenderMs']>r['avgTotalGpuMs']+.1: reasons.append('compute/render timing overlap')
if r['throttlePct']>5: reasons.append('throughput fell by more than 5 percent')
if r['stableScore']<=0: reasons.append('stable throughput not reached')
entry.update(resultID=r['id'],score=r['score'],unit=r['scoreUnit'],reviewReasons=reasons)
(root/'run-status.json').write_text(json.dumps(status,indent=2)+'\n')
print(json.dumps(entry,indent=2),flush=True)
