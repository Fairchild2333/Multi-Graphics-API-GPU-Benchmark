from pathlib import Path
import json,csv,hashlib,collections,html,math
root=Path.cwd(); shared=Path.home()/'Library/Application Support/GpuComputeBenchmark'
dest=shared/'reports/all-results-2026-10-13';dest.mkdir(parents=True,exist_ok=True)
sources=[(shared/'results/results.json','当前 Mac 主成绩','main'),(root/'results/results.json','仓库旧成绩','legacy'),(root/'out/test-results/native-smoke-data/results/results.json','隔离验证：Apple 适配','validation'),(root/'out/test-results/macos-available-suite/current-data/results/results.json','隔离验证：API 流程','validation'),(root/'out/test-results/macos-available-suite/smoke-data/results/results.json','隔离验证：OpenGL','validation')]
source_meta=[];items={}
for path,label,role in sources:
    raw=path.read_bytes();rows=json.loads(raw);source_meta.append({'path':str(path),'label':label,'role':role,'count':len(rows),'sha256':hashlib.sha256(raw).hexdigest()})
    for row in rows:
        assert row['id'] not in items,'Conflicting duplicate ID: '+row['id']
        items[row['id']]={'result':row,'source':label,'sourceFile':str(path),'role':role}
ordered=sorted(items.values(),key=lambda x:x['result']['id'],reverse=True)
assert len(ordered)==367
snapshot_audit=[]
for path in sorted((root/'out/test-results').rglob('*results.json')):
    if 'plot-env' in path.parts or path.name=='results.json':continue
    try:rows=json.loads(path.read_text())
    except (ValueError,OSError):continue
    if not isinstance(rows,list) or not all(isinstance(r,dict) and 'id' in r for r in rows):continue
    extra=[r['id'] for r in rows if r['id'] not in items]
    assert not extra, (path,extra)
    snapshot_audit.append({'path':str(path),'count':len(rows),'newIDs':extra})
def config(r):
    return dict(p.split('=',1) for p in (r.get('workloadConfig') or '').split(';') if '=' in p)
def managed(r):
    return 'scheduler_managed' in (r.get('workloadVersion') or '') or any('scheduler_managed' in v for k,v in config(r).items() if 'affinity' in k.lower())
def name(r):
    return {'stream':'粒子吞吐（推算值）','gpu_burn':'GPU Burn','cinematic_liquid':'互动水池','cpu_single_core':'单线程采样（系统调度）' if managed(r) else 'CPU 单核','cpu_multi_core':'CPU 多线程','cpu_membw':'CPU 缓存/内存','membw':'GPU 内存扫描','gpu_cache':'GPU 缓存延迟诊断'}.get(r['workload'],r['workload'])
def flags(item):
    r=item['result'];v=r.get('workloadVersion') or '';f=[]
    if item['role']=='validation':f.append('隔离短测，验证用途')
    if not v:f.append('旧格式，版本未记录')
    if r.get('isSoftware'):f.append('软件渲染器')
    if 'preview' in v:f.append('预览/诊断')
    if managed(r) and r['workload']=='cpu_single_core':f.append('未绑定核心，不能分 P/E')
    if 'moltenvk' in v:f.append('MoltenVK 转译')
    if r['workload']=='gpu_cache':f.append('L2/L3/SLC 未确认')
    if v=='metal_memory_sweep_v1_preview':f.append('内存尾段未验证')
    if 'moltenvk' in v and not r.get('headless') and 'viewport1280x720_v1' not in v:f.append('旧 Retina 尺寸组')
    return '；'.join(f) or '保留原始测试条件'
def mode(r):
    if r['graphicsApi']=='CPU':return '系统调度' if managed(r) or config(r).get('affinity')=='scheduler_managed' else '按文件记录'
    return '纯计算' if r.get('headless') else '带渲染'
def particles(r):
    n=r.get('particleCount',0);return str(n) if n else '—'
def val(r):return f"{r.get('score',0):.3f} {r.get('scoreUnit','')}"
rawrows=[x['result'] for x in ordered]
(dest/'all-results.json').write_text(json.dumps(rawrows,ensure_ascii=False,indent=2)+'\n')
(dest/'main-results.json').write_text(json.dumps([x['result'] for x in ordered if x['role']!='validation'],ensure_ascii=False,indent=2)+'\n')
fields=sorted(set().union(*(r.keys() for r in rawrows)))
with (dest/'all-results.csv').open('w',newline='',encoding='utf-8-sig') as f:
    writer=csv.DictWriter(f,fieldnames=['sourceLabel','sourceFile','sourceRole','displayWorkload','interpretation']+fields);writer.writeheader()
    for x in ordered:
        r={k:json.dumps(v,ensure_ascii=False) if isinstance(v,(dict,list)) else v for k,v in x['result'].items()}
        writer.writerow({'sourceLabel':x['source'],'sourceFile':x['sourceFile'],'sourceRole':x['role'],'displayWorkload':name(x['result']),'interpretation':flags(x),**r})
