import hashlib
import json
import shutil
import subprocess
from pathlib import Path

ROOT=Path(__file__).resolve().parent
previous=ROOT.parent/'ipad8-1-full-suite-20261016'
read=lambda path:json.loads(path.read_text())
shutil.copytree(ROOT/'corrected-seven/documents',ROOT/'phone-final',dirs_exist_ok=False)
rows=read(ROOT/'phone-final/results/results.json');by_id={r['id']:r for r in rows}
before=read(ROOT/'before/results/results.json')
assert len(before)==36 and len(rows)==51 and all(by_id[r['id']]==r for r in before)
assert not read(ROOT/'processes-after-stop.json')['result']['runningProcesses']
latest=read(ROOT/'phone-final/suite-runs/latest.json')
assert latest['status']=='passed' and len(latest['jobs'])==7 and all(j['status']=='passed' for j in latest['jobs'])
old=read(previous/'selected-results.json')
sources=read(previous/'selected-run-sources.json')
overrides={j['id']:j for j in latest['jobs']}
selected=[]
for job in dict.fromkeys(r['job'] for r in old):
    identifiers=overrides[job]['resultIDs'] if job in overrides else [r['id'] for r in old if r['job']==job]
    selected.extend(dict(job=job,**by_id[i]) for i in identifiers)
    if job in overrides:sources[job]=latest['id']
assert len(selected)==33 and len(set(r['job'] for r in selected))==31
(ROOT/'selected-results.json').write_text(json.dumps(selected,ensure_ascii=False,indent=2)+'\n')
(ROOT/'curated-suite-results.json').write_text(json.dumps([by_id[r['id']] for r in selected],ensure_ascii=False,indent=2)+'\n')
(ROOT/'selected-run-sources.json').write_text(json.dumps(sources,indent=2)+'\n')

def fields(row):return dict(v.split('=',1) for v in row['workloadConfig'].split(';') if '=' in v)
cpu=next(r for r in selected if r['workload']=='cpu_multi_core');f=fields(cpu)
assert f['allCoreParticipationVerified']=='1' and f['allCoreValidation']=='aggregate_cpu_time_v1'
assert min(float(f[k]) for k in ['minimumRoundPerformanceOccupancy','minimumRoundEfficiencyOccupancy','minimumRoundWorkerOccupancy'])>=.90
log=(ROOT/'phone-final/suite-runs'/f"{latest['id']}.log").read_text()
participation=[];workers=[]
for line in log.splitlines():
    target=participation if line.startswith('CPU_MULTI_PARTICIPATION\t') else workers if line.startswith('CPU_MULTI_WORKER\t') else None
    if target is not None:target.append(dict(p.split('=',1) for p in line.split('\t')[1:] if '=' in p))
assert len(participation)==3 and len(workers)==24 and all(p['verified']=='1' for p in participation)
for item in participation:
    assert all(.90<=float(item[k])<=1.05 for k in ['performance_occupancy','efficiency_occupancy','minimum_worker_occupancy'])
for r in selected:
    if r['workload']=='cpu_single_core':
        cf=fields(r);assert cf['residencyVerified']=='1'
        assert min(float(cf[f'round{i}RequestedLowerBound']) for i in [1,2,3])>=.99
(ROOT/'all-worker-rounds.json').write_text(json.dumps(workers,indent=2)+'\n')
(ROOT/'participation-rounds.json').write_text(json.dumps(participation,indent=2)+'\n')
verification=dict(deviceModel='iPad8,1',actualRetestJobs=7,actualRetestStatus='passed',
    curatedJobs=31,curatedRecords=33,historyBefore=36,historyAfter=51,oldRecordsUnchanged=True,
    baselineJobs=4,baselineStatus='review',metalKernelChanged=False,stabilityCVThresholdChanged=False,
    cpuScore=cpu['score'],allCoreParticipationVerified=True,
    minimumRoundPerformanceOccupancy=float(f['minimumRoundPerformanceOccupancy']),
    minimumRoundEfficiencyOccupancy=float(f['minimumRoundEfficiencyOccupancy']),
    minimumRoundWorkerOccupancy=float(f['minimumRoundWorkerOccupancy']),
    finalSHA256=hashlib.sha256((ROOT/'phone-final/results/results.json').read_bytes()).hexdigest(),
    appStopped=True,stoppedPID=914,newFullButtonRun=False)
(ROOT/'verification.json').write_text(json.dumps(verification,indent=2)+'\n')
(ROOT/'source-changes.patch').write_bytes(subprocess.check_output(['git','diff','HEAD']))
print(json.dumps(verification,indent=2))
