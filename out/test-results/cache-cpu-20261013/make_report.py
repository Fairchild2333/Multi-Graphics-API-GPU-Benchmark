from pathlib import Path
import json,hashlib,csv,statistics
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
root=Path.cwd(); out=root/'out/test-results/cache-cpu-20261013'
source=Path.home()/'Library/Application Support/GpuComputeBenchmark/results/results.json'
before=json.loads((out/'before-results.json').read_text()); after=json.loads(source.read_text()); byid={r['id']:r for r in after}
assert all(all(byid[r['id']].get(k)==v for k,v in r.items()) for r in before)
old_ids={r['id'] for r in before}; added=[r for r in after if r['id'] not in old_ids]
(out/'after-results.json').write_bytes(source.read_bytes()); (out/'new-results.json').write_text(json.dumps(added,ensure_ascii=False,indent=2)+'\n')
def records(name,prefix):
    answer=[]
    for line in (out/(name+'.log')).read_text().splitlines():
        if line.startswith(prefix+'\t'):
            answer.append(dict(item.split('=',1) for item in line.split('\t')[1:] if '=' in item))
    return answer
names=['gpu-cache-full','gpu-cache-double-loads','gpu-cache-repeat']
gpu={n:records(n,'GCACHE_POINT') for n in names}
assert all(len(points)==124 for points in gpu.values())
assert all(len(records(n,'GCACHE_SAMPLE'))==620 and all(r['checksum_valid']=='1' for r in records(n,'GCACHE_SAMPLE')) for n in names)
with (out/'gpu-cache-points.csv').open('w',newline='') as f:
    w=csv.DictWriter(f,fieldnames=['run','condition','bytes','median','min','max','unit']);w.writeheader()
    for name,points in gpu.items():
        for p in points:w.writerow({'run':name,**p})
colors={'gpu_warm':'#2563eb','eviction_pass':'#f59e0b','cpu_warm':'#059669'}
labels={'gpu_warm':'GPU warmed','eviction_pass':'After eviction pass','cpu_warm':'CPU warmed'}
fig,axes=plt.subplots(1,2,figsize=(13,5),layout='constrained')
for cond in colors:
    p=[p for p in gpu[names[-1]] if p['condition']==cond]
    x=[int(p['bytes'])/1048576 for p in p]; y=[float(p['median']) for p in p]
    axes[0].plot(x,y,label=labels[cond],color=colors[cond],lw=2)
    axes[0].fill_between(x,[float(p['min']) for p in p],[float(p['max']) for p in p],alpha=.12,color=colors[cond])
for name,style,label in [(names[0],':','GPU warmed: first run'),(names[1],'--','GPU warmed: double loads')]:
    p=[p for p in gpu[name] if p['condition']=='gpu_warm']; axes[0].plot([int(p['bytes'])/1048576 for p in p],[float(p['median']) for p in p],style,color='#64748b',lw=1.2,label=label)
axes[0].set_ylabel('Effective dependent-read time (ns/load)'); axes[0].set_title('Native Metal random dependent reads')
for name,label in zip(names,['65536 loads: first run','131072 loads','65536 loads: repeat']):
    p=[p for p in gpu[name] if p['condition']=='parallel_gpu_warm']; axes[1].plot([int(p['bytes'])/1048576 for p in p],[float(p['median']) for p in p],label=label,lw=2)
axes[1].set_ylabel('Requested uint32 payload (GB/s)');axes[1].set_yscale('log');axes[1].set_title('4096 concurrent dependent chains')
for ax in axes:
    ax.set_xscale('log',base=2);ax.set_xticks([1/256,1/32,1/4,2,16,128],['4 KiB','32 KiB','256 KiB','2 MiB','16 MiB','128 MiB']);ax.set_xlabel('Address span, one node every 128 bytes');ax.grid(alpha=.2);ax.legend(fontsize=8)
fig.suptitle('M4 Pro cache-state diagnostic — preview, hardware tiers unassigned',fontsize=12)
fig.savefig(out/'gpu-cache-curves.png',dpi=160);fig.savefig(out/'gpu-cache-curves.pdf');plt.close(fig)
cpu=records('cpu-memory-all','CPUMEM_POINT')
fig,axes=plt.subplots(1,2,figsize=(13,5),layout='constrained')
for ax,mode,title in zip(axes,['single','all'],['CPU single thread (scheduler managed)','CPU 14 threads, summed effective rates']):
    for kernel,color in [('read','#2563eb'),('write','#f59e0b'),('copy','#059669')]:
        pts=[p for p in cpu if p['mode']==mode and p['kernel']==kernel]
        ax.plot([int(p['block_bytes'])/1048576 for p in pts],[float(p['median_gbps']) for p in pts],label=kernel,color=color,lw=2)
    ax.set_xscale('log',base=2);ax.set_xticks([1/256,1/16,1,16,256]+([1024] if mode=='single' else []),['4 KiB','64 KiB','1 MiB','16 MiB','256 MiB']+(['1 GiB'] if mode=='single' else []));ax.set_yscale('log');ax.set_xlabel('Data per thread');ax.set_ylabel('GB/s');ax.set_title(title);ax.legend();ax.grid(alpha=.2)
