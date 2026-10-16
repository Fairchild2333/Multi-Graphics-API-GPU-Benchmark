from pathlib import Path
import csv, hashlib, json, re, sys
ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT.parent / 'a15-targeted-retest-20261014/plot-deps'))
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

before = json.loads((ROOT/'before/results/results.json').read_text())
final_path = ROOT/'policy-final/results/results.json'
after = json.loads(final_path.read_text())
byid = {r['id']: r for r in after}
assert len(byid) == len(after)
assert all(byid[r['id']] == r for r in before)
pattern = re.compile(r'\[GPU Timing\] Compute: ([0-9.]+) ms \| Render: ([0-9.]+) ms \| Total: ([0-9.]+) ms \| FPS: (\d+) \| T: ([0-9.]+) s(.*)')
audit=[]; points=[]; selected={}; added=set(); rejected=[]
folders = sorted([p.parent.parent for p in ROOT.glob('*/documents/suite-runs/latest.json')],
                 key=lambda p:json.loads((p/'suite-runs/latest.json').read_text())['startedAt'])
for docs in folders:
    m=json.loads((docs/'suite-runs/latest.json').read_text()); job=m['jobs'][0]
    run=docs.parent.name; log=(docs/'suite-runs'/(m['id']+'.log')).read_text()
    records=[byid[i] for i in job['resultIDs']];added.update(job['resultIDs'])
    for line in log.splitlines():
        if line.startswith('CPU_RESULT\t') and '\tkind=core\t' in line and '\tvalid=0\t' in line:
            rejected.append({'run':run,'logLine':line})
    for r in records:
        cfg=dict(s.split('=',1) for s in r['workloadConfig'].split(';') if '=' in s)
        assert r['deviceModel']=='iPhone14,2' and r['platform']=='iOS' and r['score']>0
        if r['workload']=='cpu_single_core' and cfg.get('coreType'):
            assert cfg['residencyVerified']=='1' and cfg['residencyMethod']=='mach_task_power_thread_clock_bound'
            assert min(float(cfg[f'round{i}RequestedLowerBound']) for i in [1,2,3])>=.99
        if r['graphicsApi']!='CPU':
            assert (r['resWidth'],r['resHeight'])==(1280,720)
            assert r['avgComputeMs']+r['avgRenderMs']<=r['avgTotalGpuMs']+.1
            assert r['avgTotalGpuMs']<=r['avgFrameTimeMs']*1.1
            assert r['warmupSec']==2 and r['durationSec']>=12
            if job['id'].startswith('vulkan/gpu_burn'):
                assert cfg['timingMode']=='synchronized_wall_clock' and r['framesInFlight']==1
            for line in log.splitlines():
                match=pattern.search(line)
                if not match:continue
                compute,render,total,fps,seconds=map(float,match.groups()[:5])
                value=40*r['particleCount']/compute/1e6 if r['workload']=='stream' else 1280*720*int(cfg['steps'])*2/render/1e6
                points.append({'run':run,'job':job['id'],'resultID':r['id'],'seconds':seconds,
                    'warmup':'warm-up' in match.group(6),'computeMs':compute,'renderMs':render,
                    'totalGpuMs':total,'reportedFps':fps,'derivedScore':value,'scoreUnit':r['scoreUnit']})
    entry={'run':run,'job':job['id'],'manifest':m,'resultIDs':job['resultIDs'],
           'results':records,'log':str((docs/'suite-runs'/(m['id']+'.log')).relative_to(ROOT))}
    audit.append(entry)
    if run.startswith(('final-', 'repaired-', 'policy-')) and job['status'] in ['passed','review']:selected[job['id']]=entry

assert set(byid)-{r['id'] for r in before}==added
assert len(after)==len(before)+len(added)
assert len(selected)==13
assert all(e['manifest']['jobs'][0]['status']=='passed' for e in selected.values()), 'Final quality flags remain'
cpu=selected['cpu_compute']['results']
assert len(cpu)==3 and {r['workloadConfig'].split('coreType=')[-1].split(';')[0] for r in cpu if r['workload']=='cpu_single_core'}=={'performance','efficiency'}
memory=selected['cpu_memory']['results'][0]
memcfg=dict(s.split('=',1) for s in memory['workloadConfig'].split(';') if '=' in s)
assert memcfg['residencyVerified']=='1' and memcfg['coreType']=='both'

