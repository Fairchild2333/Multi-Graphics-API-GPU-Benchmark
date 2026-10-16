from pathlib import Path
import json, math, re, hashlib, csv
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

root=Path(__file__).resolve().parent
phone=root/'final'
suite=json.loads((phone/'suite-runs/latest.json').read_text())
allrows=json.loads((phone/'results/results.json').read_text())
byid={r['id']:r for r in allrows}
rows=[byid[i] for j in suite['jobs'] for i in j['resultIDs']]
(root/'suite-results.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2)+'\n')
config=lambda row:dict(s.split('=',1) for s in row['workloadConfig'].split(';') if '=' in s)
cpu=next(r for r in rows if r['workload']=='cpu_membw')
gpu=next(r for r in rows if r['workload']=='membw')
c=config(cpu); g=config(gpu)
curves=[]
for key,text in c.items():
 if not key.startswith('curve_') or not text: continue
 for pair in text.split(','):
  x,y=map(float,pair.split(':')); curves.append((key[6:],x*1024,y))
for key,text in g.items():
 m=re.fullmatch(r'(device_local|host_visible)_(read_only|read_modify_write)_(\d+)',key)
 if m: curves.append((m[1]+'/'+m[2],int(m[3]),float(text)))
with (root/'memory-curve-points.csv').open('w') as f:
 w=csv.writer(f);w.writerow(['series','working_set_bytes','GB_per_second']);w.writerows(curves)

plt.rcParams.update({'font.size':10,'axes.spines.top':False,'axes.spines.right':False})
fig,axes=plt.subplots(1,3,figsize=(16,5),layout='constrained')
colors={'read':'#2574c9','write':'#de822b','copy':'#289572'}
for ax,mode,title in zip(axes[:2],['single','all'],['CPU: one scheduler-managed thread','CPU: all 6 cores']):
 for kernel,color in colors.items():
  data=[(b/1048576,v) for s,b,v in curves if s==f'{mode}_{kernel}']
  if data: ax.plot(*zip(*data),label=kernel,color=color,lw=1.8)
 ax.set_title(title);ax.set_xscale('log',base=2);ax.set_xlabel('Working set per thread (MiB)')
 ax.set_xticks([4/1024,16/1024,64/1024,256/1024,1,4,16,64,256], ['4K','16K','64K','256K','1M','4M','16M','64M','256M'], fontsize=8)
 ax.set_ylabel('GB/s');ax.grid(True,alpha=.18);ax.legend()
for storage,style in [('device_local','-'),('host_visible','--')]:
 for access,color in [('read_only','#7256ae'),('read_modify_write','#b65580')]:
  data=sorted((b/1048576,v) for s,b,v in curves if s==storage+'/'+access)
  axes[2].plot(*zip(*data),ls=style,color=color,lw=1.8,label=storage.replace('_',' ')+' '+('read' if access=='read_only' else 'RMW'))
axes[2].set_title('GPU: Metal fixed-grid memory diagnostic')
axes[2].set_xscale('log',base=2);axes[2].set_xlabel('Working set (MiB)');axes[2].set_ylabel('GB/s')
axes[2].set_xticks([64/1024,256/1024,1,4,16,64,256], ['64K','256K','1M','4M','16M','64M','256M'])
axes[2].grid(True,alpha=.18);axes[2].legend(fontsize=8)
fig.suptitle('iPhone 13 Pro · CPU and GPU saved memory curves',fontsize=15)
fig.text(.5,-.035,'K = KiB, M = MiB · CPU: median of 7 measurements · GPU: best of 5; DRAM plateau unverified · GPU cache levels not attributed',ha='center',fontsize=9)
fig.savefig(root/'memory-curves.png',dpi=180,bbox_inches='tight')
fig.savefig(root/'memory-curves.pdf',bbox_inches='tight')
plt.close(fig)

verification={'suiteStatus':suite['status'],'jobs':len(suite['jobs']),'passedJobs':sum(j['status']=='passed' for j in suite['jobs']),
 'selectedRecords':len(rows),'rawHistoryRecords':len(allrows),'resumedFrom':suite.get('resumedFrom'),
 'deviceModels':sorted({r.get('deviceModel','') for r in rows}),'platforms':sorted({r.get('platform','') for r in rows}),
 'gpuCurvePoints':sum('/' in p[0] for p in curves),'cpuCurvePoints':sum('/' not in p[0] for p in curves),
 'pageMappingMeasured':c.get('pageFaultMeasured')=='1','storageMeasured':c.get('storageMeasured')=='1',
 'maxThermalState':max(j['thermalStateEnd'] for j in suite['jobs']),
 'rawResultsSha256':hashlib.sha256((phone/'results/results.json').read_bytes()).hexdigest(),
 'selectedResultsSha256':hashlib.sha256((root/'suite-results.json').read_bytes()).hexdigest()}
(root/'verification.json').write_text(json.dumps(verification,ensure_ascii=False,indent=2)+'\n')

labels=['粒子 65,536 · 渲染','粒子 65,536 · 纯计算','粒子 1,048,576 · 渲染','粒子 1,048,576 · 纯计算',
 '粒子 4,194,304 · 渲染','粒子 4,194,304 · 纯计算','粒子 16,777,216 · 渲染','粒子 16,777,216 · 纯计算',
 'GPU Burn · 16 步','GPU Burn · 64 步','GPU Burn · 256 步','Cinematic Liquid','GPU 显存／缓存扫描','CPU 算力','CPU 缓存／内存／页面映射／存储']
text=['# iPhone 13 Pro 完整实机测试\n',f"15 项全部通过，选取 16 条有效成绩。手机原始历史库共 {len(allrows)} 条：保留首次结束时被标为取消的 CPU 内存记录，完整流程补跑该项后通过。13 项 GPU 和 CPU 算力的首次通过记录原样沿用，补跑前的原始报告和日志保存在 attempt-1。\n",
 f"设备：iPhone 13 Pro（iPhone14,2），{rows[0]['osVersion']}，系统报告 {rows[0].get('systemMemoryMB',0)/1024:.2f} GiB 内存，Release 真机安装包。全部 GPU 测试原生 Metal。\n",
 'GPU 粒子、GPU Burn、流体每项运行 15 秒（含 2 秒预热），固定 1280 × 720，关闭垂直同步。CPU 算力使用 15 秒测量、0.2 秒预热、3 轮；全核 6 线程，随后做 6 次由系统调度的单线程采样。这些采样没有绑定物理核心，也不能据此区分大核和小核。所有成绩保留 `_ios_preview` 合约。\n',
 '连续负载中，GPU Burn 64/256 步进入 fair（1），流体、GPU 扫描和 CPU 算力进入 serious（2）；因此这轮包含热机影响，不代表冷机峰值。补跑内存项的温度状态见下表。\n',
 '| 项目 | 成绩 | 温度状态（开始 → 结束） |\n|---|---:|---|']
for label,j in zip(labels,suite['jobs']):
 for i in j['resultIDs']:
  r=byid[i];name=label
  if r['workload']=='cpu_single_core': name+=' · 单线程采样平均'
  if r['workload']=='cpu_multi_core': name+=' · 全核'
  text.append(f"| {name} | {r['score']:.3f} {r['scoreUnit']} | {j['thermalStateStart']} → {j['thermalStateEnd']} |")
text+=['\n## CPU 缓存与内存\n','下表的缓存容量来自系统报告；线程由系统调度，无法把同一组缓存结果归属于某一个大核或小核。全核曲线的横轴表示每线程数据量。读／写／复制的单位均为 GB/s。\n',
 '| 模式 | 区间 | 读取 | 写入 | 复制 |\n|---|---|---:|---:|---:|']
for mode in ['single','all']:
 for level in ['L1','L2','L3','memory']:
  if mode+'_'+level+'_read' not in c: continue
  prefix=mode+'_'+level+'_'
  text.append(f"| {'单线程' if mode=='single' else '全核'} | {'内存' if level=='memory' else level} | {c[prefix+'read']} | {c[prefix+'write']} | {c[prefix+'copy']} |")
text+=[f"\nCPU 保存 {verification['cpuCurvePoints']} 个读／写／复制曲线点；单线程最大数据集 {c.get('maxDatasetMiB')} MiB。内存平台标志：单线程 `{c.get('single_memory_settled')}`，全核 `{c.get('all_memory_settled')}`。缓存容量和曲线标签不构成 GPU L2/L3 或 SoC/SLC 的独立测量。\n",
 f"页面映射／首次触页：**{c.get('pageFaultGBps')} GB/s**，每 {int(c.get('pageBytes','0'))//1024} KiB 页面 **{c.get('pageFaultNsPerPage')} ns**。测量的是新页面首次写入，不是磁盘交换。\n",
 f"存储：1 GiB 临时文件，直接 I/O 标志 `{c.get('storageDirectIo')}`；顺序读 **{c.get('storageSeqReadMBps')} MB/s**、顺序写 **{c.get('storageSeqWriteMBps')} MB/s**；4 KiB 随机 QD1 读 **{c.get('storageRandReadMBps')} MB/s**、写 **{c.get('storageRandWriteMBps')} MB/s**。本测试的 iOS 实现没有报告磁盘分页目标，文件速度不应当解释为已测得的虚拟内存换页速度。\n",
 '## GPU 缓存／内存\n',f"已保存 {verification['gpuCurvePoints']} 个曲线点，包含 device-local／host-visible 两种缓冲及 read-only／read-modify-write 两种访问。使用既有 Metal 粒子固定网格诊断，5 次取最好值，`memoryPlateauVerified=0`；**它尚不能提供 GPU L1/L2/L3、SoC/SLC 的独立速度或容量**。GPU 扫描分数不应直接当成硬件标称 DRAM 带宽。\n",
 '![CPU 与 GPU 内存曲线](memory-curves.png)\n',
 '## 保存与验证\n','- `suite-results.json`：本套选取的 16 条有效成绩。\n- `final/results/results.json`：从手机完整取回的原始历史库。\n- `final/suite-runs/latest.json`：15 项通过清单、成绩 ID、时间和温度状态；`resumedFrom` 指向首轮。\n- `final/suite-runs/*.log`：原生引擎输出，包括所有曲线点和 CPU 单线程采样。\n- `attempt-1`：首轮的原始报告、成绩和日志。\n- `memory-curves.png/.pdf` 与 `memory-curve-points.csv`：由保存成绩生成的图和原始点。\n- `verify/check`：使用 iOS 应用的真实成绩模型和曲线解析器核对清单、API、工作量、预览合约及曲线恢复。\n',
 '已有原生桥接回归测试在 iOS 18.5 模拟器 5/5 通过，模拟器成绩不计入这份手机性能报告。\n']
(root/'report.md').write_text('\n'.join(text))
print(json.dumps(verification,ensure_ascii=False,indent=2))
