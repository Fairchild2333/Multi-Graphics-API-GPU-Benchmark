from pathlib import Path
import json,csv
root=Path(__file__).resolve().parent
retested={d:{r['id'] for r in json.loads((root/f'{d}-retest-results.json').read_text())} for d in ['ipad','iphone']}
lines=['# Metal 与 Vulkan：iPad A12X / iPhone A15','Vulkan 在两台设备上均使用 MoltenVK 转到 Metal。这里只整理已保存成绩，不额外运行测试。新版复测与原有未异常项目按来源注明；跨 API 计时方法可能不同，以下是本轮观察，不能当作纯转译开销。']
allrows=[]
for dev,label in [('ipad','iPad Pro 第三代 / A12X'),('iphone','iPhone 13 Pro / A15')]:
 rows=json.loads((root/f'{dev}-curated-suite-results.json').read_text())
 def conf(r):return dict(p.split('=',1) for p in r.get('workloadConfig','').split(';') if '=' in p)
 def key(r):return (r['workload'],r.get('particleCount'),r.get('headless'),conf(r).get('steps'))
 apis={api:{key(r):r for r in rows if r['graphicsApi']==api and r['workload'] in ['stream','gpu_burn','membw']} for api in ['Metal','Vulkan']}
 lines+=['## '+label,'| 项目 | Metal 分数 | Vulkan 分数 | Metal FPS | Vulkan FPS | 来源 Metal / Vulkan |','|---|---:|---:|---:|---:|---|']
 for k,m in apis['Metal'].items():
  v=apis['Vulkan'][k]
  if m['workload']=='stream': title=f"粒子 {m['particleCount']:,} · {'纯计算' if m['headless'] else '绘制'}"
  elif m['workload']=='gpu_burn':title=f"GPU Burn {conf(m)['steps']}步"
  else:title='内存扫描（方法不同）'
  source=lambda r:'复测' if r['id'] in retested[dev] else '原有'
  fps=lambda r:1000/r['avgFrameTimeMs'] if r.get('avgFrameTimeMs',0)>0 else None
  fmt=lambda v:f'{v:.2f}' if v is not None else '—'
  lines.append(f"| {title} | {m['score']:.3f} {m['scoreUnit']} | {v['score']:.3f} {v['scoreUnit']} | {fmt(fps(m))} | {fmt(fps(v))} | {source(m)} / {source(v)} |")
  allrows.append(dict(device=dev,test=title,unit=m['scoreUnit'],metal=m['score'],vulkan=v['score'],metalFPS=fps(m),vulkanFPS=fps(v),metalSource=source(m),vulkanSource=source(v),metalTiming=conf(m).get('timingMode','dedicated_sweep'),vulkanTiming=conf(v).get('timingMode','dedicated_sweep'),metalID=m['id'],vulkanID=v['id']))
 lines+=['本机流体仅有 Metal；Vulkan 缺少所需 R32_SFLOAT 线性过滤，不能配对比较。']
lines+=['## 如何解读','- 两台GPU Burn256均完成复测：Metal分数高约71%（iPad）/64%（iPhone）；实际FPS也更高。16/64步的Vulkan沿用旧版连续提交结果，不能把它们当作同轮单帧复测。','- 大工作集纯计算：iPad16M Metal29.502 /Vulkan28.935 GB/s，相差约2%；iPhone16M18.983 /19.027，接近。iPad Vulkan用同步墙钟，Metal用GPU时间，提交/等待开销会使小档差距更明显。','- 绘制粒子应看FPS，分数GB/s只计算计算阶段。iPhone4M/16M Vulkan FPS略高，但来自原有两帧提交；Metal来自新版串行复测。配置和温度条件不同，不能据此推出严格API效率比。','- 两种内存诊断使用不同网格/负载/统计策略，数字可并列观察，但不得相除当作转译损耗。Metal memoryPlateauVerified=0，不能称已验证DRAM峰值。','- 手机部分结果有降频或未达稳定平台，详见复测报告的温度来源。原始异常分数未导入此表，全部旧历史仍保留。']
report=lines[0]
for prev,line in zip(lines,lines[1:]): report+= ('\n' if prev.startswith('|') and line.startswith('|') else '\n\n')+line
(root/'metal-vulkan-comparison.md').write_text(report+'\n')
(root/'metal-vulkan-comparison.json').write_text(json.dumps(allrows,ensure_ascii=False,indent=2)+'\n')
with (root/'metal-vulkan-comparison.csv').open('w',newline='') as f:
 w=csv.DictWriter(f,fieldnames=list(allrows[0]));w.writeheader();w.writerows(allrows)
print('Compared 12 paired tests per device; raw results unchanged.')