apis=collections.Counter(r['graphicsApi'] for r in rawrows); workloads=collections.Counter(r['workload'] for r in rawrows); devices=collections.Counter(r['deviceName'] for r in rawrows)
current=[x['result'] for x in ordered if x['role']=='main']
latest=lambda pred:max((r for r in current if pred(r)),key=lambda r:r['id'])
cp=latest(lambda r:r['workload']=='cpu_single_core' and '_formal_' in r.get('workloadVersion',''))
cm=latest(lambda r:r['workload']=='cpu_multi_core' and '_formal_' in r.get('workloadVersion',''))
mem=latest(lambda r:r['workload']=='cpu_membw' and r.get('workloadVersion')=='cpu_membw_v1');mf=config(mem)
vm=latest(lambda r:r['workload']=='membw' and r['graphicsApi']=='Vulkan');vf=config(vm)
mm=latest(lambda r:r['workload']=='membw' and r['graphicsApi']=='Metal')
lines=['# 所有结果汇总（2026-10-13）','',
'## 单线程成绩的含义','',
'这批单线程成绩没有分别测量大核和小核。Mac 实现没有执行逐核绑定，14 条采样全部是 `scheduler_managed`、`Unclassified`、`performance_level=-1`。编号只是采样序号。系统可以反复选择同类核心，因此相近分数不能说明大小核性能相同。','',
f'正式单线程均值是 **{cp["score"]:.1f} MWork/s**，14 次采样范围 313.4–324.4；它可作为当前系统调度下的单线程吞吐。正式 14 线程总吞吐 **{cm["score"]:.1f} MWork/s**。当前没有经过核心归属验证的 P 核/E 核各自成绩。',
'[实现依据](<'+str(root/'src/cpu_benchmark.cpp')+':428>)；[原始单线程日志](<'+str(root/'out/test-results/cache-cpu-20261013/cpu-compute-all.log')+'>)；[Apple 调度说明](https://developer.apple.com/documentation/apple-silicon/tuning-your-code-s-performance-for-apple-silicon)。','',
'## 数据范围','',
'共 **367 条不同 ID**：当前 Mac 主成绩 **133**，仓库旧成绩 **220**，另有隔离验证用成绩 **14**。两个主历史文件合计 **353**。备份、精选列表和前后快照都是副本，不重复统计。所有原始成绩文件保持不变；本目录是独立只读汇总导出。','',
'| 来源 | 条数 | 用途 |','|---|---:|---|']
for s in source_meta:lines.append(f'| {s["label"]} | {s["count"]} | '+('主历史' if s['role']!='validation' else '隔离验证，不能并入正式排名')+' |')
lines+=['','| API | 条数 |','|---|---:|']+[f'| {a} | {n} |' for a,n in sorted(apis.items())]
lines+=['','| 负载 | 条数 |','|---|---:|']+[f'| {w} | {n} |' for w,n in sorted(workloads.items())]
lines+=['',
'## 当前 M4 Pro 主要成绩','',
'下表保留 API、模式、粒子档位和版本；Vulkan 带渲染采用修正后的 1280×720 组。流体两种实现有不同计分合同，内存预览也不能与正式扫描直接排名。粒子 GB/s 是内核推算吞吐，并非统一内存实测。','',
'| 项目 | Metal | Vulkan（MoltenVK） | OpenGL | 单位 |','|---|---:|---:|---:|---|']
for n,label in [(65536,'65K'),(1048576,'1M'),(4194304,'4M'),(16777216,'16M')]:
    for head,ml in [(False,'带渲染'),(True,'纯计算')]:
        m=latest(lambda r:r['workload']=='stream' and r['graphicsApi']=='Metal' and r['particleCount']==n and r['headless']==head)
        v=latest(lambda r:r['workload']=='stream' and r['graphicsApi']=='Vulkan' and r['particleCount']==n and r['headless']==head and (head or 'viewport1280x720_v1' in (r.get('workloadVersion') or '')))
        lines.append(f'| 粒子 {label} · {ml} | {m["score"]:.3f} | {v["score"]:.3f} | — | GB/s（推算） |')
for steps in [16,64,256]:
    row=[]
    for api in ['Metal','Vulkan','OpenGL']:
        r=latest(lambda r:r['workload']=='gpu_burn' and r['graphicsApi']==api and f'_fixed_steps_{steps}_' in r.get('workloadVersion','') and (api!='Vulkan' or 'viewport1280x720_v1' in r['workloadVersion']))
        row.append(f'{r["score"]:.3f}')
    lines.append('| GPU Burn '+str(steps)+' | '+' | '.join(row)+' | Gpix-step/s |')
