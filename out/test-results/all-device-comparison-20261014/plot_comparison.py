from pathlib import Path
import json
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
root=Path(__file__).resolve().parent
rows=json.loads((root/'apple-metrics.json').read_text())
metrics=['CPU 全核','Metal 粒子16,777,216纯计算','GPU Vulkan 顺序读取']
titles=['CPU all threads · MWork/s','Metal 16M compute · GB/s','Vulkan sequential read · GB/s']
fig,axes=plt.subplots(1,3,figsize=(12,4.3),layout='constrained')
colors=['#397dad','#bd8347','#739b68']
for ax,title,key in zip(axes,titles,metrics):
 r=next(r for r in rows if r['metric']==key)
 vals=[r[d] for d in ['mac','iphone','ipad']]
 ax.barh(['Mac M4 Pro','iPhone A15','iPad A12X'],vals,color=colors)
 ax.invert_yaxis();ax.set_title(title,fontsize=11)
 ax.set_xlim(0,max(vals)*1.29)
 for i,v in enumerate(vals):ax.text(v+max(vals)*.03,i,f'{v:,.1f}',va='center',fontsize=10)
 ax.spines[['top','right']].set_visible(False)
fig.suptitle('Saved observations · current Mac and latest selected mobile results',fontsize=14)
fig.supxlabel('Different desktop / iOS contracts; 14 / 6 / 8 CPU threads. No overall score.\nMac provisional render scores excluded.',fontsize=9)
fig.savefig(root/'apple-comparison.png',dpi=160)
fig.savefig(root/'apple-comparison.pdf')
