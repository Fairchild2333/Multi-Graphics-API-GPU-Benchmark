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
 ax.set_title('CPU: '+('one scheduler-managed thread' if mode=='single' else 'all 6 cores'))
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
ax.plot(*zip(*p),color='#8060ad');ax.set_title('GPU cache: 4096 concurrent dependent chains')
ax.set_xscale('log',base=2);ax.set_xlabel('Address span (MiB)');ax.set_ylabel('GB/s of 4-byte payload')
ax.set_xticks([4/1024,64/1024,1,4,16,64,128],['4K','64K','1M','4M','16M','64M','128M']);ax.grid(alpha=.2)
ax=axes[1,2]
for series,color,label in [('l1','#cb6650','Full grid'),('l1_single','#2678c7','One workgroup')]:
 p=[(b/1024,y) for a,s,b,y,u in curves if a=='Vulkan' and s==series]
 if p: ax.plot(*zip(*p),color=color,label=label)
ax.set_title('Vulkan: workgroup-local read diagnostics');ax.set_xscale('log',base=2)
ax.set_xlabel('Private span per workgroup (KiB)');ax.set_ylabel('GB/s');ax.grid(alpha=.2);ax.legend(fontsize=8)
fig.text(.5,0,'K = KiB, M = MiB · Cache levels/SLC attribution unverified · Different memory methods do not establish API overhead',ha='center',fontsize=9)
fig.suptitle('iPhone 13 Pro · saved CPU / Metal / Vulkan / cache-state curves',fontsize=15)
fig.savefig(root/'memory-cache-curves.png',dpi=170);fig.savefig(root/'memory-cache-curves.pdf');plt.close(fig)

before=json.loads((root/'before-phone/results/results.json').read_text())
old={r['id']:r for r in before}
unchanged=all(byid.get(k)==v for k,v in old.items())
verify={'status':suite['status'],'passedJobs':suite['status']=='passed' and sum(x['status']=='passed' for x in suite['jobs']),
 'jobCount':len(suite['jobs']),'selectedRecords':len(rows),'rawHistoryRecords':len(raw),'oldPhoneRecordsPreserved':unchanged,
 'oldPhoneRecordCount':len(old),'resultIDsUnique':len({r['id'] for r in rows})==len(rows),
 'apiCounts':{api:sum(r['graphicsApi']==api for r in rows) for api in {r['graphicsApi'] for r in rows}},
 'curvePointCounts':{a:sum(x[0]==a for x in curves) for a in {x[0] for x in curves}},
 'gpuCacheAttribution':gc.get('cacheAttribution'),'warming':gc.get('warming'),
 'cpuPageMappingMeasured':c.get('pageFaultMeasured')=='1','storageMeasured':c.get('storageMeasured')=='1',
 'rawSha256':hashlib.sha256((phone/'results/results.json').read_bytes()).hexdigest(),
 'selectedSha256':hashlib.sha256((root/'suite-results.json').read_bytes()).hexdigest()}
(root/'verification.json').write_text(json.dumps(verify,indent=2,ensure_ascii=False)+'\n')

