from pathlib import Path
import os,subprocess,json
root=Path(__file__).resolve().parent
repo=root.parents[2]
worker=repo/'out/macos/arm64/Release/Mangekyo.app/Contents/Helpers/gpu_benchmark'
data=root/'mac-validation';data.mkdir(exist_ok=True)
env=dict(os.environ,GPU_BENCH_DATA_DIR=str(data))
plans=[('metal-stream4m',['--backend','metal','--workload','stream','--particles','4194304']),('metal-burn256',['--backend','metal','--workload','gpu_burn','--iter','256']),('metal-liquid',['--backend','metal','--workload','cinematic_liquid']),('vulkan-burn256',['--backend','vulkan','--allow-translated-api','--workload','gpu_burn','--iter','256'])]
status=[]
for name,args in plans:
 with (root/(name+'.log')).open('w') as log:
  r=subprocess.run([str(worker),'--gui-worker','--gpu','0','--time','15','--no-renderdoc',*args],env=env,stdout=log,stderr=subprocess.STDOUT,timeout=120)
 status.append(dict(name=name,returnCode=r.returncode))
 print(name,r.returncode,flush=True)
 if r.returncode:break
(root/'mac-validation-status.json').write_text(json.dumps(status,indent=2)+'\n')
