from pathlib import Path
import csv,json,hashlib,collections,math
repo=Path(__file__).resolve().parents[3]
root=Path(__file__).resolve().parent
recent=repo/'out/test-results/apple-targeted-retest-20261014'
mac_path=Path('/Users/fairchild/Library/Application Support/GpuComputeBenchmark/results/results.json')
paths={'repository':repo/'results/results.json','mac':mac_path,'iphone':recent/'iphone-final/results/results.json','ipad':recent/'ipad-final/results/results.json'}
sources={k:json.loads(p.read_text()) for k,p in paths.items()}
hashes={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths.values()}
(root/'source-hashes.json').write_text(json.dumps(hashes,indent=2)+'\n')
conf=lambda r:dict(x.split('=',1) for x in r.get('workloadConfig','').split(';') if '=' in x)
selected={d:json.loads((recent/f'{d}-curated-suite-results.json').read_text()) for d in ['iphone','ipad']}
# Reuse the prior Mac viewport/capture selection, then choose latest CPU and memory records.
mac=json.loads((repo/'out/test-results/macos-complete-20261013/selected-results.json').read_text())
assert isinstance(mac,list)
latest=lambda pred:next(r for r in reversed(sources['mac']) if pred(r))
for i,r in enumerate(mac):
 if r['workload']=='cpu_single_core':mac[i]=latest(lambda q:q['workload']=='cpu_single_core' and 'qos_bound' not in q.get('workloadVersion','') and conf(q).get('totalMeasureSeconds')=='15')
 if r['workload']=='cpu_multi_core':mac[i]=latest(lambda q:q['workload']=='cpu_multi_core')
 if r['workload']=='cpu_membw':mac[i]=latest(lambda q:q['workload']=='cpu_membw' and conf(q).get('storageMeasured')=='1')
 if r['workload']=='membw' and r['graphicsApi']=='Vulkan':mac[i]=latest(lambda q:q['workload']=='membw' and q['graphicsApi']=='Vulkan')
selected['mac']=mac
assert all(len(rows)==32 for rows in selected.values())
(root/'selected-apple-results.json').write_text(json.dumps(selected,ensure_ascii=False,indent=2)+'\n')
# Preserve every source row in the audit export, including zero-score legacy rows and superseded records.
known_invalid={r['id'] for r in json.loads((repo/'out/test-results/apple-score-audit-20261014/score-audit.json').read_text()) if r['classification'].startswith('exclude')}
selected_ids={k:{r['id'] for r in rows} for k,rows in selected.items()}
ledger=[]
for source,rows in sources.items():
 for r in rows:
  flags=[]
  frame=r.get('avgFrameTimeMs',0);total=r.get('avgTotalGpuMs',0)
  interval=total/frame if frame>0 else None
  if r['id'] in known_invalid:flags.append('exclude_invalid_vulkan_headless_timing')
  if interval is not None and interval>1.1:flags.append('provisional_command_interval_exceeds_frame')
  if r['workload']=='cinematic_liquid' and r.get('avgComputeMs',0)+r.get('avgRenderMs',0)>total+0.1:flags.append('provisional_compute_render_overlap')
  if r.get('isSoftware'):flags.append('software_renderer_separate')
  if source=='repository' and not r.get('workloadVersion'):flags.append('legacy_contract_unrecorded')
  if source=='repository' and not r.get('scoreUnit'):flags.append('legacy_raw_timing_only_no_saved_score')
  if source=='mac' and r['graphicsApi']=='Vulkan' and not r.get('headless') and 'viewport1280x720_v1' not in r.get('workloadVersion',''):flags.append('old_retina_render_size_excluded')
  c=conf(r)
  if c.get('captureCount','0')!='0':flags.append('capture_run_separate')
  ledger.append(dict(source=source,id=r['id'],device=r['deviceName'],cpu=r.get('cpuName',''),api=r['graphicsApi'],workload=r['workload'],particles=r.get('particleCount',0),headless=r.get('headless'),score=r.get('score'),unit=r.get('scoreUnit',''),computeMs=r.get('avgComputeMs'),renderMs=r.get('avgRenderMs'),totalMs=total,frameMs=frame,fps=r.get('avgFps',1000/frame if frame>0 else None),version=r.get('workloadVersion',''),timingMode=c.get('timingMode',''),selected=r['id'] in selected_ids.get(source,set()),flags=';'.join(flags)))
with (root/'all-raw-history.csv').open('w',newline='') as f:
 w=csv.DictWriter(f,fieldnames=list(ledger[0]));w.writeheader();w.writerows(ledger)