audit_notice='**2026-10-14 计时审计更正：任务通过仅表示流程完成与成绩保存，不保证测量有效。iPad Vulkan 1M／4M／16M 纯计算分数因计时异常排除；两机部分 Metal 绘制／流体计时暂定。详见[成绩比较与可信度审计](../apple-score-audit-20261014/comparison-audit.md)。以下原始分数保留用于追溯。**\n\n'
text=['# iPhone 13 Pro · API 补齐与完整实机测试\n',f"**{sum(j['status']=='passed' for j in suite['jobs'])}/{len(suite['jobs'])} 项通过，{len(rows)} 条保存记录。**原有手机历史 {len(old)} 条均保留（逐字段核对：{unchanged}）。\n",
 '本轮范围：13 项 Metal、12 项 Vulkan（MoltenVK → Metal）、1 项 GPU 缓存状态诊断、CPU 算力和 CPU 缓存／内存／页面映射／存储，以及 OpenGL ES 2.0 旧版 GPU Burn 三档。CPU 算力分别保存单线程和全核成绩，因此 31 个任务产生 32 条记录。\n',
 'iPhone 13 Pro 的 Vulkan 接口未提供流体所需的 R32_SFLOAT 线性过滤能力，流体从 Vulkan 清单排除；Metal 流体已实际测量。iOS 没有 ES 3.1 计算接口，所以原 Android 的 ES 3.1 粒子清单不在本机运行。OpenGL ES 2.0 是独立旧版着色器，不能与现代 GPU Burn 混排。\n',
 '新版默认关闭转译 API。开启“允许转译 API（MoltenVK）”后，图形 API 可选 Vulkan，一键测试也会包含设备支持的 Vulkan 项目。这个开关控制执行权限；调用原生入口而未显式允许转译也会被拒绝。固定 1280 × 720，粒子／GPU Burn／流体每项 15 秒。ES 2.0 使用 glFinish 同步计时，其显示提交受屏幕刷新约束。所有移动端成绩单独保留 ios_preview 合约，Vulkan 另带 moltenvk 与实现标记。\n',
 '首次 Vulkan 能力查询误请求了仅由 Vulkan loader 提供的 portability 枚举扩展，已按运行库实际提供的扩展修复。12 项 Vulkan 均已在真机跑完。第一次全尺寸缓存诊断通过，但第二轮的 128 MiB 单条预热命令触发系统 GPU watchdog；失败记录未当作成绩。iOS 已改成 8192 次读取一段的连续完整周期预热，每段验证索引和校验和。测量循环保持原样，版本更新为 metal_dependent_cache_v2_preview_ios_preview，避免与旧控制条件混排。修复后补跑通过，原始失败日志保留在 vulkan-attempt。\n',
 '这份完整清单由可核验的续跑记录组成，保留此前通过的 Metal／Vulkan 测量，再补齐新诊断和剩余任务；各项的原始时间、温度状态和成绩 ID 全部可追溯。连续负载的温度可能影响分数，不能作为冷机峰值或严格等温 A/B 对比。\n',
 '| 项目 | API | 成绩 | 温度状态（开始→结束） |\n|---|---|---:|---|']
text.insert(1,audit_notice)
for job in suite['jobs']:
 for i in job['resultIDs']:
  r=byid[i];title=job['title']
  if r['workload']=='cpu_single_core': title+=' · 单线程采样平均'
  if r['workload']=='cpu_multi_core': title+=' · 全核'
  text.append(f"| {title} | {r['graphicsApi']} | {r['score']:.3f} {r['scoreUnit']} | {job['thermalStateStart']}→{job['thermalStateEnd']} |")
text+=['\n## 缓存、内存、虚拟内存和存储\n',f"CPU 单线程内存读取 **{c.get('single_memory_read')} GB/s**，全核读取 **{c.get('all_memory_read')} GB/s**。页面映射首次触页 **{c.get('pageFaultGBps')} GB/s**，每 16 KiB 页面 **{c.get('pageFaultNsPerPage')} ns**。这不是磁盘换页测速。1 GiB 临时文件顺序读／写 **{c.get('storageSeqReadMBps')}／{c.get('storageSeqWriteMBps')} MB/s**；4 KiB 随机 QD1 读／写 **{c.get('storageRandReadMBps')}／{c.get('storageRandWriteMBps')} MB/s**。\n",
 'CPU 的单线程由系统调度，不能标成绑定到每个大核或小核。缓存诊断保存 GPU 预热、干扰读后、CPU 预热三个依赖读取延迟系列，以及一个并行四字节有效负载吞吐系列。ns/load 与 GB/s 分开保存，不能彼此混用。**目前仍不能把这些曲线独立归属到 GPU L1/L2/L3、SoC/SLC，也不能把 Metal 固定网格诊断直接当成已验证的 DRAM 峰值。**\n',
 f"![保存的内存与缓存曲线](<{root/'memory-cache-curves.png'}>)\n",
 '## 文件与验证\n',
 '- `suite-results.json`：本套选取的 32 条保存记录。\n- `final-phone/results/results.json`：完整手机原始历史。\n- `final-phone/suite-runs/latest.json` 与逐轮日志：完整清单、续跑来源、温度和全部原生输出。\n- `before-phone`：此次改动前的手机数据；`native-attempt`、`vulkan-attempt`：中间运行与失败日志。\n- `curve-points.csv`、`memory-cache-curves.png/.pdf`：保存曲线的原始点与图。\n- `verification.txt/.json`：用生产 Swift 模型与曲线解析器核对任务、API、合约、成绩及曲线恢复。\n- iOS 18.5 模拟器桥接回归：11 项中 10 项通过、1 项因虚拟 GPU 无统一内存缓存控制而跳过；缓存诊断已改在真机验证。桌面兼容／任务／序列化测试 5/5 通过。\n',
 '手机数据可从“文件 → 我的 iPhone → Mangekyo”浏览，或从 Mac Finder 的设备文件共享读取。GPU 和 CPU 均保存到 Documents/results/results.json；诊断日志在 Documents/suite-runs。\n']
(root/'report.md').write_text('\n'.join(text))
print(json.dumps(verify,ensure_ascii=False,indent=2))
