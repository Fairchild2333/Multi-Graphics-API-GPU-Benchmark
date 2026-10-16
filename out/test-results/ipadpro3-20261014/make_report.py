import json, math, hashlib, re, csv
from pathlib import Path
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
root=Path(__file__).resolve().parent
phone=root/'final-phone'
suite=json.loads((phone/'suite-runs/latest.json').read_text())
raw=json.loads((phone/'results/results.json').read_text())
byid={r['id']:r for r in raw}
rows=[byid[i] for job in suite['jobs'] for i in job['resultIDs']]
(root/'suite-results.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2)+'\n')
config=lambda r:dict(p.split('=',1) for p in r.get('workloadConfig','').split(';') if '=' in p)
cpu=next(r for r in rows if r['workload']=='cpu_membw'); c=config(cpu)
cache=next(r for r in rows if r['workload']=='gpu_cache'); gc=config(cache)
curves=[]
for key,text in c.items():
 if not key.startswith('curve_') or not text:continue
 for point in text.split(','):
  x,y=map(float,point.split(':'));curves.append(('CPU',key[6:],x*1024,y,'GB/s'))
for row in rows:
 if row['workload']=='membw':
  d=config(row)
  for key,value in d.items():
   if row['graphicsApi']=='Metal':
    m=re.fullmatch(r'(device_local|host_visible)_(read_only|read_modify_write)_(\d+)',key)
    if m:curves.append(('Metal',m[1]+'/'+m[2],int(m[3]),float(value),'GB/s'))
   elif key.startswith('curve_') and value:
    for point in value.split(','):
     x,y=map(float,point.split(':'));curves.append(('Vulkan',key[6:],x*(1024 if key in ('curve_l1','curve_l1_single') else 1048576),y,'GB/s'))
for key,value in gc.items():
 if key.startswith('curve_') and value:
  for point in value.split(','):
   x,y=map(float,point.split(':'));curves.append(('GPU cache',key[6:],x*1024,y,'GB/s payload' if 'parallel' in key else 'ns/load'))
with (root/'curve-points.csv').open('w') as f:
 w=csv.writer(f);w.writerow(['API','series','working_set_bytes','value','unit']);w.writerows(curves)
plt.rcParams.update({'font.size':10,'axes.spines.top':False,'axes.spines.right':False})
fig,axes=plt.subplots(2,3,figsize=(16,9),layout='constrained')
for ax,mode in zip(axes[0,:2],['single','all']):
 for kernel,color in [('read','#2678c7'),('write','#dc802d'),('copy','#269674')]:
  p=[(b/1048576,y) for api,series,b,y,u in curves if api=='CPU' and series==mode+'_'+kernel]
  ax.plot(*zip(*p),label=kernel,color=color)
 ax.set_title('CPU: '+('one scheduler-managed thread' if mode=='single' else 'all '+c.get('allCoreThreads','8')+' cores'))
 ax.set_ylabel('GB/s');ax.set_xscale('log',base=2);ax.set_xlabel('Working set per thread')
 ax.set_xticks([4/1024,64/1024,1,16,256],['4 KiB','64 KiB','1 MiB','16 MiB','256 MiB']);ax.grid(alpha=.2);ax.legend()
for api,series,color,label in [('Metal','device_local/read_only','#8060ad','Metal fixed-grid read (best of 5)'),('Vulkan','read','#cb6650','Vulkan sequential read (median of 10)'),('Vulkan','write','#da9f40','Vulkan write'),('Vulkan','copy','#269674','Vulkan copy')]:
 p=[(b/1048576,y) for a,s,b,y,u in curves if a==api and s==series]
 if p:axes[0,2].plot(*zip(*p),label=label,color=color)
ax=axes[0,2];ax.set_title('GPU memory: distinct diagnostic methods');ax.set_xscale('log',base=2)
ax.set_xlabel('Working set (MiB)');ax.set_ylabel('GB/s');ax.grid(alpha=.2);ax.legend(fontsize=8)
for series,color,label in [('gpu_warm','#8060ad','GPU warmed'),('eviction_pass','#cb6650','After eviction controls'),('cpu_warm','#269674','CPU warmed')]:
 p=[(b/1048576,y) for a,s,b,y,u in curves if a=='GPU cache' and s==series]
 axes[1,0].plot(*zip(*p),color=color,label=label)
ax=axes[1,0];ax.set_xscale('log',base=2);ax.set_xlabel('Address span (MiB)');ax.set_ylabel('ns / dependent load')
ax.set_title('GPU cache-state diagnostic: attribution unverified');ax.grid(alpha=.2);ax.legend(fontsize=8)
axes[0,2].set_xticks([64/1024,256/1024,1,4,16,64,256],['64K','256K','1M','4M','16M','64M','256M'])
axes[1,0].set_xticks([4/1024,64/1024,1,4,16,64,128],['4K','64K','1M','4M','16M','64M','128M'])
ax=axes[1,1]
p=[(b/1048576,y) for a,s,b,y,u in curves if a=='GPU cache' and s=='parallel_gpu_warm']
ax.plot(*zip(*p),color='#8060ad');ax.set_title('GPU cache: 4096 chains × 512 loads')
ax.set_xscale('log',base=2);ax.set_xlabel('Address span (MiB)');ax.set_ylabel('GB/s of 4-byte payload')
ax.set_xticks([4/1024,64/1024,1,4,16,64,128],['4K','64K','1M','4M','16M','64M','128M']);ax.grid(alpha=.2)
ax=axes[1,2]
for series,color,label in [('l1','#cb6650','Full grid'),('l1_single','#2678c7','One workgroup')]:
 p=[(b/1024,y) for a,s,b,y,u in curves if a=='Vulkan' and s==series]
 if p: ax.plot(*zip(*p),color=color,label=label)
ax.set_title('Vulkan: workgroup-local read diagnostics');ax.set_xscale('log',base=2)
ax.set_xlabel('Private span per workgroup (KiB)');ax.set_ylabel('GB/s');ax.grid(alpha=.2);ax.legend(fontsize=8)
fig.text(.5,0,'K = KiB, M = MiB · Cache levels/SLC attribution unverified · Different memory methods do not establish API overhead',ha='center',fontsize=9)
fig.suptitle('iPad Pro 12.9-inch (3rd gen) · saved CPU / Metal / Vulkan / cache-state curves',fontsize=15)
fig.savefig(root/'memory-cache-curves.png',dpi=170);fig.savefig(root/'memory-cache-curves.pdf');plt.close(fig)

before=json.loads((root/'before-results.json').read_text())
old={r['id']:r for r in before}
unchanged=all(byid.get(k)==v for k,v in old.items())
verify={'status':suite['status'],'passedJobs':suite['status']=='passed' and sum(x['status']=='passed' for x in suite['jobs']),
 'jobCount':len(suite['jobs']),'selectedRecords':len(rows),'rawHistoryRecords':len(raw),'oldDeviceRecordsPreserved':unchanged,
 'oldDeviceRecordCount':len(old),'resultIDsUnique':len({r['id'] for r in rows})==len(rows),
 'apiCounts':{api:sum(r['graphicsApi']==api for r in rows) for api in {r['graphicsApi'] for r in rows}},
 'curvePointCounts':{a:sum(x[0]==a for x in curves) for a in {x[0] for x in curves}},
 'gpuCacheAttribution':gc.get('cacheAttribution'),'warming':gc.get('warming'),
 'cpuPageMappingMeasured':c.get('pageFaultMeasured')=='1','storageMeasured':c.get('storageMeasured')=='1',
 'gpuCacheVersion':cache['workloadVersion'],'gpuCacheParallelSteps':gc.get('parallelSteps'),
 'firstRunRecordCount':len(json.loads((root/'first-run/results/results.json').read_text())),
 'firstRunRecordsPreserved':all(byid.get(r['id'])==r for r in json.loads((root/'first-run/results/results.json').read_text())),
 'rawSha256':hashlib.sha256((phone/'results/results.json').read_bytes()).hexdigest(),
 'selectedSha256':hashlib.sha256((root/'suite-results.json').read_bytes()).hexdigest()}
(root/'verification.json').write_text(json.dumps(verify,indent=2,ensure_ascii=False)+'\n')

# Compare only the same matrix item/API/version/unit with the preceding iPhone suite.
previous_path=root.parent/'ios-api-completion-20261014'
previous_suite=json.loads((previous_path/'final-phone/suite-runs/latest.json').read_text())
previous_rows=json.loads((previous_path/'suite-results.json').read_text())
previous_by_id={r['id']:r for r in previous_rows}
previous_by_job={j['id']:[previous_by_id[i] for i in j['resultIDs']] for j in previous_suite['jobs']}
comparison=[]
for job in suite['jobs']:
 for ident in job['resultIDs']:
  current=byid[ident]
  candidates=[r for r in previous_by_job.get(job['id'],[]) if r['workload']==current['workload'] and r['graphicsApi']==current['graphicsApi'] and r['workloadVersion']==current['workloadVersion'] and r['scoreUnit']==current['scoreUnit']]
  if not candidates:continue
  oldrow=candidates[0]
  comparison.append({'job':job['id'],'title':job['title'],'workload':current['workload'],'api':current['graphicsApi'],'version':current['workloadVersion'], 'unit':current['scoreUnit'],'ipadScore':current['score'],'iphoneScore':oldrow['score'],'ratio':current['score']/oldrow['score'],'ipadID':current['id'],'iphoneID':oldrow['id']})
excluded_timing_ids=['20261014-020328-129', '20261014-020402-107', '20261014-020443-658']
for q in comparison:
 q['timingAudit']='see ../apple-score-audit-20261014/score-audit.json; version matching alone does not establish measurement validity'
 if q['ipadID'] in excluded_timing_ids:
  q['reportedRatio']=q['ratio']
  q['ratio']=None
  q['measurementValidity']='exclude_invalid_timing'
(root/'iphone-comparison.json').write_text(json.dumps(comparison,ensure_ascii=False,indent=2)+'\n')
counts=verify['curvePointCounts']
thermals=sorted({j['thermalStateEnd'] for j in suite['jobs']})
audit_notice='**2026-10-14 计时审计更正：任务通过仅表示流程完成与成绩保存，不保证测量有效。iPad Vulkan 1M／4M／16M 纯计算分数因计时异常排除；两机部分 Metal 绘制／流体计时暂定。详见[成绩比较与可信度审计](../apple-score-audit-20261014/comparison-audit.md)。以下原始分数保留用于追溯。**\n\n'
text=['# iPad Pro 12.9 英寸第三代 · 完整实机测试\n',
 f"**{sum(j['status']=='passed' for j in suite['jobs'])}/{len(suite['jobs'])} 项通过，{len(rows)} 条保存记录。**\n",
 f"真实硬件型号 iPad8,7，GPU Apple A12X GPU，系统 {rows[0]['osVersion']}，系统报告内存 {rows[0].get('systemMemoryMB',0)/1024:.2f} GiB；CPU 报告 {c.get('physicalCores')} 个物理核心和 {c.get('allCoreThreads')} 个全核线程。设备昵称不用于硬件归属。安装前没有 Mangekyo，成绩从新库开始，原始历史共 {len(raw)} 条。最终清单的成绩可跨已记录的续跑，不能视为一次不间断等温运行。\n",
 '本机支持清单为 31 项：Metal 13 项、Vulkan（MoltenVK → Metal）12 项、原生 GPU 缓存状态诊断 1 项、CPU 算力与缓存／内存／虚拟内存／存储 2 组，以及 OpenGL ES 2.0 旧版 GPU Burn 3 档。CPU 算力另存单线程和全核两条，合计 32 条成绩。\n',
 '运行时能力查询未提供 Vulkan 流体所需的 R32_SFLOAT 线性过滤，Vulkan 流体不进入队列；Metal 流体已运行。iOS 没有 ES 3.1 计算接口，对应 Android 粒子测试没有进入队列。ES 2.0 三档使用原有旧版着色器，不能与现代 GPU Burn 混排。\n',
 '本轮以新版一键测试流程启动，缓存 v2 在 48 MiB 并行阶段两次发生 GPU 恢复，失败日志保留；其余 30 项一次通过并原样沿用，只补测新版本的缓存诊断。Vulkan 转译显式开启。粒子、GPU Burn 和流体每项 15 秒（含预热），固定 1280 × 720，默认关闭垂直同步；ES 2.0 的呈现受显示刷新约束，使用 glFinish 同步计时。CPU 默认测试全核，并依次做由系统调度的单线程采样；每项测量 15 秒、预热 0.2 秒、分 3 轮。采样不是绑定到每个大核／小核。所有移动端成绩保留 ios_preview，转译结果保留 moltenvk 和实现标记。\n',
 f"各项开始／结束的系统温度状态均记录在清单；本轮结束状态出现 {thermals}（0=正常，1=较热，2=较高，3=严重）。连续负载或不同设备散热会影响分数，本报告不是严格等温 A/B 测试。\n",
 '| 项目 | API | 成绩 | 温度（开始→结束） |\n|---|---|---:|---|']
text.insert(1,audit_notice)
for job in suite['jobs']:
 for i in job['resultIDs']:
  r=byid[i];title=job['title']
  if r['workload']=='cpu_single_core':title+=' · 单线程采样平均'
  if r['workload']=='cpu_multi_core':title+=' · 全核'
  text.append(f"| {title} | {r['graphicsApi']} | {r['score']:.3f} {r['scoreUnit']} | {job['thermalStateStart']}→{job['thermalStateEnd']} |")
text+=['\n## CPU 缓存、内存、页面映射和存储\n',
 '| 模式 | 层级／区间 | 读取 GB/s | 写入 GB/s | 复制 GB/s |\n|---|---|---:|---:|---:|']
for mode in ['single','all']:
 for level in ['L1','L2','L3','memory']:
  prefix=mode+'_'+level+'_'
  if prefix+'read' not in c:continue
  text.append(f"| {'单线程' if mode=='single' else '全核'} | {level} | {c[prefix+'read']} | {c[prefix+'write']} | {c[prefix+'copy']} |")
text += [f"\n缓存容量来自系统报告，单线程由系统调度，不能将同一组成绩独立归给某个大核或小核。单线程最大数据集 {c.get('maxDatasetMiB')} MiB；全核曲线横轴表示每线程工作集。内存平台标志为单线程 `{c.get('single_memory_settled')}`、全核 `{c.get('all_memory_settled')}`。\n",
 f"页面映射首次触页 **{c.get('pageFaultGBps')} GB/s**，每 {int(c.get('pageBytes','16384'))//1024} KiB 页面 **{c.get('pageFaultNsPerPage')} ns**；这不是磁盘换页测速。1 GiB 临时文件顺序读／写 **{c.get('storageSeqReadMBps')}／{c.get('storageSeqWriteMBps')} MB/s**，4 KiB 随机 QD1 读／写 **{c.get('storageRandReadMBps')}／{c.get('storageRandWriteMBps')} MB/s**。\n",
 '## 保存的 GPU／CPU 曲线\n', f"已保存并用生产解析器恢复：CPU {counts.get('CPU',0)} 点、Metal 内存 {counts.get('Metal',0)} 点、Vulkan 内存 {counts.get('Vulkan',0)} 点、GPU 缓存状态 {counts.get('GPU cache',0)} 点。缓存 v3 延续每段 8192 次读取的连续完整周期预热，每段校验索引与校验和。A12X 的并行测量改为每条链 512 次读取（4096 条链），与 iPhone 的 v2／8192 次配置单独分组。三种串行延迟测量仍为每条链 65536 次读取。\n",
 '**GPU L1/L2/L3、SoC/SLC 仍未独立归属。**依赖读取延迟单位 ns/load，4096 条并行链的四字节有效负载吞吐单位 GB/s；它们不能混用，也不等同于顺序 DRAM 带宽。Metal 固定网格与 Vulkan 顺序读写是不同诊断方法，不能单靠两个分数推导转译开销。\n',
 f"![内存与缓存曲线](<{root/'memory-cache-curves.png'}>)\n", '## 与上一台 iPhone 13 Pro 对照\n',
 '仅按相同测试项、API、版本和单位对应；缓存 v3 与 iPhone v2 的配置不同，未做比值比较。Vulkan 65K 渲染的计时方式版本也不同，未做比值比较。CPU 全核分别使用本机 8 线程和 iPhone 的 6 线程。比值为 iPad/iPhone，吞吐类越大越高，ns/load 延迟类越小越低。不跨 API 或不同诊断方法合并总分。\n',
 '| 项目 | API | iPad | iPhone 13 Pro | 比值 |\n|---|---|---:|---:|---:|']
for q in comparison:
 title=q['title']
 if q['workload']=='cpu_single_core':title+=' · 单线程'
 if q['workload']=='cpu_multi_core':title+=' · 全核'
 ratio_label='排除：计时异常' if q['ipadID'] in excluded_timing_ids else f"{q['ratio']:.2f}×"
 text.append(f"| {title} | {q['api']} | {q['ipadScore']:.3f} {q['unit']} | {q['iphoneScore']:.3f} {q['unit']} | {ratio_label} |")
text += ['\n## 文件和核验\n',
 '- `suite-results.json`：本套选取的 32 条成绩。\n- `final-phone/results/results.json`：iPad 原始完整历史。\n- `final-phone/suite-runs/latest.json` 与对应日志：清单、成绩 ID、时间、温度及原生输出。\n- `first-run`、`attempt-1`、`retry-1`：保留原始运行及失败日志。\n- `verification.txt/.json`：生产 Swift 模型核对工作量、API、合约及全部曲线恢复。\n- `curve-points.csv` 和 `memory-cache-curves.png/.pdf`：保存的原始曲线点与图。\n- `iphone-comparison.json`：与上一台 iPhone 相同项目的对应成绩及 ID。\n',
 '成绩保留在 iPad 的“文件 → 我的 iPad → Mangekyo → results/results.json”，也已取回 Mac 单独归档。原 iPhone 与 Mac 的成绩不导入、不重写。\n']
(root/'report.md').write_text('\n'.join(text))
print(json.dumps(verify,ensure_ascii=False,indent=2))
