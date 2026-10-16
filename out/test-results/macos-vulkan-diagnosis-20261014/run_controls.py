from pathlib import Path
import json, os, time, subprocess
root=Path(__file__).resolve().parent;repo=root.parents[2]
worker=repo/'out/macos/arm64/Release/Mangekyo.app/Contents/Helpers/gpu_benchmark'
probe=root/'thermal-probe'
env=dict(os.environ,GPU_BENCH_DATA_DIR=str(root/'data'))
plans=[('vulkan-render-flight1',False,1),('vulkan-headless-flight2',True,2),('vulkan-headless-flight1',True,1),('vulkan-render-flight2-baseline',False,2)]
status=[]
def thermal(): return json.loads(subprocess.check_output([str(probe)],text=True))
def save(): (root/'status.json').write_text(json.dumps(status,indent=2)+'\n')
for index,(name,headless,flights) in enumerate(plans):
 if index: print('rest60s',flush=True);time.sleep(60)
 before=thermal();deadline=time.monotonic()+600
 while before['thermalState']!=0 and time.monotonic()<deadline:
  print('waiting',before['thermalStateName'],flush=True);time.sleep(10);before=thermal()
 assert before['thermalState']==0 and not before['lowPowerMode']
 data=root/'data/results/results.json';old={r['id'] for r in json.loads(data.read_text())}
 args=[str(worker),'--gui-worker','--gpu','0','--time','15','--no-renderdoc','--backend','vulkan','--allow-translated-api','--workload','stream','--particles','1048576','--flights',str(flights)]
 if headless: args.append('--headless')
 entry={'job':name,'headless':headless,'flights':flights,'command':args,'thermalBefore':before,'classification':'diagnostic_control' if index<3 else 'baseline_repetition'}
 status.append(entry);save();print('running',name,flush=True)
 with (root/(name+'.log')).open('w') as log:
  p=subprocess.run(args,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=150)
 entry['returnCode']=p.returncode;entry['thermalAfter']=thermal()
 fresh=[r for r in json.loads(data.read_text()) if r['id'] not in old]
 if p.returncode or len(fresh)!=1: entry['error']='worker/result failure';save();raise RuntimeError(entry)
 r=fresh[0];assert r['headless']==headless and r['framesInFlight']==flights and r['particleCount']==1048576 and r['durationSec']>=11
 reasons=[]
 if r['avgTotalGpuMs']>r['avgFrameTimeMs']*1.1: reasons.append('GPU timing span exceeds frame interval')
 if r['avgComputeMs']+r['avgRenderMs']>r['avgTotalGpuMs']+.1: reasons.append('compute/render timing overlap')
 if headless and r['avgComputeMs']<r['avgFrameTimeMs']*.5: reasons.append('compute timing needs wall-clock verification')
 if r['throttlePct']>5: reasons.append('throughput fell by more than5%')
 if r['stableScore']<=0: reasons.append('stable throughput not reached')
 entry.update(resultID=r['id'],score=r['score'],stableScore=r['stableScore'],throughputDropPct=r['throttlePct'],reviewReasons=reasons)
 save();print('finished',name,r['score'],'GB/s',r['throttlePct'],'% drop',reasons,flush=True)
print('controls complete',flush=True)