(root/'all-raw-history.json').write_text(json.dumps(ledger,ensure_ascii=False,indent=2)+'\n')
# Convenient Apple metric selectors.
def pick(device,workload,api=None,n=None,headless=None,steps=None):
 return next(r for r in selected[device] if r['workload']==workload and (api is None or r['graphicsApi']==api) and (n is None or r['particleCount']==n) and (headless is None or r['headless']==headless) and (steps is None or conf(r).get('steps')==str(steps)))
metric_rows=[]
def add(title,unit,getter):
 vals={d:getter(d) for d in ['mac','iphone','ipad']}
 metric_rows.append(dict(metric=title,unit=unit,**vals))
add('CPU 单线程自动采样','MWork/s',lambda d:pick(d,'cpu_single_core')['score'])
add('CPU 全核','MWork/s',lambda d:pick(d,'cpu_multi_core')['score'])
add('CPU 全核内存读取','GB/s',lambda d:pick(d,'cpu_membw')['score'])
for n in [65536,1048576,4194304,16777216]:
 for api in ['Metal','Vulkan']:
  add(f'{api} 粒子{n:,}纯计算','GB/s',lambda d,n=n,api=api:pick(d,'stream',api,n,True)['score'])
add('GPU Vulkan 顺序读取','GB/s',lambda d:pick(d,'membw','Vulkan')['score'])
add('GPU Metal 固定网格诊断','GB/s',lambda d:pick(d,'membw','Metal')['score'])
for field,title,unit in [('single_memory_read','CPU 单线程内存读取','GB/s'),('single_L1_read','CPU 单线程L1读取','GB/s'),('single_L2_read','CPU 单线程L2读取','GB/s'),('pageFaultGBps','页面首次映射','GB/s'),('storageSeqReadMBps','存储顺序读','MB/s'),('storageSeqWriteMBps','存储顺序写','MB/s'),('storageRandReadMBps','存储随机4K读','MB/s'),('storageRandWriteMBps','存储随机4K写','MB/s')]:
 add(title,unit,lambda d,field=field:float(conf(pick(d,'cpu_membw'))[field]))
(root/'apple-metrics.json').write_text(json.dumps(metric_rows,ensure_ascii=False,indent=2)+'\n')
with (root/'apple-metrics.csv').open('w',newline='') as f:
 w=csv.DictWriter(f,fieldnames=list(metric_rows[0]));w.writeheader();w.writerows(metric_rows)
lines=['# 当前 Mac 与全部已找到历史成绩：2026-10-14',
'原始来源：当前Mac M4 Pro主库137条，iPhone A15原始历史67条，iPad A12X原始历史43条，仓库旧显卡220条（全部为粒子测试，很多没有保存分数/版本）。合计467条来源记录逐条导出，未将测试目录中的重复副本累加。另有HANDOFF中18台Android/鸿蒙设备的历史摘要；原始Windows adb/hdc目录未在当前Mac找到。摘要可供参考，不能冒充本轮重新核验的原始数据。',
'主表选用最新完整手机复测清单，Mac采用原有修正1280×720、关闭抓帧的32条选择，并更新至最新CPU全核/内存及Vulkan内存扫描；自动单线程采用最新15秒系统调度平均322.300。没有取历史最高分，也没有重新测试或修改任何原始结果。移动端为ios_preview，Mac为桌面合同；比值只能说明此内核/记录的观察，不是统一认证总分。',
'## 1. 当前三台设备：CPU、纯计算、内存与存储',
'| 项目 | 单位 | Mac M4 Pro | iPhone 13 Pro A15 | iPad Pro A12X |',
'|---|---|---:|---:|---:|']
for m in metric_rows:lines.append(f"| {m['metric']} | {m['unit']} | {m['mac']:.3f} | {m['iphone']:.3f} | {m['ipad']:.3f} |")
lines += [f'![三台设备主要项目对比](<{root}/apple-comparison.png>)','CPU全核分别14/6/8线程；单线程是系统调度采样。Mac另外已保存大核312.254、小核后台策略80.762 MWork/s，带≥99%归属校验；手机没有这类归属验证，不能把147.268/109.415分别称为大核或小核。Mac最新单线程缓存/内存来自验证过的大核组，手机对应系统调度单线程，条件不同。',
'Mac CPU全核相对iPhone约5.47倍、相对iPad约7.28倍；Metal16M纯计算分别约7.55/4.86倍。Vulkan独立顺序内存读取252.395，相对iPhone29.298约8.62倍、相对iPad56.760约4.45倍。粒子GB/s是按40字节估算的更新吞吐，不是DRAM带宽；Metal固定网格最佳值没有验证内存平台，也不能与Vulkan中位数顺序扫描相除推导API开销。页面首次映射不是swap换入/换出，三台都未单独测实际交换/压缩。',
'## 2. GPU绘制：保留原始分数，限制解释',
'**新发现：Mac旧Metal绘制的1M/4M/16M粒子、三档GPU Burn也出现总命令跨度大于帧间隔；流体compute+render重叠。因此这些旧分数标为待复测，不参与得分倍数排行。手机的指定项已经使用新计时合同复测。此前Mac报告的“Vulkan得分高33%–57%”不能沿用为真实API优势。**',
'| 项目 | Mac Metal | Mac Vulkan | iPhone Metal | iPhone Vulkan | iPad Metal | iPad Vulkan | 单位 |',
'|---|---:|---:|---:|---:|---:|---:|---|']
for n in [65536,1048576,4194304,16777216]:
 vals=[]
 for d in ['mac','iphone','ipad']:
  for api in ['Metal','Vulkan']:
   r=pick(d,'stream',api,n,False);vals.append(f"{r['score']:.3f}"+('†' if d=='mac' and api=='Metal' and n>=1048576 else ''))
 lines.append('| 粒子 '+f'{n:,}'+'绘制 | '+' | '.join(vals)+' | GB/s |')
