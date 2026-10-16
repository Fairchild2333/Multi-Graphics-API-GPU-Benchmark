import json, hashlib, math
from pathlib import Path
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
root=Path(__file__).resolve().parent
sources={'iPad A12X':root.parent/'ipadpro3-20261014','iPhone A15':root.parent/'ios-api-completion-20261014'}
rows={name:json.loads((folder/'suite-results.json').read_text()) for name,folder in sources.items()}
checks=[]
for name,rs in rows.items():
 for r in rs:
  flags=[]; classification='plausible_observation'
  ratio=r['avgTotalGpuMs']*r['avgFps']/1000 if r['avgFps'] else None
  cross=40*r['particleCount']*r['avgFps']/1e9 if r['workload']=='stream' else None
  if name=='iPad A12X' and r['graphicsApi']=='Vulkan' and r['workload']=='stream' and r['headless'] and r['particleCount']>=1048576:
   flags.append('invalid_compute_timestamp_metric');classification='exclude_reported_score_pending_retest'
  if r['graphicsApi']=='Metal' and not r['headless'] and ratio and ratio>1.1:
   flags.append('command_interval_fps_ratio_above_one');classification='provisional_render_timing'
  if r['workload']=='cinematic_liquid' and r['graphicsApi']=='Metal' and r['avgComputeMs']+r['avgRenderMs']>1.1*r['avgTotalGpuMs']:
   flags.append('compute_render_intervals_overlap_score_sums_them');classification='provisional_render_timing'
  if r['bottleneck']=='Display-bound':flags.append('display_pacing_affects_interpretation')
  if r['graphicsApi']=='OpenGL ES 2.0':flags.append('legacy_shader_and_display_paced_sync_timing')
  if r['workload']=='gpu_cache':flags.append('different_parallel_profile_between_devices_no_ratio')
  if r['workload']=='cpu_single_core':flags.append('scheduler_managed_samples_not_pinned_cores')
  checks.append({'device':name,'id':r['id'],'workload':r['workload'],'api':r['graphicsApi'],'particles':r['particleCount'],'headless':r['headless'],
   'reportedScore':r['score'],'unit':r['scoreUnit'],'classification':classification,'flags':flags,'avgComputeMs':r['avgComputeMs'],
   'avgFrameMs':1000/r['avgFps'] if r['avgFps'] else None,'fps':r['avgFps'],'intervalFpsRatio':ratio,
   'wallFrameCrosscheckGBps':cross,'reportedVsWallCrosscheckRatio':r['score']/cross if cross else None})
(root/'score-audit.json').write_text(json.dumps(checks,ensure_ascii=False,indent=2)+'\n')
invalid=[c for c in checks if c['classification']=='exclude_reported_score_pending_retest']
assert len(invalid)==3
lookup=lambda dev,work,api='CPU',headless=None,particles=None:next(r for r in rows[dev] if r['workload']==work and r['graphicsApi']==api and (headless is None or r['headless']==headless) and (particles is None or r['particleCount']==particles))
selected=[('CPU 单线程采样平均','cpu_single_core','CPU',None,None),('CPU 全核','cpu_multi_core','CPU',None,None),
 ('CPU 内存全核读取','cpu_membw','CPU',None,None),('GPU 顺序内存扫描','membw','Vulkan',None,None),
 ('Metal 粒子 16M 纯计算','stream','Metal',True,16777216)]
text=['# iPhone 13 Pro 与 iPad Pro A12X：成绩比较与可信度审计\n',
 '**有明确异常。iPad 的 Vulkan 1M／4M／16M 纯计算三条带宽分数应排除，不能解释为 32–80 倍的性能提升。**此前“31/31通过、32条有效”只验证任务完成、正数、版本和保存；没有验证每条计时是否覆盖真实工作。该描述过强，现更正为“31项完成、32条保存记录，其中有异常指标需重测”。原始 JSON 和日志原样保留。\n',
 '目前较适合参考的同项观察如下。这些是单轮、非等温结果；CPU 单线程是调度采样，全核分别为 iPhone 6 线程、iPad 8 线程。\n',
 '| 项目 | iPhone 13 Pro / A15 | iPad Pro / A12X | iPad/iPhone |\n|---|---:|---:|---:|']
