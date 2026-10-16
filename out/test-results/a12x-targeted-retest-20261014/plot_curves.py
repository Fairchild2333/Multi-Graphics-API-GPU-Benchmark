"""Export archived device curves; plotted GPU rates use rounded log windows."""
import csv
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT.parent / 'a15-targeted-retest-20261014/plot-deps'))
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

rows = json.loads((ROOT / 'selected-results.json').read_text())
audit = json.loads((ROOT / 'audit.json').read_text())
by_job = {r['job']: r for r in rows if r['graphicsApi'] != 'CPU'}
pattern = re.compile(r'\[GPU Timing\] Compute: ([0-9.]+) ms \| Render: ([0-9.]+) ms \| Total: ([0-9.]+) ms \| FPS: (\d+) \| T: ([0-9.]+) s(.*)')
gpu = []
fig, axes = plt.subplots(4, 3, figsize=(15, 12), layout='constrained')
for ax, entry in zip(axes.flat, [e for e in audit if 'stream-' in e['job'] or 'gpu_burn-' in e['job']]):
    row = by_job[entry['job']]
    f = dict(v.split('=', 1) for v in row['workloadConfig'].split(';') if '=' in v)
    points = []
    for line in (ROOT / entry['log']).read_text().splitlines():
        match = pattern.search(line)
        if not match:
            continue
        compute, render, total, fps, seconds = map(float, match.groups()[:5])
        rate = (40 * row['particleCount'] / compute / 1e6 if row['workload'] == 'stream'
                else 1280 * 720 * int(f['steps']) * 2 / render / 1e6)
        point = dict(job=entry['job'], resultID=row['id'], seconds=seconds,
            warmup='warm-up' in match.group(6), computeMs=compute, renderMs=render,
            totalMs=total, reportedFPS=fps, derivedRate=rate, unit=row['scoreUnit'])
        points.append(point)
        gpu.append(point)
    ax.plot([p['seconds'] for p in points], [p['derivedRate'] for p in points], marker='.',
            color='#2e75b6' if row['graphicsApi'] == 'Metal' else '#d97924')
    ax.axvspan(0, 2, color='#ccd6df', alpha=.4)
    ax.set_title(f"{entry['job']} | avg {row['score']:.3f}\n{entry['status']}", fontsize=10)
    ax.set_xlabel('Elapsed seconds')
    ax.set_ylabel(row['scoreUnit'])
    ax.grid(alpha=.2)
    ax.spines[['top', 'right']].set_visible(False)
axes.flat[-1].set_visible(False)
fig.suptitle('A12X targeted GPU retests | fixed coverage | Vulkan uses MoltenVK')
fig.supxlabel('Grey: warm-up. Curves use rounded log windows; saved JSON contains final scores.', fontsize=9)
fig.savefig(ROOT / 'gpu-curves.png', dpi=150)
fig.savefig(ROOT / 'gpu-curves.pdf')
with (ROOT / 'gpu-curves.csv').open('w') as stream:
    writer = csv.DictWriter(stream, fieldnames=list(gpu[0]))
    writer.writeheader()
    writer.writerows(gpu)

cpu = next(r for r in rows if r['workload'] == 'cpu_membw')
f = dict(v.split('=', 1) for v in cpu['workloadConfig'].split(';') if '=' in v)
fig, axes = plt.subplots(1, 3, figsize=(15, 4.6), layout='constrained')
cpu_points = []
for ax, mode, title in zip(axes, ['single', 'single_1', 'all'], ['Performance core', 'Efficiency core (background)', 'All cores']):
    for kernel, color in [('read', '#2e75b6'), ('write', '#d97924'), ('copy', '#438b58')]:
        points = [tuple(map(float, v.split(':'))) for v in f['curve_' + mode + '_' + kernel].split(',')]
        ax.plot([p[0] for p in points], [p[1] for p in points], label=kernel, color=color, marker='.', markersize=3)
        cpu_points.extend(dict(mode=mode, kernel=kernel, KiB=p[0], GBps=p[1]) for p in points)
    ax.set_xscale('log', base=2)
    ax.set_title(title)
    ax.set_xlabel('KiB per thread')
    ax.set_ylabel('GB/s')
    ax.grid(alpha=.2)
    ax.legend()
    ax.spines[['top', 'right']].set_visible(False)
fig.suptitle('A12X CPU cache and memory | verified P/E placement | E uses background QoS')
fig.savefig(ROOT / 'cpu-memory-curves.png', dpi=150)
fig.savefig(ROOT / 'cpu-memory-curves.pdf')
with (ROOT / 'cpu-memory-curves.csv').open('w') as stream:
    writer = csv.DictWriter(stream, fieldnames=list(cpu_points[0]))
    writer.writeheader()
    writer.writerows(cpu_points)
print('Exported GPU timing and CPU memory curves')