for steps in [16,64,256]:
 vals=[]
 for d in ['mac','iphone','ipad']:
  for api in ['Metal','Vulkan']:
   r=pick(d,'gpu_burn',api,steps=steps);vals.append(f"{r['score']:.3f}"+('†' if d=='mac' and api=='Metal' else ''))
 lines.append('| GPU Burn '+str(steps)+'步 | '+' | '.join(vals)+' | Gpix-step/s |')
lines += ['†=Mac计时待复测，只保留历史值。手机Vulkan16/64步沿用原有双帧成绩；256步为单帧复测。MacVulkan仍是双帧合同。不能把这些值汇成严格跨平台API总排名。',
'实际FPS更容易观察绘制表现，但Mac不跟显示刷新，iOS直接呈现且调度不同，仍是应用表现：',
'| 项目 | Mac Metal / Vulkan FPS | iPhone Metal / Vulkan FPS | iPad Metal / Vulkan FPS |',
'|---|---:|---:|---:|']
for work,n,steps,title in [('stream',1048576,None,'1M粒子绘制'),('stream',4194304,None,'4M粒子绘制'),('stream',16777216,None,'16M粒子绘制'),('gpu_burn',None,256,'GPU Burn256'),('cinematic_liquid',None,None,'流体')]:
 cells=[]
 for d in ['mac','iphone','ipad']:
  vals=[]
  for api in ['Metal','Vulkan']:
   try:r=pick(d,work,api,n,False,steps);vals.append(f"{r.get('avgFps',1000/r['avgFrameTimeMs']):.2f}")
   except StopIteration:vals.append('不支持')
  cells.append(' / '.join(vals))
 lines.append('| '+title+' | '+' | '.join(cells)+' |')
