from pathlib import Path
import csv
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
base=Path(__file__).resolve().parent
with (base/'curve-points.csv').open() as file: rows=list(csv.DictReader(file))
fig,axes=plt.subplots(1,3,figsize=(15,4.4),layout='constrained')
colors={'read':'#2878B5','write':'#E76F28','copy':'#249F75'}
for ax,mode,title in zip(axes,['single','single_1','all'],['Performance core · 1 thread','Efficiency core · 1 thread (background)','All 14 threads · aggregate']):
    for kernel in ['read','write','copy']:
        points=[r for r in rows if r['mode']==mode and r['kernel']==kernel]
        ax.plot([float(r['block_KiB']) for r in points],[float(r['GBps']) for r in points],color=colors[kernel],label=kernel.title(),lw=1.8)
    ax.set_xscale('log',base=2);ax.set_yscale('log');ax.set_xlabel('Working set per thread (KiB)')
    ax.set_ylabel('Bandwidth (GB/s)');ax.set_title(title,fontsize=11);ax.grid(alpha=.2,which='both');ax.legend(fontsize=9)
    if mode!='all':
        for label,capacity in [('L1',128 if mode=='single' else 64),('L2',16384 if mode=='single' else 4096)]:
            ax.axvline(capacity,color='#777',ls='--',lw=.8,alpha=.6)
            ax.text(capacity,.97,label,transform=ax.get_xaxis_transform(),va='top',ha='right',color='#555',fontsize=9)
fig.suptitle('Apple M4 Pro · verified P/E cache and memory curves · 2026-10-13',fontsize=14)
fig.savefig(base/'cpu-core-type-curves.png',dpi=180)
fig.savefig(base/'cpu-core-type-curves.pdf')