summary=[]
for title,work,api,headless,particles in selected:
 a=lookup('iPhone A15',work,api,headless,particles);b=lookup('iPad A12X',work,api,headless,particles)
 ratio=b['score']/a['score'];summary.append({'title':title,'iphone':a['score'],'ipad':b['score'],'unit':a['scoreUnit'],'ratio':ratio})
 text.append(f"| {title} | {a['score']:.3f} {a['scoreUnit']} | {b['score']:.3f} {b['scoreUnit']} | {ratio:.2f}× |")
text+=['\n这组数据里，iPad 的 CPU 算力约为 iPhone 的 74–75%；CPU 内存读高约48%，GPU顺序内存读约1.94倍。Metal大工作集纯计算约高55%。不能据此合并成一个“GPU整体强多少”的结论。\n',
 '## 1. 明确异常：iPad Vulkan 纯计算时间戳\n',
 '| 粒子数 | 报告分数 GB/s | GPU 计算时间 ms | 实际帧间隔 ms | 帧率交叉核对 GB/s |\n|---|---:|---:|---:|---:|']
for x in invalid:text.append(f"| {x['particles']:,} | {x['reportedScore']:.3f} | {x['avgComputeMs']:.3f} | {x['avgFrameMs']:.3f} | {x['wallFrameCrosscheckGBps']:.3f} |")
text+=['\n交叉核对只按“每粒子40字节 × 粒子数 × 实际FPS”计算，用来检验数量级，**不是替换后的正式GPU成绩**。三档实际约28.5–28.9 GB/s，与同机Metal约29.5–30.2 GB/s接近；GPU查询却只覆盖帧间隔的一小部分，分数被放大约21–60倍。日志连续十余秒都显示该矛盾，不能用缓存加速或偶然波动解释。\n',
 '本地 VulkanBackend 的 CheckTimestampsAgainstWallClock 明确跳过 headless，只校验窗口粒子路径；CollectTimestampResults仅检查空／非正间隔，没有对这些非零但严重偏小的间隔做外部校验。因此现有成功门槛漏掉了这三条。具体驱动／采样路径还需GPU trace或同步计时重测确认，不能直接断言GPU芯片故障。\n',
 'GPU顺序内存扫描是另一条代码路径，带有工作量放大和独立提交／等待校验。iPad日志中4096 MiB读取GPU计时71.539 ms、墙钟72.358 ms；iPhone139.944／140.817 ms，两者接近，且报告timestampAnomalies=0。它们没有出现上述数十倍矛盾，因此56.760／29.298 GB/s这组比值更值得参考。\n',
 '## 2. 可疑口径：Metal 绘制间隔与“利用率”\n',
 'iPhone Metal GPU Burn 三档的 avgTotalGpuMs × FPS 为1.91、1.98、1.98；iPad重档为1.87。界面相当于显示190%–200%“利用率”，它不是实际GPU忙碌百分比。本地实现用命令缓冲开始／结束跨度，随后把跨度与帧间隔相除。并发提交、重叠或等待可能造成这些区间不等同于互斥执行时间；具体贡献尚未由trace拆分。\n',
 'Metal流体也有重叠迹象：iPad compute358.243 + render471.078 ms，但total471.141 ms；iPhone174.623 +284.895 ms，但total285.005 ms。当前分数用compute+render作分母，需要核查重复计时。流体实际FPS为iPhone3.5、iPad2.1，可作为此次流程的观测；MParticle-step/s先不作严格硬件比较。\n',
 'iPhone Vulkan GPU Burn256报2.098，Metal报0.811 Gpix-step/s，但实际FPS反而为Vulkan1.7、Metal3.4。GPU内核时间与呈现时间可能不同，但这组矛盾意味着不能直接宣称Vulkan比Metal快2.6倍。两种API的绘制计时均应做单帧、同步以及多帧校验。\n',
 '## 3. ES2轻档：刷新／同步影响\n',
 'ES2 16步时，iPad117.9 FPS／3.501 Gpix-step/s，iPhone49.3 FPS／1.458 Gpix-step/s，分数比2.40与FPS比2.39接近。64步两机各为81.6与48.7 FPS；256步变为32.5与32.1 FPS，分数也几乎相同。低档成绩明显受呈现／同步节奏影响，不能把2.4倍直接解释为纯GPU算力。ES2使用独立旧版着色器，也不能与Metal/Vulkan现代GPU Burn数值混排。\n',
 '## 4. 看起来奇怪但已有解释的项目\n',
 '- 大工作集粒子分数比小档更高：绘制版分数只统计计算阶段的GB/s，渲染又会改变争用与占空比；16M分数高不代表画面更流畅。应同时看FPS、计算时间和渲染时间。\n- GPU缓存小工作集的几百GB/s不等于DRAM速度；缓存曲线的ns/load又是另一单位。iPad缓存v3每链512次、iPhonev2每链8192次，配置不同，不作两机比值。\n- 单线程采样全部相近不是大小核一样快：iPad八次为108.4–109.8，iPhone六次为147.1–147.4 MWork/s。它们由系统调度，没有绑到八／六个不同物理核心。日志中的residency_verified=1同时带requested_lower_bound=0，不能当作P/E驻留证明。\n- iPad CPU内存读取51.2、Vulkan内存56.8、Metal诊断59.9 GB/s在同一量级；不同读写／统计方法会有差异。Metal诊断仍标记memoryPlateauVerified=0，不应叫硬件标称带宽。\n',
 '## 保存与结论限制\n',
 '原始64条选定记录、原日志和已有结果文件都未改写。score-audit.json逐条标出需要排除、暂定或不可直接比较的原因；crosscheck仅辅助检验，不冒充复测成绩。优先需要修复并复测的项目：iPad Vulkan纯计算1M/4M/16M；两机Metal绘制／流体计时；iPhone Vulkan重档与刷新限制。CPU和独立内存扫描可暂用于比较，但不是经过多轮冷热控制的最终硬件定论。\n',
 'MoltenVK的具体硬件／仿真时间戳路径可查其[固定1.4.2源码](https://github.com/KhronosGroup/MoltenVK/blob/v1.4.2/MoltenVK/MoltenVK/GPUObjects/MVKQueryPool.mm)。源码存在无计数缓冲时的回退路径；这不能单凭代码就证明本次用了该路径。其[官方时间戳说明](https://github.com/KhronosGroup/MoltenVK/blob/v1.4.2/Docs/MoltenVK_Runtime_UserGuide.md#timestamping)也不能替代本次外部计时校验。\n']
