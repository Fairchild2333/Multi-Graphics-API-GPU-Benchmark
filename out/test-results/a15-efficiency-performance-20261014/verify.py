import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent
before_path = ROOT / 'before/results/results.json'
after_path = ROOT / 'foreground-confirm-final/results/results.json'
before = json.loads(before_path.read_text())
after = json.loads(after_path.read_text())
assert len(before) == 113 and len(after) == 119
assert after[:len(before)] == before
by_id = {r['id']: r for r in after}
assert len(by_id) == len(after)
def fields(row):
    return dict(v.split('=', 1) for v in row['workloadConfig'].split(';') if '=' in v)
runs = []
for prefix in ['foreground-first', 'foreground-confirm']:
    report = json.loads((ROOT / (prefix + '-01-cpu_compute/latest.json')).read_text())
    job = report['jobs'][0]
    assert report['status'] == job['status'] == 'passed'
    assert job['thermalStateStart'] == job['thermalStateEnd'] == 0
    rows = [by_id[i] for i in job['resultIDs']]
    assert len(rows) == 3
    p = next(r for r in rows if fields(r).get('coreType') == 'performance')
    e = next(r for r in rows if fields(r).get('coreType') == 'efficiency')
    all_core = next(r for r in rows if r['workload'] == 'cpu_multi_core')
    f = fields(e)
    assert f['scheduling'] == 'user_initiated'
    assert f['residencyMethod'] == 'thread_selfcounts_perf_level_spi'
    assert f['efficiencyPolicy'] == 'foreground_with_performance_load'
    assert f['residencyVerified'] == '1' and int(f['placementHelperThreads']) == 2
    bounds = [float(f[f'round{i}RequestedLowerBound']) for i in range(1, 4)]
    clocks = [float(f[f'round{i}EffectiveGHz']) for i in range(1, 4)]
    busy = [float(f[f'round{i}WorkerCpuSeconds']) / 5 for i in range(1, 4)]
    helper_bounds = [float(f[f'helper{i}RequestedLowerBound']) for i in range(2)]
    assert min(bounds) >= .99 and min(helper_bounds) >= .99
    assert min(busy) >= .9 and max(busy) <= 1.05 and min(clocks) > 0
    assert all(float(fields(p)[f'round{i}RequestedLowerBound']) >= .99 for i in range(1, 4))
    workers = [float(v) for v in fields(all_core)['workerScores'].split(',')]
    assert len(workers) == 6 and abs(sum(workers) - all_core['score']) < .0005
    estimate = 2 * p['score'] + 4 * e['score']
    runs.append({'prefix': prefix, 'result_ids': job['resultIDs'], 'performance': p['score'],
        'efficiency': e['score'], 'all_core': all_core['score'], 'efficiency_lower_bounds': bounds,
        'efficiency_ghz': clocks, 'efficiency_busy_fractions': busy,
        'helper_performance_lower_bounds': helper_bounds, 'weighted_estimate': estimate,
        'estimate_difference_percent': (all_core['score'] - estimate) / all_core['score'] * 100})
prior = ROOT.parent / 'a15-cpu-score-ui-20261014'
selected = json.loads((prior / 'selected-results.json').read_text())
curated = json.loads((prior / 'curated-suite-results.json').read_text())
old_cpu_ids = {r['id'] for r in selected['cpu_compute']}
selected['cpu_compute'] = [by_id[i] for i in runs[-1]['result_ids']]
curated = [r for r in curated if r['id'] not in old_cpu_ids] + selected['cpu_compute']
assert len(curated) == 33 and len({r['id'] for r in curated}) == 33
assert {r['id'] for r in curated}.issubset(by_id)
(ROOT / 'selected-results.json').write_text(json.dumps(selected, indent=2) + '\n')
(ROOT / 'curated-suite-results.json').write_text(json.dumps(curated, indent=2) + '\n')
verification = {'old_rows_unchanged': 113, 'history_rows': 119, 'new_rows': 6,
    'curated_rows': len(curated), 'runs': runs,
    'efficiency_repeat_difference_percent': abs(runs[1]['efficiency'] - runs[0]['efficiency']) / runs[0]['efficiency'] * 100,
    'before_sha256': hashlib.sha256(before_path.read_bytes()).hexdigest(),
    'after_sha256': hashlib.sha256(after_path.read_bytes()).hexdigest()}
(ROOT / 'verification.json').write_text(json.dumps(verification, indent=2) + '\n')
print(json.dumps(verification, indent=2))