summary={'historyBefore':len(before),'historyAfter':len(after),'newRecords':len(added),
    'oldRecordsUnchanged':True,'finalJobsPassed':len(selected),'finalCPURecords':len(cpu),
    'cpuResidencyThreshold':.99,'rejectedCoreObservations':rejected,
    'memoryCoreTypesVerified':True,'finalTimingChecksPassed':True,
    'afterSha256':hashlib.sha256(final_path.read_bytes()).hexdigest()}
(ROOT/'verification.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2)+'\n')
(ROOT/'audit.json').write_text(json.dumps(audit,ensure_ascii=False,indent=2)+'\n')
(ROOT/'selected-results.json').write_text(json.dumps({k:v['results'] for k,v in selected.items()},ensure_ascii=False,indent=2)+'\n')
previous=json.loads((ROOT.parent/'a15-targeted-retest-20261014/curated-suite-results.json').read_text())
def job_key(r):
    cfg=dict(s.split('=',1) for s in r['workloadConfig'].split(';') if '=' in s)
    if r['workload'] in ['cpu_single_core','cpu_multi_core']:return 'cpu_compute'
    if r['workload']=='cpu_membw':return 'cpu_memory'
    prefix='vulkan/' if r['graphicsApi']=='Vulkan' else 'gles2/' if r['graphicsApi']=='OpenGL ES 2.0' else ''
    if r['workload']=='stream':return prefix+('stream_headless-' if r['headless'] else 'stream-')+str(r['particleCount'])
    if r['workload']=='gpu_burn':return prefix+'gpu_burn-'+cfg['steps']
    return prefix+r['workload']
curated=[];replaced=set()
for r in previous:
    key=job_key(r)
    if key in selected:
        if key not in replaced:curated.extend(selected[key]['results']);replaced.add(key)
    else:curated.append(r)
assert len(curated)==33 and replaced==set(selected)
(ROOT/'curated-suite-results.json').write_text(json.dumps(curated,ensure_ascii=False,indent=2)+'\n')
with (ROOT/'curves.csv').open('w') as f:
    w=csv.DictWriter(f,fieldnames=list(points[0]));w.writeheader();w.writerows(points)

fig,axes=plt.subplots(3,2,figsize=(13,8),layout='constrained')
mainjobs=['stream-1048576','vulkan/stream-1048576','vulkan/gpu_burn-256']
labels=['Metal particles 1M','Vulkan particles 1M (MoltenVK)','Vulkan GPU Burn 256 (MoltenVK)']
for row,key in enumerate(mainjobs):
    left,right=axes[row]
    for entry in audit:
        run=entry['run']
        if entry['job']!=key or not run.startswith(('quiet-','legacy-control-','final-')):continue
        ps=[p for p in points if p['run']==run]; xs=[p['seconds'] for p in ps]
        style=':' if run.startswith('legacy') else '-' if run.startswith('quiet') else '--'
        tag='legacy control' if run.startswith('legacy') else 'fixed first' if run.startswith('quiet') else 'fixed confirm'
        if 'gpu_burn' in key:tag='sync first' if run.startswith('quiet') else 'sync confirm'
        for metric,label,color in [('computeMs','Compute','#236fbb'),('renderMs','Render','#dd853a'),('totalGpuMs','Total','#334755')]:
            left.plot(xs,[p[metric] for p in ps],linestyle=style,color=color,linewidth=1.25,label=f'{label}: {tag}')
        right.plot(xs,[p['derivedScore'] for p in ps],linestyle=style,marker='.',label=tag)
    r=selected[key]['results'][0]
    left.set_title(labels[row]);left.set_ylabel('Time (ms)');left.legend(fontsize=6.5,ncol=2)
    right.set_title(f"Final mean {r['score']:.3f}, stable {r['stableScore']:.3f}, drop {r['throttlePct']:.2f}%")
    right.set_ylabel(r['scoreUnit']);right.legend(fontsize=8)
    for ax in [left,right]:
        ax.axvspan(0,2,color='#d4dce2',alpha=.45);ax.grid(alpha=.2);ax.set_xlabel('Elapsed seconds');ax.spines[['top','right']].set_visible(False)
fig.suptitle('A15: fixed coverage controls and per-frame synchronized Burn timing',fontsize=14)
fig.supxlabel('1280x720. Grey: warm-up. Curves derived from rounded log windows. Legacy and new contracts are preserved separately.',fontsize=9)
fig.savefig(ROOT/'gpu-fix-curves.png',dpi=170);fig.savefig(ROOT/'gpu-fix-curves.pdf')

rows=['# A15 大小核与 3 项 GPU 问题修复验证（2026-10-14）','',
    f"大小核分开测量成功，3 项 GPU 问题修正后均通过两轮验证。最终验证的 13 项全部通过；本轮手机历史从 {len(before)} 增至 {len(after)} 条，所有旧对象逐项相等。",'',
    '## CPU 实测','', '| 测试 | 成绩 | 编号 |','| --- | ---: | --- |']
for r in cpu:
    cfg=dict(s.split('=',1) for s in r['workloadConfig'].split(';') if '=' in s)
    label={'performance':'大核单线程','efficiency':'小核单线程（后台 QoS）'}.get(cfg.get('coreType'),'全核 6 线程')
    rows.append(f"| {label} | {r['score']:.3f} MWork/s | `{r['id']}` |")
rows += ['', '系统报告 2 个大核、4 个小核；没有按编号绑核。使用 QoS 请求类型，再以公开 task_info 的大核 CPU 时间、进程总 CPU 时间与线程 CPU 时钟计算保守驻留下界，每轮必须达到 99%。小核进度暂存至测量结束后发送，未通过验证的观察不保存成正式小核成绩。后台小核成绩不代表最高频率上限；iOS 没有提供本方法所需的周期计数。', '',
    '两轮成功与此前两轮拒绝的所有记录见 audit.json。CPU 缓存／内存扫描也验证了分核默认路径，保留大小核曲线和驻留下界。', '',
    '## GPU 修正与对照', '', '| 项目 | 首轮修正均值 | 最终均值 | 最终稳定段 | 最终下降 |', '| --- | ---: | ---: | ---: | ---: |']
for key,label in zip(mainjobs,labels):
    first=next(e for e in audit if e['job']==key and e['run'].startswith('quiet-'))['results'][0]
    last=selected[key]['results'][0]
    rows.append(f"| {label} | {first['score']:.3f} | {last['score']:.3f} | {last['stableScore']:.3f} | {last['throttlePct']:.2f}% |")
rows += ['', '粒子单位为 GB/s，按计算阶段每粒子 40 B 推导；不是独立 DRAM 扫描带宽。Burn 单位为 Gpix-step/s。', '',
    '旧绘制对照重新出现下降：Metal 1M 32.94%，Vulkan 1M 13.69%。恢复固定画面覆盖后稳定，支持动态绘制负载影响的判断。新版本只绕回顶点显示坐标，计算内核和粒子数量不变；版本标记为 fixed_coverage_v2，与旧绘制成绩分组保存。iOS 普通按钮与一键按钮默认使用新版，无新增选项。其他平台默认显示方式不变。', '',
    'iOS MoltenVK Burn 始终以每帧提交／栅栏完成的实际耗时计分，完整运行都不依赖 GPU 查询。旧查询均值 1.367／1.623 继续保留为旧观察值；不能把新旧变化直接归因为硬件性能变化。Mac MoltenVK Burn 计时策略保留原方式。', '',
    '同一路径的 65K／4M／16M 绘制档位在两种 API 上也完成新版验证。系统热状态及每个结果的计时条件见 audit.json。日志中的吞吐下降说明已改为计时／负载／调度复核，不再自动断言温控降频。', '',
    '## 文件', '', '- [完整手机 results.json](policy-final/results/results.json)', '- [历史保留、驻留与计时核对](verification.json)',
    '- [逐轮清单、结果及原生日志](audit.json)', '- [全部 GPU 曲线 CSV](curves.csv)', '- [GPU 曲线 PNG](gpu-fix-curves.png) / [PDF](gpu-fix-curves.pdf)',
    '- [构建指纹](build-info.json)', '- [当前选定全套 33 条成绩](curated-suite-results.json)', '',
    '方法参考：[Apple QoS](https://developer.apple.com/library/archive/documentation/Performance/Conceptual/EnergyGuide-iOS/PrioritizeWorkWithQoS.html)、[公开 task_power_info_v2 结构](https://developer.apple.com/documentation/kernel/task_power_info_v2_data_t)、[XNU task CPU 时间实现](https://github.com/apple-oss-distributions/xnu/blob/main/osfmk/kern/task.c)。', '', '![A15 GPU fixes](gpu-fix-curves.png)']
(ROOT/'report.md').write_text('\n'.join(rows)+'\n')
print(json.dumps(summary,ensure_ascii=False,indent=2))
