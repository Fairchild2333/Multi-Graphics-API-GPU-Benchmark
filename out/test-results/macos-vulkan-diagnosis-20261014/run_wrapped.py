from pathlib import Path
import json,os,time,subprocess
from datetime import datetime
root=Path(__file__).resolve().parent
worker=root/'wrapped-worker/gpu_benchmark'
previous=json.loads((root/'status.json').read_text())[-1]
end=datetime.fromisoformat(previous['thermalAfter']['timestamp'].replace('Z','+00:00')).timestamp()
time.sleep(max(0,end+60-time.time()))
probe=root/'thermal-probe'
def thermal():return json.loads(subprocess.check_output([str(probe)],text=True))
before=thermal();assert before['thermalState']==0 and not before['lowPowerMode']
args=[str(worker),'--gui-worker','--gpu','0','--time','15','--no-renderdoc','--backend','vulkan','--allow-translated-api','--workload','stream','--particles','1048576','--flights','2']
entry={'job':'vulkan-render-flight2-wrapped','classification':'diagnostic_shader_variant_not_standard_score','command':args,'thermalBefore':before}
env=dict(os.environ,GPU_BENCH_DATA_DIR=str(root/'wrapped-render-data'))
with (root/'vulkan-render-flight2-wrapped.log').open('w') as log:
 p=subprocess.run(args,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=150)
entry['returnCode']=p.returncode;entry['thermalAfter']=thermal()
r=json.loads((root/'wrapped-render-data/results/results.json').read_text());assert p.returncode==0 and len(r)==1
entry.update(resultID=r[0]['id'],score=r[0]['score'],stableScore=r[0]['stableScore'],throughputDropPct=r[0]['throttlePct'])
(root/'wrapped-render-status.json').write_text(json.dumps(entry,indent=2)+'\n');print(json.dumps(entry,indent=2),flush=True)