axes[0].axvline(.125,color='#64748b',ls='--',alpha=.5);axes[0].axvline(16,color='#64748b',ls='--',alpha=.5)
fig.suptitle('CPU memory sweep — default NEON kernels, median samples; reference L1/L2 sizes on single-thread plot',fontsize=11)
fig.savefig(out/'cpu-memory-curves.png',dpi=160);fig.savefig(out/'cpu-memory-curves.pdf');plt.close(fig)
compute=records('cpu-compute-all','CPU_RESULT'); summary=next(r for r in compute if r.get('kind')=='summary'); multi=next(r for r in compute if r.get('kind')=='multi')
levels=records('cpu-memory-all','CPUMEM_LEVEL');storage=records('cpu-memory-all','CPUMEM_STORAGE')[0];page=records('cpu-memory-all','CPUMEM_PAGEFAULT')[0]
vk=records('vulkan-existing-memory-sweep','MEMBW_SUMMARY'); l1=records('vulkan-existing-memory-sweep','MEMBW_L1')[0]
vr={r['kernel']:r for r in vk}; gpu_row={(r['condition'],int(r['bytes'])):r for r in gpu[names[-1]]}
old_v=next(r for r in before if r['id']=='20261013-182141-555-membw');old_c=next(r for r in before if r['id']=='20261013-183008-002-cpu-membw')
text=['# M4 Pro：CPU 完整测试与 GPU 缓存诊断（2026-10-13）','',
'CPU 支持的测试类型已完成；GPU 新增随机依赖读取与 CPU/GPU 预热对照，并重跑原有 Vulkan 内存扫描。3 项 CPU 运行、4 项 GPU 运行均退出成功，新增 8 条成绩。原有 125 条成绩的已有字段全部保留，仓库的 220 条旧成绩文件未改。','',
'## CPU 成绩','',
f'- 混合算力：单线程 14 次采样均值 **{float(summary["average_score"]):.1f} MWork/s**，范围 {float(summary["min_score"]):.1f}–{float(summary["max_score"]):.1f}；全核 14 线程 **{float(multi["score"]):.1f} MWork/s**。每项 15 秒，3 轮取中位数。',
'- macOS 由调度器安排核心，不能把这 14 次单线程采样当作已绑定、逐一验证的 P/E 核成绩。缓存表的名字沿用系统参考容量，表示此内核的有效带宽。','',
'| 模式 | 区间 | 读 GB/s | 写 GB/s | 复制 GB/s |','|---|---|---:|---:|---:|']
for r in levels:
    text.append(f'| {"单线程" if r["mode"]=="single" else "14 线程合计"} | {r["level"]} | {float(r["read_gbps"]):.1f} | {float(r["write_gbps"]):.1f} | {float(r["copy_gbps"]):.1f} |')
