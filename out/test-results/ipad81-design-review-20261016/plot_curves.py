"""Plot archived real measurements without changing result files."""
import csv
import json
import math
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT.parent/'a15-targeted-retest-20261014/plot-deps'))
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

rows=json.loads((ROOT/'selected-results.json').read_text())
sources=json.loads((ROOT/'selected-run-sources.json').read_text())
logs={}
for source in dict.fromkeys(sources.values()):
    log=(ROOT/'phone-final/suite-runs'/f"{source}.log").read_text()
    parts=re.split(r'\n=== \d+/\d+ ([^\n]+): (?:passed|review|failed|cancelled) ===\n',log)
    for i in range(1,len(parts),2):
        if sources.get(parts[i])==source:logs[parts[i]]=parts[i+1]

def fields(row):
    return dict(p.split('=',1) for p in row.get('workloadConfig','').split(';') if '=' in p)

def points(value):
    result=[]
    for item in value.split(','):
        p=item.split(':')
        if len(p)!=2: continue
        x,y=map(float,p)
        if x>0 and y>0 and math.isfinite(x) and math.isfinite(y): result.append((x,y))
    return result

def style(ax):
    ax.grid(alpha=.2)
    ax.spines[['top','right']].set_visible(False)

def save(fig,name):
    fig.savefig(ROOT/f'{name}.png',dpi=140)
    fig.savefig(ROOT/f'{name}.pdf')
    plt.close(fig)

def csv_save(name,data):
    if not data:return
    with (ROOT/f'{name}.csv').open('w') as stream:
        writer=csv.DictWriter(stream,fieldnames=list(data[0]))
        writer.writeheader(); writer.writerows(data)

cpu=next((r for r in rows if r['workload']=='cpu_membw'),None)
if cpu:
    f=fields(cpu); exported=[]
    fig,axes=plt.subplots(1,3,figsize=(15,4.6),layout='constrained')
    for ax,mode,title in zip(axes,['single','single_1','all'],['Performance core','Efficiency core (background)','All cores']):
        for kernel in ['read','write','copy']:
            p=points(f.get('curve_'+mode+'_'+kernel,''))
            ax.plot([x for x,y in p],[y for x,y in p],label=kernel,marker='.',markersize=3)
            exported.extend(dict(mode=mode,kernel=kernel,KiB=x,GBps=y) for x,y in p)
        ax.set(xscale='log',title=title,xlabel='KiB per thread',ylabel='GB/s')
        ax.legend(); style(ax)
    fig.suptitle('iPad8,1 / A12X | CPU cache and memory | E uses background QoS')
    save(fig,'cpu-memory-curves'); csv_save('cpu-memory-curves',exported)

fig,axes=plt.subplots(1,2,figsize=(13,4.8),layout='constrained'); exported=[]
for ax,api in zip(axes,['Metal','Vulkan']):
    row=next((r for r in rows if r['workload']=='membw' and r['graphicsApi']==api),None)
    if not row: ax.set_visible(False); continue
    f=fields(row)
    series={}
    if api=='Metal':
        for storage in ['device_local','host_visible']:
            for access in ['read_only','read_modify_write']:
                prefix=storage+'_'+access+'_'
                series[storage+'/'+access]=sorted((float(k[len(prefix):]),float(v)) for k,v in f.items() if k.startswith(prefix) and k[len(prefix):].isdigit())
    else:
        for kernel in ['read','write','copy','host_read','host_write']:
            series[kernel]=[(x*1048576,y) for x,y in points(f.get('curve_'+kernel,''))]
    for name,p in series.items():
        if not p:continue
        ax.plot([x/1048576 for x,y in p],[y for x,y in p],label=name,marker='.',markersize=3)
        exported.extend(dict(api=api,series=name,bytes=x,GBps=y) for x,y in p)
    ax.set(xscale='log',xlabel='Dataset MiB',ylabel='GB/s',title=api+(' (MoltenVK)' if api=='Vulkan' else ''))
    ax.legend(fontsize=8); style(ax)
fig.suptitle('iPad8,1 / A12X | GPU memory curves | cache levels remain unverified')
save(fig,'gpu-memory-curves');csv_save('gpu-memory-curves',exported)

cache=next((r for r in rows if r['workload']=='gpu_cache'),None)
if cache:
    f=fields(cache);exported=[]
    fig,axes=plt.subplots(1,2,figsize=(13,4.8),layout='constrained')
    for condition in ['gpu_warm','eviction_pass','cpu_warm','parallel_gpu_warm']:
        p=points(f.get('curve_'+condition,''))
        parallel=condition.startswith('parallel_');ax=axes[int(parallel)]
        ax.plot([x for x,y in p],[y for x,y in p],label=condition,marker='.',markersize=3)
        exported.extend(dict(condition=condition,KiB=x,value=y,unit='GB/s' if parallel else 'ns/load') for x,y in p)
    for ax,title,unit in zip(axes,['Dependent read','Parallel uint32 payload'],['ns/load','GB/s']):
        ax.set(xscale='log',xlabel='Address range KiB',ylabel=unit,title=title)
        ax.legend(fontsize=8);style(ax)
    fig.suptitle('iPad8,1 / A12X | GPU cache-state diagnostic | unverified attribution')
    save(fig,'gpu-cache-curves');csv_save('gpu-cache-curves',exported)

pattern=re.compile(r'\[GPU Timing\] Compute: ([0-9.]+) ms \| Render: ([0-9.]+) ms \| Total: ([0-9.]+) ms \| FPS: (\d+) \| T: ([0-9.]+) s(.*)')
timed=[]
for row in rows:
    if row['graphicsApi']=='CPU': continue
    p=[];f=fields(row)
    for match in pattern.finditer(logs.get(row['job'],'')):
        compute,render,total,fps,seconds=map(float,match.groups()[:5])
        if row['workload']=='stream' and compute>0:
            rate=40*row['particleCount']/compute/1e6
        elif row['workload']=='gpu_burn' and render>0:
            rate=row['resWidth']*row['resHeight']*int(f.get('steps','16'))*2/render/1e6
        else:continue
        p.append(dict(job=row['job'],seconds=seconds,warmup='warm-up' in match.group(6),computeMs=compute,renderMs=render,totalMs=total,rate=rate,unit=row['scoreUnit']))
    if p:timed.append((row,p))
if timed:
    fig,axes=plt.subplots(math.ceil(len(timed)/4),4,figsize=(16,math.ceil(len(timed)/4)*2.9),layout='constrained')
    for ax,(row,p) in zip(axes.flat,timed):
        ax.plot([v['seconds'] for v in p],[v['rate'] for v in p],marker='.',color='#d97924' if row['graphicsApi']=='Vulkan' else '#2e75b6')
        ax.axvspan(0,2,color='#ccd6df',alpha=.4)
        ax.set(title=f"{row['job']}\navg {row['score']:.3f}",xlabel='Elapsed seconds',ylabel=row['scoreUnit']);style(ax)
    for ax in list(axes.flat)[len(timed):]:ax.set_visible(False)
    fig.suptitle('iPad8,1 / A12X | selected full-suite records + targeted retests | rounded log-window rates')
    save(fig,'gpu-timing-curves');csv_save('gpu-timing-curves',[p for row,group in timed for p in group])
print('Exported archived real CPU/GPU curves')