lines += ['GPU缓存诊断Mac/iPhone/iPad版本分别v1/v2/v3，每链读取/预热条件不同，不能用340.717/311.192/356.150 ns/load排芯片快慢，也没有独立确认GPU L1/L2/L3/SoC-SLC归属。曲线原样保留在各自档案。',
'## 3. 以前Android / 鸿蒙设备：历史摘要参考',
'下面引用仓库HANDOFF记录，不是当前Mac已取得的原始JSON。GPU列固定为Vulkan16M纯计算、Vulkan顺序读取；GPU Burn列采用更完整的ES3.1 256步历史记录（不能与ES2.0混排）。不支持/无值以—表示，避免切换API凑一列。32位CPU版本、设备限制与温度不同，不作正式统一排行。',
'| 设备 | CPU全核 MWork/s | Vulkan16M纯计算 GB/s | Vulkan内存读 GB/s | ES3.1 Burn256 Gpix-step/s | 备注 |',
'|---|---:|---:|---:|---:|---|']
# These entries are explicitly handoff summaries, preserving the later corrections.
history=[
 ('MIX Fold3 / SD8 Gen2',936.75,26.14,53.0,3.08,'CPU8线程；内存扫描上限128MiB，未到平台；源2286/2084'),
 ('Quest3 / XR2 Gen2',608.5,14.79,32.6,2.64,'CPU6线程；头显频率限制；ES新方法；源2253/1758'),
 ('黑鲨5Pro / SD8 Gen1',844.99,19.24,44.17,1.90,'CPU8线程；cpu6受到额外干扰；源2012/1749'),
 ('MIX4 / SD888+',758.80,20.85,32.7,1.09,'内存校验后改为32.7；16M512MiB粒子缓冲未做同类校验；源1972/1665'),
 ('黑鲨3Pro / SD865',773.17,13.46,23.2,0.81,'CPU8线程；256MiB内存校验；16M粒子缓冲未核验；源1941/1665'),
 ('小米平板5 / SD860',607.79,10.17,24.6,0.66,'Vulkan Burn256不稳，使用ES列；源1913/1665'),
 ('MIX2S / SD845',548.44,9.07,15.4,0.48,'Vulkan Burn256不稳；源1874/1665'),
 ('小米5C / 澎湃S1',416.18,None,None,0.079,'CPU8线程强制全速状态；多数GPU项不支持；源1828'),
 ('Switch / TegraX1',273.97,5.13,11.2,0.74,'内存时钟800MHz；CPU4线程；源1714'),
 ('小米平板3 / MT8176',352.11,None,None,0.076,'CPU6线程；无Vulkan；重档只3帧；源1625'),
 ('初代小米平板 / TegraK1',129.77,None,None,0.57,'32位CPU4线程；另有performance策略130.92，单独保留；无Vulkan；源1583/1510'),
 ('小米平板2 / Atomx5',105.58,None,None,0.173,'32位CPU4线程；无Vulkan；重档5帧；源1549'),
 ('小米3 / Tegra4',94.79,None,None,None,'CPU4线程强制在线后；仅CLI日志；ES2独立256步0.45；源1510'),
 ('Quest2 / XR2',359.23,7.72,14.2,0.66,'CPU4线程；GPU限制490–525MHz；源1455'),
 ('小米电视 / MT9655',224.23,None,5.2,0.094,'CPU4线程；16M不稳定；重档2–3帧；源1423'),
 ('TCL电视 / MT5889',69.95,2.33,2.3,0.039,'32位CPU4线程；16M超驱动声明范围未验数据；Burn仅1帧；源1365'),
 ('MateBookPro / KirinX90',1901.70,17.79,60.2,1.21,'CPU20线程；CPU内存扫描10线程；源1315'),
 ('Mate60Pro / Kirin9000S',697.10,7.86,29.3,0.63,'CPU12线程；CPU内存扫描8线程；源1265'),
]
historical_rows=[dict(device=name,cpuAllMWork=cpu,vulkan16MGBps=gpu,vulkanReadGBps=mem,es31Burn256Gpix=b,notes=note) for name,cpu,gpu,mem,b,note in history]
(root/'historical-mobile-summary.json').write_text(json.dumps(historical_rows,ensure_ascii=False,indent=2)+'\n')
with (root/'historical-mobile-summary.csv').open('w',newline='') as f:
 w=csv.DictWriter(f,fieldnames=list(historical_rows[0]));w.writeheader();w.writerows(historical_rows)
for name,cpu,gpu,mem,burn,note in history:
 fmt=lambda x:f'{x:.3f}' if x is not None else '—'
 lines.append('| '+name+' | '+' | '.join(fmt(x) for x in [cpu,gpu,mem,burn])+' | '+note+' |')
lines += ['历史上RTX5090独立内存校验记录1544.6 GB/s（HANDOFF1665节），当前M4Pro252.395约为其16.3%；这是不同显存/统一内存平台的同内核观察。其余旧显卡当前220条原始文件只包含粒子项，没有GPU Burn/CPU/内存分数，不能从粒子时间补造这些项目。',
'## 4. 仓库旧显卡：相同1M纯计算的原始耗时',
'220条旧记录多数没有workloadVersion与score。下表直接使用保存的计算毫秒，越低越快；不把由舍入时间反算的GB/s冒充原有分数。DX11的0ms按计时不可用处理。先按同API/同1M/headless分组，保留原驱动与版本不足的限制。',
'| 设备 | Vulkan计算 ms | DX12计算 ms | DX11计算 ms | OpenGL计算 ms |',
'|---|---:|---:|---:|---:|']
legacy=sources['repository']
names=sorted({r['deviceName'] for r in legacy if r['headless'] and r['particleCount']==1048576 and not r['isSoftware']})
for name in names:
 cells=[]
 for api in ['Vulkan','DX12','DX11','OpenGL']:
  rs=[r for r in legacy if r['deviceName']==name and r['graphicsApi']==api and r['headless'] and r['particleCount']==1048576]
  value=rs[-1]['avgComputeMs'] if rs else None
  cells.append(f'{value:.3f}' if value is not None and value>0 else '无有效计时' if value==0 else '—')
 lines.append('| '+name+' | '+' | '.join(cells)+' |')
