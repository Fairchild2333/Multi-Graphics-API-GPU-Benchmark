"""Audit actual device archives; this does not create benchmark scores."""
import argparse
import hashlib
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parent

def fields(row):
    return dict(v.split('=', 1) for v in row['workloadConfig'].split(';') if '=' in v)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--prefix', default='measured')
    parser.add_argument('--partial', action='store_true')
    args = parser.parse_args()
    plan = json.loads((ROOT / 'run-plan.json').read_text())
    selected_runs = json.loads((ROOT / 'selected-runs.json').read_text()) if not args.partial else None
    manifests = [v['manifest'] for v in selected_runs] if selected_runs is not None else json.loads((ROOT / f'{args.prefix}-manifests.json').read_text())
    if not args.partial:
        assert [m['jobs'][0]['id'] for m in manifests] == plan['jobs']
    last = manifests[-1]['jobs'][0]['id']
    folder = ROOT / f'{args.prefix}-{len(manifests):02d}-{last.replace("/", "-")}'
    before_path = ROOT / 'before/results/results.json'
    after_path = (folder / 'documents' if args.partial else ROOT / 'phone-final') / 'results/results.json'
    before = json.loads(before_path.read_text())
    after = json.loads(after_path.read_text())
    records = {r['id']: r for r in after}
    assert len(records) == len(after)
    assert all(records[r['id']] == r for r in before), 'Old history changed'
    audit = []
    selected = []
    measured_ids = set()
    cpu = {}
    memory = {}
    for index, report in enumerate(manifests, 1):
        job = report['jobs'][0]
        assert len(report['jobs']) == 1 and report['status'] in ('passed', 'review')
        assert job['status'] in ('passed', 'review')
        rows = [records[i] for i in job['resultIDs']]
        assert len(rows) == (3 if job['id'] == 'cpu_compute' else 1)
        run_folder = ROOT / selected_runs[index-1]['folder'] if selected_runs is not None else ROOT / f'{args.prefix}-{index:02d}-{job["id"].replace("/", "-")}'
        log_path = run_folder / 'documents/suite-runs' / (report['id'] + '.log')
        assert log_path.exists()
        for row in rows:
            assert row['deviceModel'] == 'iPad8,7' and row['platform'] == 'iOS'
            assert row['score'] > 0 and math.isfinite(row['score']) and not row['isSoftware']
            f = fields(row)
            measured_ids.add(row['id'])
            selected.append(dict(job=job['id'], **row))
            if row['workload'] in ('stream', 'gpu_burn'):
                assert row['durationSec'] >= 12 and row['timingSamples'] > 0
                assert row['avgComputeMs'] + row['avgRenderMs'] <= row['avgTotalGpuMs'] + .1
                assert row['avgTotalGpuMs'] <= row['avgFrameTimeMs'] * 1.1
                if row['workload'] == 'stream':
                    assert f['renderCoverage'] == 'wrapped_ndc_v2'
                if row['graphicsApi'] == 'Vulkan':
                    assert f['apiImplementation'] == 'MoltenVK' and f['translatedTo'] == 'Metal'
                    if row['workload'] == 'gpu_burn':
                        assert f['timingMode'] == 'synchronized_wall_clock'
            elif row['workload'] == 'membw':
                assert row['workloadVersion'].startswith('membw_v2_verified')
                assert f['dataVerified'] == '1' and int(f['writeCopyChecks']) > 0
                assert f['timestampAnomalies'] == '0' and f['cacheAttribution'] == 'unverified'
                assert f['writeCopyDispatch'] == 'one_owner_per_chunk'
                assert f['apiImplementation'] == 'MoltenVK' and f['translatedTo'] == 'Metal'
                memory['vulkan'] = {'score': row['score'], 'timing': f['timing'],
                    'scoreKind': f['score'], 'checks': int(f['writeCopyChecks']),
                    'curvePoints': sum(len(v.split(',')) for k, v in f.items() if k.startswith('curve_') and v)}
            elif row['workload'] in ('cpu_single_core', 'cpu_multi_core'):
                kind = 'all' if row['workload'] == 'cpu_multi_core' else f['coreType']
                cpu[kind] = row['score']
                if kind != 'all':
                    assert f['residencyVerified'] == '1'
                    bounds = [float(f[f'round{i}RequestedLowerBound']) for i in range(1, 4)]
                    assert min(bounds) >= .99
                    cpu[kind + 'MinimumResidency'] = min(bounds)
                    cpu[kind + 'GHz'] = [float(f[f'round{i}EffectiveGHz']) for i in range(1, 4)]
                else:
                    workers = [float(v) for v in f['workerScores'].split(',')]
                    assert len(workers) == 8 and abs(sum(workers) - row['score']) < .001
                    assert f['workerSchedulingPolicy'] == 'apple_pe_qos_v1'
                    cpu['allCorePlacementVerified'] = f['allCorePlacementVerified'] == '1'
                    cpu['allWorkerMinimumResidency'] = min(map(float, f['workerResidency'].split(',')))
                    cpu['allWorkerMinimumBusyFraction'] = min(map(float, f['workerBusyFractions'].split(',')))
                    if cpu['allCorePlacementVerified']:
                        assert cpu['allWorkerMinimumResidency'] >= .99
                        assert cpu['allWorkerMinimumBusyFraction'] >= .9
                if kind == 'efficiency':
                    cpu['efficiencyPolicy'] = f['efficiencyPolicy']
                    assert f['efficiencyPolicy'] in ('foreground_with_performance_load', 'foreground_utility_without_helpers', 'background')
                    assert f['scheduling'] in ('user_initiated', 'utility', 'background')
                    if f['scheduling'] == 'utility':
                        assert 'foreground_e_utility_v1' in row['workloadVersion'] and f['placementHelperThreads'] == '0'
                        assert min(float(f[f'round{i}WorkerCpuSeconds']) / 5 for i in range(1,4)) >= .90
                    for i in range(int(f.get('placementHelperThreads', '0'))):
                        assert float(f[f'helper{i}RequestedLowerBound']) >= .99
            elif row['workload'] == 'cpu_membw':
                assert f['coreType'] == 'both' and f['residencyVerified'] == '1'
                assert f['efficiencyPolicy'] in ('foreground_with_performance_load', 'foreground_utility_without_helpers', 'background')
                assert f['pageFaultMeasured'] == f['storageMeasured'] == '1'
                bounds = {}
                for mode in ['single', 'single_1']:
                    entries = [v.split(':') for v in f['residency_' + mode].split(',')]
                    bounds[mode] = min(float(v[2]) for v in entries)
                    assert bounds[mode] >= .99
                for mode in ['single', 'single_1', 'all']:
                    for kernel in ['read', 'write', 'copy']:
                        assert f['curve_' + mode + '_' + kernel]
                memory['cpu'] = {'score': row['score'], 'minimumResidency': bounds,
                    'efficiencyPolicy': f['efficiencyPolicy'],
                    'curvePoints': sum(len(v.split(',')) for k, v in f.items() if k.startswith('curve_') and v),
                    'pageFaultMeasured': True, 'storageMeasured': True,
                    'levelReadGBps': {k: float(v) for k, v in f.items() if k.endswith('_read') and not k.startswith('curve_')}}
        audit.append({'job': job['id'], 'status': job['status'], 'reviewReason': job['error'],
            'resultIDs': job['resultIDs'], 'thermalStart': job['thermalStateStart'],
            'thermalEnd': job['thermalStateEnd'], 'log': str(log_path.relative_to(ROOT))})
    old_ids = {r['id'] for r in before}
    recorded_attempt_ids = set()
    for p in ROOT.glob('*-manifests.json'):
        for m in json.loads(p.read_text()):
            for j in m['jobs']:
                recorded_attempt_ids.update(j['resultIDs'])
    assert set(records) - old_ids == recorded_attempt_ids
    assert measured_ids.issubset(recorded_attempt_ids)
    if not args.partial:
        assert len(manifests) == 14 and len(measured_ids) == 16
    verification = {'complete': not args.partial, 'jobs': len(manifests),
        'selectedNewRecords': len(measured_ids), 'newRecords': len(recorded_attempt_ids),
        'otherAttemptRecords': len(recorded_attempt_ids-measured_ids), 'historyBefore': len(before), 'historyAfter': len(after),
        'oldRecordsUnchanged': True, 'cpu': cpu, 'memory': memory,
        'reviewJobs': [e['job'] for e in audit if e['status'] == 'review'],
        'independentGpuCacheAttribution': 'out_of_scope_per_user',
        'beforeSha256': hashlib.sha256(before_path.read_bytes()).hexdigest(),
        'afterSha256': hashlib.sha256(after_path.read_bytes()).hexdigest()}
    name = 'interim-audit.json' if args.partial else 'verification.json'
    (ROOT / name).write_text(json.dumps(verification, indent=2) + '\n')
    (ROOT / 'audit.json').write_text(json.dumps(audit, indent=2) + '\n')
    (ROOT / 'selected-results.json').write_text(json.dumps(selected, indent=2) + '\n')
    print(json.dumps(verification, indent=2))

if __name__ == '__main__':
    main()
