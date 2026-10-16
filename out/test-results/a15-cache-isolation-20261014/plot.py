from pathlib import Path
import csv,json,statistics
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
root=Path(__file__).resolve().parent
plt.rcParams.update({'font.family':'DejaVu Sans','font.size':10,'axes.spines.top':False,'axes.spines.right':False,'savefig.dpi':180})
def curves(path):
 r=json.loads(path.read_text())[-1]
 c=dict(x.split('=',1) for x in r['workloadConfig'].split(';') if '=' in x)
 return {k[6:]:{int(t.split(':')[0])/1024:float(t.split(':')[1]) for t in v.split(',')} for k,v in c.items() if k.startswith('curve_')}
b=curves(root/'phone-after-controls/results/results.json');a=curves(root/'phone-after-alu/results/results.json');f=curves(root/'phone-final/results/results.json')
fig,axes=plt.subplots(2,2,figsize=(12,8.3),layout='constrained')
colors={'gpu_warm':'#2563eb','cpu_warm':'#d97706','eviction_pass':'#dc2626'}
for k,label in [('gpu_warm','GPU warm'),('cpu_warm','CPU warm'),('eviction_pass','After control reads')]:
 axes[0,0].plot(list(f[k]),list(f[k].values()),label=label,color=colors[k],lw=2)
 for tail,ls in [('', '-'),('_revisit','--')]:
  axes[0,1].plot(list(a[k+tail]),list(a[k+tail].values()),label=label+(' first' if not tail else ' revisit'),color=colors[k],ls=ls,lw=1.8)
axes[0,0].set_title('A. Default suite: repeated dependent loads')
axes[0,1].set_title('B. First pass / immediate revisit (ALU control)')
raw=list(csv.DictReader((root/'raw-controls-alu.csv').open()))
for target,col in [(2,'#2563eb'),(8,'#d97706')]:
 vals=[];errors=[]
 for dose in [0,2,8,32,128]:
  cond='gpu_warm' if not dose else f'gpu_warm_gpu_evict_{dose}m'
  v=sorted(float(r['first_ns']) for r in raw if r['condition']==cond and int(r['bytes'])==target*1048576)
  m=statistics.median(v);vals.append(m);errors.append((m-v[1],v[-2]-m))
 axes[1,0].errorbar(range(5),vals,yerr=list(zip(*errors)),color=col,marker='o',capsize=3,label=f'{target} MiB target')
axes[1,0].set_xticks(range(5),['0','2','8','32','128']);axes[1,0].set_xlabel('GPU control read dose (MiB)');axes[1,0].set_title('C. GPU eviction dose: first-pass median + middle 5/7')
for cond,col,label in [('eviction_pass','#dc2626','After control reads'),('cpu_warm','#d97706','CPU warm')]:
 for source,ls,suffix in [(b,':','baseline'),(a,'-','ALU-conditioned')]:
  axes[1,1].plot(list(source[cond]),list(source[cond].values()),color=col,ls=ls,lw=2,label=f'{label}: {suffix}')
axes[1,1].set_title('D. Work-state sensitivity: same first-pass addresses')
for i,ax in enumerate(axes.flat):
 ax.set_ylabel('GPU interval / dependent load (ns)');ax.grid(alpha=.18);ax.legend(frameon=False,fontsize=8)
 if i!=2:ax.set_xscale('log',base=2);ax.set_xlabel('Address span (MiB)')
fig.suptitle('Physical iPhone 13 Pro / A15 - native Metal cache-state evidence',fontsize=15)
fig.supxlabel('All checksums passed; thermal state 0 -> 0. No independent L2/L3/SLC or peak-bandwidth attribution.',fontsize=10)
fig.savefig(root/'cache-controls.png');fig.savefig(root/'cache-controls.pdf')
print('Saved PNG and PDF')
