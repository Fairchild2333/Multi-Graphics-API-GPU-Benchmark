from pathlib import Path
import json,datetime,re,csv,statistics,hashlib,subprocess
report=Path(__file__).resolve().parent
manifest=json.loads((report/'run-manifest.json').read_text())
source=Path(manifest['resultsFile'])
all_rows=json.loads(source.read_text())
raw=[r for r in all_rows if r['id'] not in manifest['baselineIds']]
initial=json.loads((report/'new-results.json').read_text())
fixed=json.loads((report/'fixed-viewports-status.json').read_text())
control=json.loads((report/'metal-control-status.json').read_text())
assert len(fixed)==len(control)==8 and all(s['passed'] for s in fixed+control)
ids={s['resultIds'][0] for s in fixed+control}
valid=[r for r in initial if r['headless'] or r['graphicsApi'] in ['OpenGL','CPU']]+[r for r in raw if r['id'] in ids]
assert len(valid)==32
assert len([r for r in valid if r['graphicsApi']=='Vulkan'])==13
assert len([r for r in valid if r['graphicsApi']=='Metal'])==13
by_id={r['id']:r for r in valid}
def key(r):
 if r['workload']=='stream': return ('stream',r['headless'],r['particleCount'])
 if r['workload']=='gpu_burn': return ('gpu_burn',False,int(re.search(r'steps=(\d+);',r['workloadConfig']).group(1)))
 return (r['workload'],False,0)
def label(k):
 if k[0]=='stream':return '粒子 '+{65536:'65K',1048576:'1M',4194304:'4M',16777216:'16M'}[k[2]]+(' · 纯计算' if k[1] else ' · 有画面')
 if k[0]=='gpu_burn':return f'GPU Burn {k[2]} 步'
 return '流体 · 互动水池'
metal={key(r):r for r in valid if r['graphicsApi']=='Metal' and r['workload']!='membw'}
vulkan={key(r):r for r in valid if r['graphicsApi']=='Vulkan' and r['workload']!='membw'}
keys=[('stream',h,n) for h in [False,True] for n in [65536,1048576,4194304,16777216]]+[('gpu_burn',False,n) for n in [16,64,256]]
comparisons=[]
for k in keys:
 m=metal[k];v=vulkan[k]
 assert m['scoreUnit']==v['scoreUnit'] and m['score']>0 and v['score']>0
 if not k[1]:
  assert (m['resWidth'],m['resHeight'],v['resWidth'],v['resHeight'])==(1280,720,1280,720)
  assert 'renderDoc=disabled' in m['workloadConfig'] and 'renderDoc=disabled' in v['workloadConfig']
 comparisons.append({'test':label(k),'Metal':m['score'],'Vulkan':v['score'],'unit':m['scoreUnit'],'VulkanRelativePct':(v['score']/m['score']-1)*100,'MetalFps':m['avgFps'],'VulkanFps':v['avgFps'],'MetalResultId':m['id'],'VulkanResultId':v['id']})