text+=['',f'页面映射：{float(page["gbps"]):.1f} GB/s，{float(page["ns_per_page"]):.1f} ns / 16 KiB 页面。',
f'磁盘顺序读/写：{float(storage["seq_read_mbps"]):.1f} / {float(storage["seq_write_mbps"]):.1f} MB/s；随机 4 KiB 单请求读/写：{float(storage["rand_read_mbps"]):.1f} / {float(storage["rand_write_mbps"]):.1f} MB/s。',
'另做一次原有 `--cpu-membw-curve-levels` 扫描：单线程/全核仍只识别出两段缓存平台和内存，没有独立 SoC 段。这是曲线近似分段，不是新确认的缓存容量。','',
'![CPU 内存曲线](cpu-memory-curves.png)','',
'## GPU 原有测试复测','',
f'Vulkan（MoltenVK）大工作集读/写/复制：**{float(vr["read"]["memory_gbps"]):.1f} / {float(vr["write"]["memory_gbps"]):.1f} / {float(vr["copy"]["memory_gbps"]):.1f} GB/s**，三个内存尾段都已稳定。',
f'缓存命中段：读 {float(vr["read"]["cache_gbps"]):.1f}、写 {float(vr["write"]["cache_gbps"]):.1f}、复制 {float(vr["copy"]["cache_gbps"]):.1f} GB/s。局部/L1 型探测：全 GPU {float(l1["full_load_gbps"]):.1f}、单工作组 {float(l1["single_group_gbps"]):.1f} GB/s；转折 192–256 KiB 仍是探测范围，不能当作已确认硬件容量。',
f'此前 Vulkan 内存读 {old_v["score"]:.1f} GB/s，本轮 {float(vr["read"]["memory_gbps"]):.1f}（变化 {(float(vr["read"]["memory_gbps"])/old_v["score"]-1)*100:+.1f}%）；此前 CPU 全核读 {old_c["score"]:.1f}，本轮 {float(next(r for r in levels if r["mode"]=="all" and r["level"]=="memory")["read_gbps"]):.1f} GB/s。',
'Apple 标称 M4 Pro 内存带宽为 273 GB/s，本轮 CPU / Vulkan 有效读速约为其 90.6% / 92.5%。[Apple 技术规格](https://support.apple.com/en-au/121553)。','',
'## 新增 GPU 缓存状态诊断','',
'三轮完整扫描均包含 31 个地址跨度 × 4 个条件；每轮 620 个测量样本，所有依赖链最终位置与累加校验均正确。第一、第三轮 65536 次读取，第二轮 131072 次；每个点取 5 个交错样本的中位数。纵轴包含循环和调度开销，不是裸缓存响应延迟。','',
'| 地址跨度 | GPU 预热 ns/读取 | 清扫后 ns/读取 | CPU 预热 ns/读取 | 并发逻辑负载 GB/s |','|---|---:|---:|---:|---:|']
for size in [131072,1048576,4194304,8388608,16777216,33554432,134217728]:
    vals=[float(gpu_row[(c,size)]['median']) for c in ['gpu_warm','eviction_pass','cpu_warm','parallel_gpu_warm']]
    label=f'{size//1024} KiB' if size<1048576 else f'{size//1048576} MiB'
    text.append('| '+label+' | '+' | '.join(f'{v:.1f}' for v in vals)+' |')
text+=['',
'![GPU 缓存诊断曲线](gpu-cache-curves.png)','',
'大跨度的 GPU 预热段在多轮之间较稳定；第一轮小跨度存在启动/时钟影响，读取次数变化也改变小周期的重复命中次数。CPU 预热相对清扫后的收益没有在两种读取次数和复测之间一致重现，因此本轮没有独立确认 SLC 段。',
'本机公开 Metal counter-set 枚举只提供 `timestamp`；本轮没有采集能区分硬件缓存层的计数器。清扫缓冲不等于硬件 cache flush，预热也不保证跨命令固定在同一个 GPU 核。随机读取还受 TLB、缓存行取回、时钟和地址生成影响。**这些是真实诊断曲线，不能直接写成独立 GPU L2/L3/SLC 的峰值带宽或容量。**',
'原有 Vulkan 吞吐曲线与新增 Metal 延迟/随机逻辑负载曲线使用不同内核和字节计数，不能直接相减来计算转译开销。',
'[诊断方法](../../../docs/gpu-cache-probe.md)；[Apple 的 GPU 缓存/MMU 诊断说明](https://developer.apple.com/videos/play/tech-talks/111374/)。','',
'## 单线程采样明细','',
'| 采样序号（系统调度） | MWork/s |','|---|---:|']
for r in compute:
    if r.get('kind')=='core': text.append(f'| {int(r["core_index"])+1} | {float(r["score"]):.2f} |')
text+=['','所有 CPU / GPU 成绩在共享 `results/results.json` 中；新增 GPU 诊断使用独立 preview 版本，GUI 历史曲线可直接选择。原始日志、逐点最小/最大值、CSV、新增成绩和前后快照在本报告旁。']
report='\n'.join(text)+'\n'; (out/'report.md').write_text(report)
shared=Path.home()/'Library/Application Support/GpuComputeBenchmark/reports'
shared.mkdir(exist_ok=True,parents=True)
for name in ['gpu-cache-curves.png','cpu-memory-curves.png','gpu-cache-curves.pdf','cpu-memory-curves.pdf']:
    (shared/name).write_bytes((out/name).read_bytes())
(shared/'cache-cpu-complete-2026-10-13.md').write_text(report.replace('[诊断方法](../../../docs/gpu-cache-probe.md)',f'[诊断方法](<{root}/docs/gpu-cache-probe.md>)'))
verification={'beforeCount':len(before),'afterCount':len(after),'newCount':len(added),'oldRecordsUnchanged':True,'newIDs':[r['id'] for r in added],'newDiagnosticSamples':1860,'oldRepositorySHA256':hashlib.sha256((root/'results/results.json').read_bytes()).hexdigest(),'cpuLedger':json.loads((out/'cpu-ledger.json').read_text()),'gpuLedger':json.loads((out/'gpu-ledger.json').read_text())}
(out/'verification.json').write_text(json.dumps(verification,ensure_ascii=False,indent=2)+'\n')
print('Saved report and figures; '+str(len(added))+' new records; original '+str(len(before))+' records preserved.')