(root/'comparison-audit.md').write_text('\n'.join(text))
(root/'comparison-summary.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2)+'\n')
(root/'source-hashes.json').write_text(json.dumps({name:hashlib.sha256((path/'suite-results.json').read_bytes()).hexdigest() for name,path in sources.items()},indent=2)+'\n')
fig,ax=plt.subplots(figsize=(9,5),layout='constrained')
labels=['CPU single','CPU all','CPU memory read','GPU sequential read','Metal 16M compute']; vals=[s['ratio'] for s in summary]
ax.barh(labels[::-1],vals[::-1],color=['#4b91ac']*5);ax.axvline(1,color='#555',ls='--',lw=1)
for y,v in enumerate(vals[::-1]):ax.text(v+.02,y,f'{v:.2f}x',va='center')
ax.set_xlim(0,2.25);ax.set_xlabel('iPad / iPhone ratio · throughput (higher is larger)')
ax.set_title('Selected comparable observations · A12X iPad vs A15 iPhone')
ax.spines[['top','right']].set_visible(False)
fig.text(.5,0,'One recorded suite per device; scheduler-managed CPU and thermal/pacing limitations apply. Suspect GPU scores excluded.',ha='center',fontsize=8)
fig.savefig(root/'comparison.png',dpi=160);plt.close(fig)
print('Raw records inspected:',len(checks),'Definite excluded metrics:',len(invalid),'Provisional render timing:',sum(c['classification']=='provisional_render_timing' for c in checks))