lines += ['当前M4Pro同档Metal0.286ms、Vulkan0.191ms；iPhoneMetal2.041ms、Vulkan1.980ms；iPadMetal1.389ms、Vulkan同步1.854ms。iPadVk包含提交等待，版本不同。小耗时舍入误差、驱动历史与呈现条件不同，旧显卡表不形成综合GPU等级结论。',
'无Headless配对的其他显卡与软件/虚拟设备仍全部进入all-raw-history.csv/json。220条的完整设备/API/数据量/模式统计如下：',
'| 原始设备名 | 记录数 | API |',
'|---|---:|---|']
for name in sorted({r['deviceName'] for r in legacy}):
 rs=[r for r in legacy if r['deviceName']==name]
 lines.append('| '+name+' | '+str(len(rs))+' | '+', '.join(sorted({r['graphicsApi'] for r in rs}))+' |')
lines += ['## 5. 同一台Mac的旧记录波动与限制',
'Mac137条中有早期几秒预览、抓帧、不同渲染尺寸、完整15秒、CPU不同归属策略。历史最高值不能混成一套“最佳成绩”。例如默认15秒CPU全核3010.644→3726.693→最新3581.602，变化既有系统状态也有序列条件，不能据此称硬件升级或回退；短时1秒3734.093不进入正式15秒比较。Metal16M纯计算原有完整本轮143.277GB/s，与早期113–149GB/s有波动。',
'尤其Mac最新Metal Burn256旧分4.386 Gpix-step/s，总命令跨度107.592ms、帧间隔55.089ms（约195%）；流体compute38.811+render47.953ms、total48.011ms。这些具有与手机修复前相同的口径风险，应该单帧核验后再用分数与手机、Vulkan比较。本次用户要求对比，没有重新启动Mac测试。',
'## 6. 输出文件',
'- `apple-metrics.csv/json`：三台最新选择的CPU、纯计算、内存与存储。\n- `selected-apple-results.json`：各32条完整选择，保留原始ID与合同。\n- `all-raw-history.csv/json`：467条原始来源记录及异常/旧版标记，不改原始文件。\n- `historical-mobile-excerpts.md`：18台旧设备的原摘要表、后续更正及来源行。\n- `source-hashes.json`：本次读取来源的SHA256。']
report=lines[0]
for prev,line in zip(lines,lines[1:]):report+= ('\n' if prev.startswith('|') and line.startswith('|') else '\n\n')+line
(root/'report.md').write_text(report+'\n')
# Preserve every previous mobile table, rather than silently omitting unsupported categories.
s= (repo/'HANDOFF.md').read_text().splitlines()
sections=[]
for i,line in enumerate(s):
 if line.startswith('### ') and 1260<i<2373:
  end=next((j for j in range(i+1,len(s)) if s[j].startswith('### ')),len(s))
  tables=[(j,l) for j,l in enumerate(s[i+1:end],i+1) if l.lstrip().startswith('|')]
  if '设备' in line or any(t in line for t in ['显存带宽加了数据校验','Quest 3：','MIX Fold 3 补跑','缓存 / 内存成绩','GPU Burn 要跑']):
   sections.append(f'## {line[4:]}\n\n来源：[HANDOFF.md:{i+1}](<{repo}/HANDOFF.md:{i+1}>)\n')
   previous_line=None
   for j,l in tables:
    if previous_line is not None and j>previous_line+1:sections.append('')
    sections.append(l.strip());previous_line=j
   sections.append('')
(root/'historical-mobile-excerpts.md').write_text('# 旧移动设备测试摘要原表\n\n原始adb/hdc JSON未在当前Mac找到；本文件仅整理仓库已有记录，不执行复测。主报告采用后续更正值。\n\n'+'\n'.join(sections)+'\n')
print('Sources:',{k:len(v) for k,v in sources.items()},'total',len(ledger))
print('Selected Apple metric rows:',len(metric_rows),'legacy physical Headless devices',len(names))
