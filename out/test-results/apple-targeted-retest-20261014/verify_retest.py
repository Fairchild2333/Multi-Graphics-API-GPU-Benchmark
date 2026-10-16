from pathlib import Path
import hashlib,json,math
root=Path(__file__).resolve().parent
summary={}
selected={}
for device,count in [('ipad',11),('iphone',7)]:
    before=json.loads((root/f'{device}-before/results/results.json').read_text())
    after_path=root/f'{device}-final/results/results.json'
    after=json.loads(after_path.read_text())
    byid={r['id']:r for r in after}
    latest=json.loads((root/f'{device}-final/suite-runs/latest.json').read_text())
    manifests=[latest]
    if device=='iphone':
        first=json.loads((root/'iphone-first-retest/suite-runs/latest.json').read_text())
        manifests=[first,latest]
        assert len(first['jobs'])==7 and len(latest['jobs'])==4
        assert all(j['thermalStateStart']==0 for j in latest['jobs'])
    expected=set((root/f'{device}-jobs.txt').read_text().split(','))
    source_jobs={}
    all_new_ids=set()
    for manifest in manifests:
        assert manifest['status']=='passed' and all(j['status']=='passed' for j in manifest['jobs'])
        for job in manifest['jobs']:
            source_jobs[job['id']]=job
            all_new_ids.update(job['resultIDs'])
    assert len(source_jobs)==count and set(source_jobs)==expected
    assert len(after)==len(before)+len(all_new_ids)
    assert all(byid[r['id']]==r for r in before), 'Prior result changed'
    assert all_new_ids==set(byid)-{r['id'] for r in before}
    manifest_jobs=list(source_jobs.values())
    selected[device]=[]
    for job in manifest_jobs:
        r=byid[job['resultIDs'][0]]
        assert math.isfinite(r['score']) and r['score']>0
        config=dict(p.split('=',1) for p in r['workloadConfig'].split(';') if '=' in p)
        mode=config['timingMode']
        interval=r['avgTotalGpuMs']/r['avgFrameTimeMs']
        assert interval<=1.05, (job['id'],'command interval exceeds wall frame',interval)
        assert r['avgComputeMs']+r['avgRenderMs']<=r['avgTotalGpuMs']+0.1
        if r['graphicsApi']=='Metal':
            assert r['framesInFlight']==1
            assert mode=='serialized_command_buffer_gpu_time'
            assert r['workloadVersion'].endswith('_serial_timing_v2')
        if job['id'].startswith('vulkan/stream_headless'):
            assert mode=='synchronized_wall_clock'
            assert r['avgRenderMs']==0
            proxy=40*r['particleCount']/(r['avgFrameTimeMs']/1000)/1e9
            assert 0.9<r['score']/proxy<1.1
        if job['id']=='vulkan/gpu_burn-256':
            assert r['framesInFlight']==1
            assert mode in ['validated_single_frame_gpu_query','synchronized_wall_clock']
        selected[device].append(dict(job=job['id'],**r))
    summary[device]={
        'jobsPassed':count,'newRecords':len(all_new_ids),'historyBefore':len(before),'historyAfter':len(after),
        'priorRecordsUnchanged':True,'intervalChecksPassed':True,'computeRenderOverlapAbsent':True,
        'rawSha256':hashlib.sha256(after_path.read_bytes()).hexdigest(),
        'manifestIDs':[m['id'] for m in manifests],
        'thermalStart':[j['thermalStateStart'] for j in manifest_jobs],
        'thermalEnd':[j['thermalStateEnd'] for j in manifest_jobs]}
    (root/f'{device}-retest-results.json').write_text(json.dumps(selected[device],ensure_ascii=False,indent=2)+'\n')
(root/'verification.json').write_text(json.dumps(summary,indent=2)+'\n')
# Curated mixed suite: update exactly the targeted jobs, preserve every other selection.
for device,archive in [('ipad','ipadpro3-20261014'),('iphone','ios-api-completion-20261014')]:
    old_manifest=json.loads((root.parent/archive/'final-phone/suite-runs/latest.json').read_text())
    old_rows=json.loads((root.parent/archive/'suite-results.json').read_text())
    byid={r['id']:r for r in old_rows}
    replacements={r['job']:r for r in selected[device]}
    curated=[]
    for job in old_manifest['jobs']:
        if job['id'] in replacements:
            r=dict(replacements[job['id']]);r.pop('job');curated.append(r)
        else:
            curated.extend(byid[i] for i in job['resultIDs'])
    assert len(curated)==32
    (root/f'{device}-curated-suite-results.json').write_text(json.dumps(curated,ensure_ascii=False,indent=2)+'\n')
print(json.dumps(summary,indent=2))
