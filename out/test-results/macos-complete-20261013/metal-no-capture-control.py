from pathlib import Path
import json,subprocess
report=Path(__file__).resolve().parent
helper=report.parents[1]/'macos/arm64/Release/Mangekyo.app/Contents/Helpers/gpu_benchmark'
results=Path('/Users/fairchild/Library/Application Support/GpuComputeBenchmark/results/results.json')
jobs=[dict(s.split('=',1) for s in l.split('\t')[1:] if '=' in s) for l in (report/'suite-plan.txt').read_text().splitlines() if l.startswith('SUITE_JOB\t')]
jobs=[j for j in jobs if j['backend']=='metal' and j['headless']=='0']
assert len(jobs)==8
checks=[]
for i,j in enumerate(jobs,1):
    args=[str(helper),'--backend','metal','--gpu','0','--workload',j['workload'],'--time','15','--gui-worker','--no-renderdoc']
    if j['workload']=='stream': args+=['--particles',j['particles']]
    if j['workload']=='gpu_burn': args+=['--iter',j['steps']]
    log=report/f'metal-control-{i:02d}-{j["workload"]}-{j["particles"]}-{j["steps"]}.log'
    if j['workload']=='stream' and j['particles']=='1048576':
        earlier=json.loads((report/'metal-no-capture-check.json').read_text())
        code=earlier['exit'];fresh=earlier['results'];log=report/'metal-no-capture-1m.log'
    else:
        before={r['id'] for r in json.loads(results.read_text())}
        print(f'Start Metal control {i}/8: {j["workload"]} particles={j["particles"]} steps={j["steps"]}',flush=True)
        with log.open('w') as output:
            try: code=subprocess.run(args,cwd='/tmp',stdout=output,stderr=subprocess.STDOUT,timeout=240).returncode
            except subprocess.TimeoutExpired: code=124
        fresh=[r for r in json.loads(results.read_text()) if r['id'] not in before]
    match=[r for r in fresh if r['graphicsApi']=='Metal' and r['workload']==j['workload'] and not r['headless'] and r['resWidth']==1280 and r['resHeight']==720 and r['score']>0 and 'renderDoc=disabled' in r['workloadConfig'] and 'captureCount=' not in r['workloadConfig']]
    if j['workload']=='stream': match=[r for r in match if r['particleCount']==int(j['particles'])]
    if j['workload']=='gpu_burn': match=[r for r in match if f'steps={j["steps"]};' in r['workloadConfig']]
    check={'index':i,'job':j,'exitCode':code,'passed':code==0 and len(match)==1,'resultIds':[r['id'] for r in match],'log':str(log),'args':args}
    checks.append(check)
    (report/'metal-control-status.json').write_text(json.dumps(checks,ensure_ascii=False,indent=2)+'\n')
    print(f'Metal control {i}/8: passed={check["passed"]} ids={check["resultIds"]}',flush=True)
print('Metal controls passed:',sum(c['passed'] for c in checks),'/',len(jobs),flush=True)
raise SystemExit(0 if all(c['passed'] for c in checks) else 1)
