from pathlib import Path
import json, re, csv
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
root=Path(__file__).resolve().parent
status=json.loads((root/'run-status.json').read_text())
records={r['id']:r for r in json.loads((root/'data/results/results.json').read_text())}
old=json.loads((root/'../all-device-comparison-20261014/retest-needed.json').read_text())
patterns=re.compile(r'\[GPU Timing\] Compute: ([0-9.]+) ms \| Render: ([0-9.]+) ms \| Total: ([0-9.]+) ms \| FPS: (\d+) \| T: ([0-9.]+) s(.*)')
labels={'metal-stream1m-render':'Metal particles 1M','metal-stream16m-render':'Metal particles 16M','metal-burn16':'Metal GPU Burn 16','metal-burn64':'Metal GPU Burn 64','vulkan-stream1m-render':'Vulkan particles 1M (MoltenVK)','vulkan-stream1m-render-repeat':'Vulkan particles 1M repeat (MoltenVK)'}
points=[];audit=[]
for entry in status:
 r=records[entry['resultID']]
 assert r['resWidth']==1280 and r['resHeight']==720
 assert r['durationSec']>=11 and r['warmupSec']==2 and r['timingSamples']>0
 assert r['platform']=='macOS' and r['deviceName']=='Apple M4 Pro' and not r['isSoftware']
 config=dict(s.split('=',1) for s in r['workloadConfig'].split(';') if '=' in s)
 if r['graphicsApi']=='Metal':
  assert r['framesInFlight']==1 and config['timingMode']=='serialized_command_buffer_gpu_time'
 else:
  assert r['framesInFlight']==2 and config['apiImplementation']=='MoltenVK' and config['timingMode']=='gpu_timestamp_query'
  m=re.search(r'Timestamp check: ([0-9.]+) ms by the GPU.* ([0-9.]+) ms by wall clock', (root/(entry['job']+'.log')).read_text())
  assert m and .5<float(m.group(1))/float(m.group(2))<2
 assert r['avgComputeMs']+r['avgRenderMs']<=r['avgTotalGpuMs']+.1
 assert r['avgTotalGpuMs']<=r['avgFrameTimeMs']*1.1
 windows=[]
 for line in (root/(entry['job']+'.log')).read_text().splitlines():
  m=patterns.search(line)
  if not m: continue
  compute,render,total,fps,seconds=map(float,m.groups()[:5]);warm='warm-up' in m.group(6)
  if r['workload']=='stream': value=40*r['particleCount']/compute/1e6 if compute>0 else 0
  else: value=1280*720*int(config['steps'])*2/render/1e6 if render>0 else 0
  point={'job':entry['job'],'seconds':seconds,'warmup':warm,'computeMs':compute,'renderMs':render,'totalGpuMs':total,'reportedFps':fps,'derivedScore':value,'scoreUnit':r['scoreUnit']}
  points.append(point);windows.append(point)
 early=[p['derivedScore'] for p in windows if not p['warmup']][:5]
 late=[p['derivedScore'] for p in windows if not p['warmup']][-5:]
 audit.append({'job':entry['job'],'resultID':r['id'],'contractValid':True,'gpuFrameRatio':r['avgTotalGpuMs']/r['avgFrameTimeMs'],'windowCount':len([p for p in windows if not p['warmup']]),'thermalBefore':entry['thermalBefore'],'thermalAfter':entry['thermalAfter'],'reviewReasons':entry['reviewReasons'],'result':r})
(root/'audit.json').write_text(json.dumps(audit,ensure_ascii=False,indent=2)+'\n')
with (root/'curves.csv').open('w') as f:
 writer=csv.DictWriter(f,fieldnames=list(points[0]));writer.writeheader();writer.writerows(points)
fig,axes=plt.subplots(len(status),2,figsize=(13,2.7*len(status)),layout='constrained')
for row,e in enumerate(status):
 r=records[e['resultID']];ps=[p for p in points if p['job']==e['job']];xs=[p['seconds'] for p in ps]
 left,right=axes[row]
 for key,label,color in [('computeMs','Compute','#236fbb'),('renderMs','Render','#dd853a'),('totalGpuMs','Total GPU','#334755')]:
  left.plot(xs,[p[key] for p in ps],label=label,color=color,marker='.',linewidth=1.4)
 right.plot(xs,[p['derivedScore'] for p in ps],color='#167964',marker='.',linewidth=1.7)
 left.set_title(labels[e['job']]);left.set_ylabel('Time (ms)');right.set_ylabel(r['scoreUnit'])
 right.set_title(f"Average {r['score']:.3f} / stable {r['stableScore']:.3f} | drop {r['throttlePct']:.2f}%")
 for ax in (left,right):
  ax.axvspan(0,2,color='#d4dce2',alpha=.45);ax.set_xlim(0,15);ax.grid(alpha=.2);ax.set_xlabel('Elapsed seconds');ax.spines[['top','right']].set_visible(False)
 left.legend(loc='best',fontsize=8)
fig.suptitle('M4 Pro targeted retests | 1280x720 | 15s run incl. 2s warm-up',fontsize=15)
fig.supxlabel('Grey: warm-up. Curves use rounded per-window logs; saved result JSON contains the final measurements. Vulkan uses MoltenVK translation.',fontsize=9)
fig.savefig(root/'mac-retest-curves.png',dpi=170)
fig.savefig(root/'mac-retest-curves.pdf')
print('Contract audit and curves saved:',len(audit),'runs,',len(points),'window points')