liquid=[latest(lambda r:r['workload']=='cinematic_liquid' and r['graphicsApi']==api and (api!='Vulkan' or 'viewport1280x720_v1' in r.get('workloadVersion',''))) for api in ['Metal','Vulkan']]
lines+= [f'| 水池 · 不同计分合同 | {liquid[0]["score"]:.3f} | {liquid[1]["score"]:.3f} | — | MParticle-step/s |',f'| GPU 内存读 · 不同测量方法 | {mm["score"]:.3f}（预览） | {vm["score"]:.3f} | — | GB/s |','',
f'CPU：正式单线程采样 **{cp["score"]:.1f} MWork/s**；14 线程 **{cm["score"]:.1f} MWork/s**；全核内存读/写/复制 **{mf["all_memory_read"]} / {mf["all_memory_write"]} / {mf["all_memory_copy"]} GB/s**。',
f'CPU 单线程参考缓存读：L1 {mf["single_L1_read"]}、L2 {mf["single_L2_read"]} GB/s；它们仍不代表已区分的 P/E 核各自缓存速度。',
f'Vulkan 缓存命中段读 **{vf["readCacheGBps"]} GB/s**；局部/L1 型全 GPU **{vf["l1FullLoadGBps"]}**、单工作组 **{vf["l1SingleGroupGBps"]} GB/s**。不能据此独立确认 L2/L3/SLC。',
'[CPU 与 GPU 缓存完整曲线报告](<'+str(shared/'reports/cache-cpu-complete-2026-10-13.md')+'>)。','',
'## 全部成绩与原始条件','',
'[可筛选的全部成绩表](<'+str(dest/'all-results.html')+'>) · [完整 CSV](<'+str(dest/'all-results.csv')+'>) · [367 条完整 JSON](<'+str(dest/'all-results.json')+'>) · [353 条主历史 JSON](<'+str(dest/'main-results.json')+'>)。',
'旧格式 220 条均未记录 workloadVersion；保留原分数与参数，不混入当前版本的排名。软件渲染器单独标注。隔离短测可能未越过缓存，较高 GB/s 不能作为 DRAM 带宽。时间列保留源文件原始字符串，不推断旧成绩的时区。','',
'## 按原始设备名称计数','',
'同一设备可能有不同 API 别名或节点编号，下表的名称数量不等于物理设备数量。','',
'| 原始设备名 | 条数 |','|---|---:|']+[f'| {d.replace("|","/")} | {n} |' for d,n in devices.most_common()]
lines+=['','## 全部 367 条明细','',
'| ID | 来源 | 设备 | API | 项目 | 模式 | 粒子数 | 成绩 | 版本 | 说明 |','|---|---|---|---|---|---|---:|---|---|']
for x in ordered:
    r=x['result'];cells=[r['id'],x['source'],r['deviceName'],r['graphicsApi'],name(r),mode(r),particles(r),val(r),r.get('workloadVersion') or '未记录',flags(x)]
    lines.append('| '+' | '.join(str(c).replace('|','/').replace('\n',' ') for c in cells)+' |')
(dest/'summary.md').write_text('\n'.join(lines)+'\n')
ui=[]
for x in ordered:
    r=x['result'];ui.append({'id':r['id'],'time':r['timestamp'],'source':x['source'],'role':x['role'],'device':r['deviceName'],'api':r['graphicsApi'],'workload':name(r),'mode':mode(r),'particles':particles(r),'score':r['score'],'unit':r['scoreUnit'],'version':r.get('workloadVersion') or '未记录','note':flags(x),'width':r.get('resWidth',0),'height':r.get('resHeight',0),'difficulty':r.get('difficulty',''),'precision':r.get('precision',''),'config':r.get('workloadConfig') or '未记录'})
