from pathlib import Path
import json,subprocess,os,datetime,re
report=Path(__file__).resolve().parent
helper=report.parents[1]/'macos/arm64/Release/Mangekyo.app/Contents/Helpers/gpu_benchmark'
results=Path('/Users/fairchild/Library/Application Support/GpuComputeBenchmark/results/results.json')
jobs=[dict(s.split('=',1) for s in l.split('\t')[1:] if '=' in s) for l in (report/'suite-plan.txt').read_text().splitlines() if l.startswith('SUITE_JOB\t')]
jobs=[j for j in jobs if j['backend']=='vulkan' and j['headless']=='0']
assert len(jobs)==8
checks=[]
for i,j in enumerate(jobs,1):
    before={r['id'] for r in json.loads(results.read_text())}
    args=[str(helper),'--backend','vulkan','--gpu','0','--workload',j['workload'],'--time','15','--gui-worker','--allow-translated-api','--no-renderdoc']
    if j['workload']=='stream': args+=['--particles',j['particles']]
    if j['workload']=='gpu_burn': args+=['--iter',j['steps']]
    log=report/f'fixed-{i:02d}-{j["workload"]}-{j["particles"]}-{j["steps"]}.log'
    print(f'Start {i}/8: {j}',flush=True)
    with log.open('w') as output:
        try: code=subprocess.run(args,cwd='/tmp',stdout=output,stderr=subprocess.STDOUT,timeout=240).returncode
        except subprocess.TimeoutExpired: code=124
    fresh=[r for r in json.loads(results.read_text()) if r['id'] not in before]
    match=[r for r in fresh if r['graphicsApi']=='Vulkan' and r['workload']==j['workload'] and not r['headless'] and r['resWidth']==1280 and r['resHeight']==720 and 'viewport1280x720_v1' in r['workloadVersion'] and r['score']>0]
    if j['workload']=='stream': match=[r for r in match if r['particleCount']==int(j['particles'])]
    if j['workload']=='gpu_burn': match=[r for r in match if f'steps={j["steps"]};' in r['workloadConfig']]
    passed=code==0 and len(match)==1 and '[Vulkan] Render extent: 1280x720' in log.read_text()
    check={'index':i,'job':j,'exitCode':code,'passed':passed,'resultIds':[r['id'] for r in match],'log':str(log),'args':args}
    checks.append(check)
    (report/'fixed-viewports-status.json').write_text(json.dumps(checks,ensure_ascii=False,indent=2)+'\n')
    print(json.dumps(check,ensure_ascii=False),flush=True)
print('Corrected viewport runs passed:',sum(c['passed'] for c in checks),'/',len(jobs),flush=True)
raise SystemExit(0 if all(c['passed'] for c in checks) else 1)
