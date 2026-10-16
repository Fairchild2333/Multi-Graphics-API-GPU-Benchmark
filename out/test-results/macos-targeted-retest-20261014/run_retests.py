from pathlib import Path
import os, subprocess, time, json, hashlib
root = Path(__file__).resolve().parent
repo = root.parents[2]
worker = repo / 'out/macos/arm64/Release/Mangekyo.app/Contents/Helpers/gpu_benchmark'
probe = root / 'thermal-probe'
env = dict(os.environ, GPU_BENCH_DATA_DIR=str(root / 'data'))
plans = [
 ('metal-stream1m-render', ['--backend','metal','--workload','stream','--particles','1048576']),
 ('metal-stream16m-render', ['--backend','metal','--workload','stream','--particles','16777216']),
 ('metal-burn16', ['--backend','metal','--workload','gpu_burn','--iter','16']),
 ('metal-burn64', ['--backend','metal','--workload','gpu_burn','--iter','64']),
 ('vulkan-stream1m-render', ['--backend','vulkan','--allow-translated-api','--workload','stream','--particles','1048576']),
]
status=[]
def thermal():
 return json.loads(subprocess.check_output([str(probe)], text=True))
def save():
 (root/'run-status.json').write_text(json.dumps(status,indent=2)+'\n')
def reasons(r):
 out=[]
 if r.get('avgTotalGpuMs',0)>r.get('avgFrameTimeMs',0)*1.1: out.append('GPU timing span exceeds frame interval')
 if r.get('avgComputeMs',0)+r.get('avgRenderMs',0)>r.get('avgTotalGpuMs',0)+.1: out.append('compute/render timing overlap')
 if r.get('throttlePct',0)>5: out.append('throughput fell by more than 5 percent')
 if r.get('stableScore',0)<=0: out.append('stable throughput not reached')
 return out
for index,(name,args) in enumerate(plans):
 if index:
  print('cooldown 30s',flush=True)
  time.sleep(30)
 deadline=time.monotonic()+600
 before=thermal()
 while before['thermalState']!=0 and time.monotonic()<deadline:
  print(name,'waiting for nominal thermal state', before['thermalStateName'],flush=True)
  time.sleep(10);before=thermal()
 if before['thermalState']!=0: raise RuntimeError('thermal timeout')
 results=root/'data/results/results.json'
 old={r['id'] for r in json.loads(results.read_text())}
 command=[str(worker),'--gui-worker','--gpu','0','--time','15','--no-renderdoc',*args]
 entry={'job':name,'command':command,'thermalBefore':before}
 status.append(entry);save();print('running',name,flush=True)
 with (root/(name+'.log')).open('w') as log:
  p=subprocess.run(command,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=150)
 entry['returnCode']=p.returncode;entry['thermalAfter']=thermal()
 fresh=[r for r in json.loads(results.read_text()) if r['id'] not in old]
 if len(fresh)!=1 or p.returncode!=0: entry['error']='missing/excess result or worker failure';save();raise RuntimeError(entry)
 r=fresh[0]
 entry['resultID']=r['id'];entry['score']=r['score'];entry['unit']=r['scoreUnit'];entry['reviewReasons']=reasons(r)
 save();print('finished',name,r['score'],r['scoreUnit'],'review',entry['reviewReasons'],flush=True)
print('all pending Mac jobs completed',flush=True)
