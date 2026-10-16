import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent
plan = json.loads((ROOT / 'run-plan.json').read_text())
candidates = {}
for file in ROOT.glob('*-manifests.json'):
    if file.name == 'completed-manifests.json':
        continue
    prefix = file.name.removesuffix('-manifests.json')
    for index, manifest in enumerate(json.loads(file.read_text()), 1):
        job = manifest['jobs'][0]
        if job['status'] not in ('passed', 'review'):
            continue
        candidate = {'folder': f'{prefix}-{index:02d}-{job["id"].replace("/", "-")}', 'manifest': manifest}
        old = candidates.get(job['id'])
        if old is None or manifest['startedAt'] > old['manifest']['startedAt']:
            candidates[job['id']] = candidate
assert set(plan['jobs']).issubset(candidates), set(plan['jobs']) - set(candidates)
selected = [candidates[job] for job in plan['jobs']]
(ROOT / 'selected-runs.json').write_text(json.dumps(selected, indent=2) + '\n')
(ROOT / 'completed-manifests.json').write_text(json.dumps([v['manifest'] for v in selected], indent=2) + '\n')
print('Selected latest completed measurement for all 14 jobs; no score-based selection')
