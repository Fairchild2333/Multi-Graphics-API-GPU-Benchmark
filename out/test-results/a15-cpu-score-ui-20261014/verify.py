import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent
before_path = ROOT / 'before/results/results.json'
after_path = ROOT / 'transition/documents/results/results.json'
before = json.loads(before_path.read_text())
after = json.loads(after_path.read_text())
assert len(before) == 105 and len(after) == 113
assert after[:len(before)] == before, 'Historical rows changed'
by_id = {r['id']: r for r in after}
assert len(by_id) == len(after), 'Duplicate IDs'
reports = [json.loads((ROOT / folder / 'latest.json').read_text()) for folder in
    ['diagnostic-01-cpu_compute', 'diagnostic-02-cpu_memory', 'transition']]
assert all(r['status'] == 'passed' for r in reports)
jobs = [j for r in reports for j in r['jobs']]
assert all(j['status'] == 'passed' and j['thermalStateStart'] == 0 and j['thermalStateEnd'] == 0 for j in jobs)
new_ids = [i for j in jobs for i in j['resultIDs']]
assert set(new_ids) == set(by_id) - {r['id'] for r in before}
def fields(r):
    return dict(v.split('=', 1) for v in r['workloadConfig'].split(';') if '=' in v)
checks = []
for r in after[len(before):]:
    f = fields(r)
    if r['workload'] == 'cpu_multi_core':
        workers = [float(v) for v in f['workerScores'].split(',')]
        assert len(workers) == int(f['threads']) == 6
        assert abs(sum(workers) - r['score']) < .0005
        assert f['workerLabels'] == 'unclassified_threads_not_numbered_cores'
        checks.append({'id': r['id'], 'score': r['score'], 'worker_scores': workers,
            'sum': sum(workers), 'process_cpu_seconds': float(f['processCpuSeconds']),
            'performance_cpu_seconds': float(f['performanceCpuSeconds'])})
    elif r['workload'] == 'cpu_single_core':
        bounds = [float(f[f'round{i}RequestedLowerBound']) for i in range(1, 4)]
        assert min(bounds) >= .99 and f['residencyVerified'] == '1'
        assert f['scheduling'] == ('background' if f['coreType'] == 'efficiency' else 'user_interactive')
    elif r['workload'] == 'cpu_membw':
        points = [v.split(':') for k in ['residency_single', 'residency_single_1'] for v in f[k].split(',')]
        assert len(points) == 186 and min(float(v[2]) for v in points) >= .99
        assert f['pageFaultMeasured'] == f['storageMeasured'] == '1'
        assert all(f['curve_' + mode + '_' + kernel] for mode in ['single', 'single_1', 'all'] for kernel in ['read', 'write', 'copy'])
baseline = json.loads((ROOT.parent / 'a15-cpu-pe-20261014/curated-suite-results.json').read_text())
selected = dict(json.loads((ROOT.parent / 'a15-cpu-pe-20261014/selected-results.json').read_text()))
for j in jobs:
    old = selected.get(j['id'], [])
    remove_ids = {r['id'] for r in old}
    replacement = [by_id[i] for i in j['resultIDs']]
    if remove_ids:
        baseline = [r for r in baseline if r['id'] not in remove_ids] + replacement
    selected[j['id']] = replacement
assert len(baseline) == 33 and len({r['id'] for r in baseline}) == 33
(ROOT / 'curated-suite-results.json').write_text(json.dumps(baseline, indent=2) + '\n')
(ROOT / 'selected-results.json').write_text(json.dumps(selected, indent=2) + '\n')
verification = {'historical_rows_preserved': len(before), 'history_rows': len(after),
    'new_rows': len(new_ids), 'curated_rows': len(baseline), 'jobs_passed': len(jobs),
    'all_worker_sums_verified': checks, 'classified_memory_points_verified': 186,
    'before_sha256': hashlib.sha256(before_path.read_bytes()).hexdigest(),
    'after_sha256': hashlib.sha256(after_path.read_bytes()).hexdigest()}
(ROOT / 'verification.json').write_text(json.dumps(verification, indent=2) + '\n')
print(json.dumps(verification, indent=2))
