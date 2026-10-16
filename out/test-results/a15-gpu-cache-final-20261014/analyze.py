import json,csv,math
from pathlib import Path
import xml.etree.ElementTree as ET
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
root=Path(__file__).resolve().parent
before=json.loads((root/'before/results/results.json').read_text())
after=json.loads((root/'final-documents/results/results.json').read_text())
old={r['id']:r for r in before}; new=[r for r in after if r['id'] not in old]
assert all(next(r for r in after if r['id']==k)==v for k,v in old.items())
assert len(new)==3
curves={}
rows=[]
for r in new:
 d=dict(x.split('=',1) for x in r['workloadConfig'].split(';') if '=' in x)
 c={k.removeprefix('curve_'):[(int(pair.split(':')[0])*1024,float(pair.split(':')[1])) for pair in v.split(',')] for k,v in d.items() if k.startswith('curve_')}
 assert set(c)=={'gpu_warm','eviction_pass','cpu_warm','parallel_gpu_warm'}
 assert all(len(points)==31 and points[0][0]==4096 and points[-1][0]==128*1024*1024 and all(math.isfinite(v) and v>0 for _,v in points) for points in c.values())
 assert d['checksumValid']=='1'
 curves[r['id']]=c
 for condition,points in c.items():
  for size,value in points: rows.append([r['id'],d['steps'],condition,size,value,'GB/s_payload' if condition=='parallel_gpu_warm' else 'ns/load'])
with (root/'curves.csv').open('w') as f:
 w=csv.writer(f); w.writerow(['record_id','serial_steps','condition','address_span_bytes','median','unit']); w.writerows(rows)
info={}
for name in ['gpu-counter-info','gpu-counter-value','gpu-counter-profile']:
 tree=ET.parse(root/(name+'.xml')); info[name]=len(tree.findall('.//row'))
baseline,extended=new[1],new[2]
b=dict(curves[baseline['id']]['gpu_warm']); e=dict(curves[extended['id']]['gpu_warm'])
comparison={str(size):{'baseline_ns':b[size],'double_load_ns':e[size],'change_percent':100*(e[size]/b[size]-1)} for size in [4096,16384,1024*1024,2*1024*1024,32*1024*1024,128*1024*1024]}
audit={'old_rows_unchanged':len(old),'history_rows':len(after),'new_records':[r['id'] for r in new],'verified_curve_points':len(rows),'physical_test_passed':True,'public_counter_inventory':'timestamp/GPUTimestamp','external_trace_counter_profile':0,'external_counter_rows':info,'independent_L2_L3_SLC_attribution':'not_established','attribution_reason':'No cache hit/miss or SLC traffic evidence collected; timing curves and checksum alone do not identify hardware levels','gpu_warm_load_count_comparison':comparison,'phone_benchmark_ended':True}
(root/'verification.json').write_text(json.dumps(audit,indent=2))
fig,axes=plt.subplots(2,2,figsize=(12,8),constrained_layout=True)
for ax,r,title in zip(axes[0],[baseline,extended],['Baseline: 65,536 serial loads','Control: 131,072 serial loads']):
 for condition in ['gpu_warm','eviction_pass','cpu_warm']:
  points=curves[r['id']][condition]; ax.plot([x/1024 for x,_ in points],[y for _,y in points],marker='.',label=condition)
 ax.set_title(title); ax.set_ylabel('Effective dependent-read latency (ns/load)'); ax.legend(fontsize=8)
for r,label in zip(new,['Profiled baseline','Baseline','Double serial loads']):
 for ax,condition in [(axes[1,0],'gpu_warm'),(axes[1,1],'parallel_gpu_warm')]:
  points=curves[r['id']][condition];ax.plot([x/1024 for x,_ in points],[y for _,y in points],marker='.',label=label)
axes[1,0].set_title('GPU-warmed comparison');axes[1,0].set_ylabel('Effective latency (ns/load)')
axes[1,1].set_title('4,096 parallel chains: 8,192 loads each');axes[1,1].set_ylabel('Requested 4-byte payload (GB/s)');axes[1,1].set_yscale('log')
for ax in axes.flat:
 ax.set_xscale('log',base=2);ax.set_xlabel('Address span (KiB)');ax.grid(True,alpha=.25)
for ax in axes[1]:ax.legend(fontsize=8)
fig.suptitle('A15 GPU cache-state measurements: hardware L2/L3/SLC attribution unverified')
fig.savefig(root/'cache-curves.png',dpi=160);fig.savefig(root/'cache-curves.pdf');plt.close(fig)
print(json.dumps(audit,indent=2))
