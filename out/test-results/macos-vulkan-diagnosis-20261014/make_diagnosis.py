from pathlib import Path
import csv,json,re
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
root=Path(__file__).resolve().parent
status=json.loads((root/'status.json').read_text());results={r['id']:r for r in json.loads((root/'data/results/results.json').read_text())}
wrapped=json.loads((root/'wrapped-render-status.json').read_text());wrapped.update(headless=False,flights=2)
status.insert(4,wrapped)
results.update({r['id']:r for r in json.loads((root/'wrapped-render-data/results/results.json').read_text())})
pattern=re.compile(r'\[GPU Timing\] Compute: ([0-9.]+) ms \| Render: ([0-9.]+) ms \| Total: ([0-9.]+) ms \| FPS: (\d+) \| T: ([0-9.]+) s(.*)')
points=[];checks=[]
for e in status:
 r=results[e['resultID']];text=(root/(e['job']+'.log')).read_text()
 check=re.search(r'Timestamp check: ([0-9.]+) ms by the GPU.*\(([0-9.]+) ms with one frame behind the other\), ([0-9.]+) ms by wall clock',text)
 assert check, e['job']
 batch,queued,wall=map(float,check.groups())
 assert min(batch,queued)>.5*wall and max(batch,queued)<2*wall
 assert r['avgComputeMs']+r['avgRenderMs']<=r['avgTotalGpuMs']+.1 and r['avgTotalGpuMs']<=r['avgFrameTimeMs']*1.1
 checks.append({'job':e['job'],'resultID':r['id'],'gpuComputeBatchMs':batch,'queuedComputeBatchMs':queued,'synchronizedWallBatchMs':wall,'timestampCheckPassed':True,'gpuFrameRatio':r['avgTotalGpuMs']/r['avgFrameTimeMs'],'classification':e['classification'],'result':r})
 for line in text.splitlines():
  m=pattern.search(line)
  if m:
   compute,render,total,fps,seconds=map(float,m.groups()[:5])
   points.append({'job':e['job'],'seconds':seconds,'warmup':'warm-up' in m.group(6),'computeMs':compute,'renderMs':render,'totalGpuMs':total,'reportedFps':fps,'scoreGBs':40*1048576/compute/1e6})
(root/'audit.json').write_text(json.dumps(checks,ensure_ascii=False,indent=2)+'\n')
with (root/'curves.csv').open('w') as f:
 w=csv.DictWriter(f,fieldnames=list(points[0]));w.writeheader();w.writerows(points)
fig,axes=plt.subplots(len(status),2,figsize=(12,2.6*len(status)),layout='constrained')
for row,e in enumerate(status):
 r=results[e['resultID']];ps=[p for p in points if p['job']==e['job']];xs=[p['seconds'] for p in ps]
 l,a=axes[row]
 for key,label,color in [('computeMs','Compute','#276ab7'),('renderMs','Render','#d88a3c'),('totalGpuMs','Total GPU','#394b58')]: l.plot(xs,[p[key] for p in ps],label=label,color=color)
 a.plot(xs,[p['scoreGBs'] for p in ps],color='#258470',marker='.')
 l.set_title(('Wrapped display coordinates' if 'wrapped' in e['job'] else 'Compute only' if e['headless'] else 'Original compute + render')+f" | {e['flights']} frame(s) in flight")
 a.set_title(f"Mean {r['score']:.3f} GB/s | drop {r['throttlePct']:.2f}%")
 l.set_ylabel('Time (ms)');a.set_ylabel('Derived bandwidth (GB/s)');l.legend(fontsize=8)
 for ax in (l,a):
  ax.axvspan(0,2,color='#d9e0e5',alpha=.45);ax.set_xlim(0,15);ax.set_xlabel('Elapsed seconds');ax.grid(alpha=.2);ax.spines[['top','right']].set_visible(False)
fig.suptitle('M4 Pro Vulkan/MoltenVK control runs | 1M particles | 15 seconds',fontsize=14)
fig.supxlabel('Grey: 2s warm-up. Variants are separate diagnostic observations. Curves use rounded log values.',fontsize=9)
fig.savefig(root/'control-curves.png',dpi=170);fig.savefig(root/'control-curves.pdf')
fig2, axes2=plt.subplots(1,2,figsize=(12,4.5),layout='constrained')
for job,label,color in [('vulkan-render-flight2-baseline','Original before','#c3762d'),('vulkan-render-flight2-wrapped','Wrapped diagnostic','#1b8271'),('vulkan-render-flight2-reversal','Original restored','#4067ac')]:
 ps=[p for p in points if p['job']==job];x=[p['seconds'] for p in ps]
 axes2[0].plot(x,[p['scoreGBs'] for p in ps],label=label,color=color,marker='.')
 axes2[1].plot(x,[p['renderMs'] for p in ps],label=label,color=color,marker='.')
for ax in axes2:
 ax.axvspan(0,2,color='#d9e0e5',alpha=.45);ax.set_xlabel('Elapsed seconds');ax.set_xlim(0,15);ax.grid(alpha=.2);ax.legend(fontsize=9);ax.spines[['top','right']].set_visible(False)
axes2[0].set_ylabel('Derived compute bandwidth (GB/s)');axes2[1].set_ylabel('Render time (ms)')
fig2.suptitle('Vulkan 1M: original -> bounded display -> original | 2 frames in flight',fontsize=13)
fig2.supxlabel('Vertex-only diagnostic preserves screen coverage; it is a separate workload variant. All thermal states nominal.',fontsize=9)
fig2.savefig(root/'visibility-control.png',dpi=170);fig2.savefig(root/'visibility-control.pdf')
print('Controls audited:',len(checks))
