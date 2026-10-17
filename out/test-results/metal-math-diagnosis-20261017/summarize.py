"""Reproduce the isolated diagnostic tables and check history preservation."""
from pathlib import Path
import csv
import hashlib
import json
import math
import re
import statistics

ROOT = Path(__file__).resolve().parent

def records(path, prefix):
    return [dict(v.split('=', 1) for v in line.split('\t')[1:])
            for line in path.read_text().splitlines() if line.startswith(prefix + '\t')]

def ax_table(name):
    text = (ROOT / name).read_text()
    headers, rows, values = [], [], None
    for line in text.splitlines():
        if 'column (disabled)' in line:
            headers.append(line.split('column (disabled) ', 1)[1].split(', ID:')[0])
        elif 'row (disabled)' in line:
            if values is not None:
                rows.append(values)
            values = []
        elif values is not None and ' text ' in line:
            values.append(line.split(' text ', 1)[1].strip())
        elif values is not None and 'scroll bar ' in line:
            rows.append(values)
            break
    if headers[0] == 'Thumbnails':
        rows = [[''] + row for row in rows]
    assert rows and all(len(row) == len(headers) for row in rows), (name, headers, rows)
    data = [dict(zip(headers, row)) for row in rows]
    with (ROOT / (Path(name).stem + '.csv')).open('w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=headers)
        writer.writeheader(); writer.writerows(data)
    return data

stages = records(ROOT / 'stage-output.log', 'MATHSTAGE')
controls = records(ROOT / 'discard-output.log', 'MATHSTAGE')
pairs = records(ROOT / 'fma-pair-output.log', 'FMAPAIR')
assert len(stages) == 30 and len(controls) == 24 and len(pairs) == 4
for name, summaries in [('stage-output.log', stages), ('discard-output.log', controls)]:
    samples = records(ROOT / name, 'STAGESAMPLE')
    assert len(samples) == len(summaries) * 7
    for row in summaries:
        selected = [float(p['seconds']) for p in samples if
                    all(p[k] == row[k] for k in ('case', 'phase', 'steps', 'passes'))]
        assert len(selected) == 7 and all(math.isfinite(v) and v > 0 for v in selected)
        assert abs(statistics.median(selected) - float(row['medianSeconds'])) < 2e-9
        rate = 1280 * 720 * int(row['steps']) * int(row['passes']) / statistics.median(selected) / 1e9
        assert abs(rate - float(row['GpixStepsPerSec'])) < 2e-5
    outputs = records(ROOT / name, 'STAGEOUTPUT')
    assert len(outputs) == 6 and all(int(p['colorMin']) < int(p['colorMax']) for p in outputs)

for filename, rows in [('fixed-stages.csv', stages), ('discard-controls.csv', controls), ('fma-pairs.csv', pairs)]:
    with (ROOT / filename).open('w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0])); writer.writeheader(); writer.writerows(rows)

shader = ax_table('xcode-shaders-ax.txt')
fragment = ax_table('xcode-fragment-counters-ax.txt')
discard_shader = ax_table('xcode-discard-shaders-ax.txt')
discard_fragment = ax_table('xcode-discard-fragment-counters-ax.txt')
full = next(r for r in fragment if 'Encoder 2' in r['Name'])
simple = next(r for r in fragment if 'Encoder 1' in r['Name'] and 'Render' in r['Name'])
full_control = next(r for r in discard_fragment if 'Encoder 2' in r['Name'])
removed = next(r for r in discard_fragment if 'Encoder 3' in r['Name'])
integer = lambda value: int(value.replace(',', '').strip())
assert integer(full['FS Invocations']) > 1280 * 720 * 2 * 1.4
assert integer(full_control['FS Invocations']) > 1280 * 720 * 2 * 1.4
assert integer(removed['FS Invocations']) < 1280 * 720 * 1.1

metadata = json.loads((ROOT / 'run-metadata.json').read_text())
for history in metadata['historiesBefore']:
    assert hashlib.sha256(Path(history['path']).read_bytes()).hexdigest() == history['sha256']
assert metadata['returnCode'] == 0
control_meta = json.loads((ROOT / 'controls-metadata.json').read_text())
assert all(r['returnCode'] == 0 for r in control_meta['runs'])
source = ROOT.parents[2] / 'scripts/metal-math-probe/burn_stage_probe.mm'
assert hashlib.sha256(source.read_bytes()).hexdigest() == control_meta['sourceSha256']
assert 'Finished running' in (ROOT / 'xcode-stopped-ax.txt').read_text()
buffered = statistics.mean(float(r['TFLOPS']) for r in pairs if r['case'] == 'buffered')
literal = statistics.mean(float(r['TFLOPS']) for r in pairs if r['case'] == 'literal')
result = {'verified': True, 'fixedStageCases': len(stages), 'discardControlCases': len(controls),
          'timedSamples': (len(stages) + len(controls)) * 7,
          'warmedPairedFmaTFLOPS': {'buffered': buffered, 'literal': literal,
                                  'literalGainPercent': (literal / buffered - 1) * 100},
          'initialCapture': {'fullFragmentInvocations': integer(full['FS Invocations']),
                             'twoSubmittedDrawsPixels': 1280 * 720 * 2,
                             'extraInvocationPercent': (integer(full['FS Invocations']) / (1280 * 720 * 2) - 1) * 100,
                             'fullFSOccupancy': full['FS Occupancy'], 'simpleFSOccupancy': simple['FS Occupancy']},
          'controlCapture': {'fullFragmentInvocations': integer(full_control['FS Invocations']),
                             'noDiscardFragmentInvocations': integer(removed['FS Invocations']),
                             'warning': 'Removing discard changes visibility; one of the two opaque draws is culled. Rates are submitted work, not two executed loops.'},
          'allFiveCanonicalHistoriesAndLiveMacUnchanged': True, 'xcodeReplayStopped': True}
(ROOT / 'verification.json').write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
print(json.dumps(result, ensure_ascii=False, indent=2))