mem_m=next(r for r in valid if r['graphicsApi']=='Metal' and r['workload']=='membw')
mem_v=next(r for r in valid if r['graphicsApi']=='Vulkan' and r['workload']=='membw')
liquid_m=metal[('cinematic_liquid',False,0)];liquid_v=vulkan[('cinematic_liquid',False,0)]
summary={'completedUtc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'functionalSuitePassed':31,'functionalSuiteTotal':31,'correctedVulkanPassed':8,'metalNoCapturePassed':8,'rawNewRecords':len(raw),'selectedPerformanceRecords':len(valid),'excludedInitialRenderedResults':[r['id'] for r in initial if r['graphicsApi'] in ['Metal','Vulkan'] and not r['headless']],'nativeCaptureArtifacts':len(json.loads((report/'capture-artifacts.json').read_text())),'comparisons':comparisons,'memory':{'Metal':mem_m['score'],'Vulkan':mem_v['score'],'directlyComparable':False},'liquid':{'Metal':liquid_m['score'],'Vulkan':liquid_v['score'],'MetalFps':liquid_m['avgFps'],'VulkanFps':liquid_v['avgFps'],'sharedScoreContract':False}}
(report/'summary.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2)+'\n')
(report/'all-new-results.json').write_text(json.dumps(raw,ensure_ascii=False,indent=2)+'\n')
(report/'selected-results.json').write_text(json.dumps(valid,ensure_ascii=False,indent=2)+'\n')
with (report/'comparison.csv').open('w',newline='') as f:
 writer=csv.DictWriter(f,fieldnames=list(comparisons[0]));writer.writeheader();writer.writerows(comparisons)
lines=['# macOS 完整测试：Metal 与 Vulkan（MoltenVK）','', '测试日期：2026-10-13（Australia/Sydney）。设备：Apple M4 Pro，Mac16,8，48 GiB 统一内存，macOS 27.0.1，ARM64。测试时接通电源。','', 'GUI 完整流程 **31/31 成功**：Vulkan 13、Metal 13、OpenGL 3、CPU 2。产生 32 条成绩；CPU 算力会保存单核和多核两条记录。8 个原生 Metal 抓帧全部存在。','', '实测中发现 Vulkan 的 1280×720 窗口在 Retina 上实际生成 2560×1440 渲染目标。修正后重新验证 Vulkan 有画面 8/8 项；再完成 Metal 无抓帧对照 8/8 项。修正后的两条路径均为 1280×720，关闭垂直同步、关闭抓帧，每项运行 15 秒，排除约 2 秒预热。纯计算、显存、OpenGL、CPU 使用本轮完整流程的结果。','',f'原始新增成绩 {len(raw)} 条均保留在 results.json；本报告选择 {len(valid)} 条代表 31 个项目。旧的 Retina 尺寸不一致成绩和带抓帧的初轮 Metal 有画面成绩不进入下面的性能对比。修正后的 Vulkan 有画面成绩使用独立版本后缀 `_viewport1280x720_v1`。','', '## 同项目性能对比','', '粒子 GB/s 是按每粒子 40 字节和 GPU compute 时间推算的吞吐指标，不是硬件显存带宽。GPU Burn 的单位是 Gpix-step/s。表中的百分比为 Vulkan ÷ Metal − 1，正数表示本轮 Vulkan 得分更高。','', '| 项目 | Metal | Vulkan / MoltenVK | 单位 | Vulkan 相对 Metal |','|---|---:|---:|---|---:|']
for c in comparisons:lines.append(f'| {c["test"]} | {c["Metal"]:.3f} | {c["Vulkan"]:.3f} | {c["unit"]} | {c["VulkanRelativePct"]:+.1f}% |')
lines+=['','这些是本应用两条执行路径的单轮结果，差异包括着色器实现、编译优化、调度和呈现方式，不能直接把百分比解释成 MoltenVK 的纯转译成本。未做跨运行顺序或长期温控的统计验收。','', '## 显存 / 统一内存扫描','', f'- Vulkan 读带宽：**{mem_v["score"]:.3f} GB/s**；扫描到 2 GiB，达到内存平台，GPU timestamp，10 次测量取中位数，timestamp anomalies 为 0。', f'- Metal 原生预览读带宽：**{mem_m["score"]:.3f} GB/s**；粒子衍生固定网格扫描到 512 MiB，5 次取最好值，尚未验证内存平台。','- 这两项内核、扫描上限和统计方法不同，数值分别展示，不计算直接性能百分比。Apple 芯片的 CPU 和 GPU 使用同一套统一内存。','', '## 流体','', f'同为 1280×720、15 秒、MLS-MPM 场景：Metal **{liquid_m["score"]:.3f} MParticle-step/s**、平均 **{liquid_m["avgFps"]:.1f} FPS**；Vulkan **{liquid_v["score"]:.3f} MParticle-step/s**、平均 **{liquid_v["avgFps"]:.1f} FPS**。Metal 使用独立 `_metal_preview` 计时/呈现版本，保留分组，以上作为两条实现路径的观察值。','', '## OpenGL 与 CPU','']
for r in valid:
 if r['graphicsApi']=='OpenGL':lines.append(f'- OpenGL {label(key(r))}：{r["score"]:.3f} {r["scoreUnit"]}（OpenGL 4.1，同步墙钟计时版本）。')
 if r['graphicsApi']=='CPU':lines.append(f'- {r["workload"]}：{r["score"]:.3f} {r["scoreUnit"]}。')
lines+=['','## 文件与验证','', f'- 共用成绩文件：`{source}`。',f'- 本轮证据目录：`{report}`，包含前后成绩、流程记录、渲染尺寸探针、8 项 Vulkan 复测日志、8 项 Metal 对照日志、逐项结果 ID、CSV 对比与选择后的 JSON。','- 修正后的 Apple Silicon / Intel macOS 构建通过；Intel 为交叉构建。CTest 3/3 通过。','- 本次没有重新运行 iOS 性能测试。','']
markdown='\n'.join(lines)
(report/'comparison.md').write_text(markdown)
shared=Path('/Users/fairchild/Library/Application Support/GpuComputeBenchmark/reports')
shared.mkdir(parents=True,exist_ok=True)
(shared/'metal-vulkan-complete-2026-10-13.md').write_text(markdown)
manifest['completedUtc']=summary['completedUtc'];manifest['correctedHelperSha256']=hashlib.sha256(Path('out/macos/arm64/Release/Mangekyo.app/Contents/Helpers/gpu_benchmark').read_bytes()).hexdigest();manifest['summary']=summary
(report/'run-manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
print(json.dumps(summary,ensure_ascii=False,indent=2))
print('report:',shared/'metal-vulkan-complete-2026-10-13.md')