data=json.dumps(ui,ensure_ascii=False).replace('<','\\u003c').replace('>','\\u003e').replace('&','\\u0026')
page='''<!doctype html><html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>所有测试成绩</title><style>body{font:15px system-ui;margin:0;color:#172335;background:#f5f7fa}header{padding:28px 32px;background:white;border-bottom:1px solid #d9e1ed}h1{margin:0 0 14px;font-size:28px}.note{max-width:1100px;color:#51617b;line-height:1.6}.controls{padding:18px 32px;display:flex;gap:12px;flex-wrap:wrap;align-items:center}input,select{font:inherit;padding:8px;border:1px solid #bbc9da;border-radius:6px}input{min-width:240px}.table{margin:0 32px 32px;overflow:auto;background:white;border:1px solid #d9e1ed}table{border-collapse:collapse;min-width:1650px;width:100%}th,td{padding:10px 12px;text-align:left;border-bottom:1px solid #e5eaf1;vertical-align:top}th{position:sticky;top:0;background:#eaf0f8}tr:hover{background:#f1f6ff}.score{font-variant-numeric:tabular-nums;white-space:nowrap}.small{font-size:12px;color:#667894;max-width:260px;overflow-wrap:anywhere}details{max-width:340px}summary{cursor:pointer;color:#355a92}.source{white-space:nowrap}.badge{display:inline-block;padding:4px 9px;margin-right:8px;border-radius:6px;background:#eaf0f8}button{font:inherit;border:1px solid #bbc9da;background:white;padding:8px;border-radius:6px}</style><header><h1>所有 result 成绩</h1><div><span class="badge">367 条不同 ID</span><span class="badge">主历史 353 条</span><span class="badge">隔离验证 14 条</span></div><p class="note">单线程的 14 个编号是系统调度的采样序号，没有绑定不同核心，也没有得到大核／小核分别的成绩。默认不列入隔离验证短测；切换来源即可查看全部。不同单位、版本、渲染模式和粒子档位需分别查看，本表不生成跨测试排名。</p></header><div class="controls"><input id="search" placeholder="搜索设备、版本、ID…"><select id="source"><option value="mainHistory">主历史（353 条）</option><option value="all">全部（367 条）</option><option value="main">当前 Mac（133 条）</option><option value="legacy">仓库旧成绩（220 条）</option><option value="validation">隔离验证（14 条）</option></select><select id="api"><option value="">所有 API</option></select><select id="workload"><option value="">所有项目</option></select><select id="mode"><option value="">所有模式</option></select><button id="reset">重置筛选</button><b id="count"></b></div><div class="table"><table><thead><tr><th>时间／ID</th><th>来源</th><th>设备</th><th>API</th><th>项目／模式</th><th>粒子数</th><th>成绩</th><th>测试版本／参数</th><th>说明</th></tr></thead><tbody id="rows"></tbody></table></div><script>const data=DATA;const $=s=>document.getElementById(s);for(const key of ['api','workload','mode'])for(const value of [...new Set(data.map(r=>r[key]))].sort()){const o=document.createElement('option');o.value=value;o.textContent=value;$(key).append(o)}function cell(tr,text,cls){const td=document.createElement('td');td.textContent=text;if(cls)td.className=cls;tr.append(td);return td}function render(){const q=$('search').value.trim().toLowerCase(),source=$('source').value;const filtered=data.filter(r=>(source==='all'||source==='mainHistory'&&r.role!=='validation'||source===r.role)&&['api','workload','mode'].every(k=>!$(k).value||r[k]===$(k).value)&&(!q||Object.values(r).join(' ').toLowerCase().includes(q)));$('count').textContent=filtered.length+' 条';const frag=document.createDocumentFragment();for(const r of filtered){const tr=document.createElement('tr');cell(tr,r.time+'\n'+r.id,'small');cell(tr,r.source,'source');cell(tr,r.device);cell(tr,r.api);cell(tr,r.workload+' · '+r.mode);cell(tr,r.particles);cell(tr,Number(r.score).toLocaleString('zh-CN',{maximumFractionDigits:3})+' '+r.unit,'score');const td=cell(tr,r.version,'small');const d=document.createElement('details'),s=document.createElement('summary'),p=document.createElement('p');s.textContent='查看原始参数';p.textContent=r.width+' × '+r.height+' · '+r.difficulty+' · '+r.precision+'\n'+r.config;d.append(s,p);td.append(d);cell(tr,r.note,'small');frag.append(tr)}$('rows').replaceChildren(frag)}for(const key of ['search','source','api','workload','mode'])$(key).addEventListener('input',render);$('reset').onclick=()=>{$('search').value='';$('source').value='mainHistory';for(const k of ['api','workload','mode'])$(k).value='';render()};render();</script></html>'''.replace('DATA',data)
(dest/'all-results.html').write_text(page)
assert json.loads((dest/'all-results.json').read_text())==rawrows
assert len(list(csv.DictReader((dest/'all-results.csv').open(encoding='utf-8-sig'))))==367
for s in source_meta:assert hashlib.sha256(Path(s['path']).read_bytes()).hexdigest()==s['sha256']
manifest={'sources':source_meta,'uniqueCount':367,'mainHistoryCount':353,'isolatedValidationCount':14,'sourceFilesUnchanged':True,'snapshotsExcluded':snapshot_audit,'byApi':dict(apis),'byWorkload':dict(workloads),'cpuSingleCoreInterpretation':'14 scheduler-managed samples; not verified P/E or per-physical-core measurements','summaryPath':str(dest/'summary.md')}
(dest/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
print(json.dumps({'output':str(dest),'allCount':367,'mainCount':353,'validationCount':14,'snapshotsExcluded':len(snapshot_audit)},ensure_ascii=False))
