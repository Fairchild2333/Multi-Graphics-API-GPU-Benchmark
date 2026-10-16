from pathlib import Path
import json
root=Path(__file__).resolve().parent
sets=json.loads((root/'selected-apple-results.json').read_text())
def find(device,api,work,n=None,steps=None,headless=False):
 for r in sets[device]:
  c=dict(x.split('=',1) for x in r.get('workloadConfig','').split(';') if '=' in x)
  if r['graphicsApi']==api and r['workload']==work and r['headless']==headless and (n is None or r['particleCount']==n) and (steps is None or c.get('steps')==str(steps)):return r
 raise ValueError((device,api,work,n,steps))
items=[]
def add(device,title,api,work,n=None,steps=None,priority='P1',reason=''):
 r=find(device,api,work,n,steps)
 items.append(dict(device=device,test=title,priority=priority,reason=reason,resultID=r['id'],score=r['score'],unit=r['scoreUnit'],stableScore=r.get('stableScore'),stableCV=r.get('stableVariancePct'),throughputDropPct=r.get('throttlePct'),version=r['workloadVersion']))
for n in [1048576,4194304,16777216]:add('mac',f'Metal {n:,}粒子绘制','Metal','stream',n,priority='P0',reason='总命令跨度超过帧间隔；旧计时需串行/单帧核验')
for n in [16,64,256]:add('mac',f'Metal GPU Burn {n}步','Metal','gpu_burn',steps=n,priority='P0',reason='命令跨度/帧间隔约1.9–2.0；不能用旧分数作API排行')
add('mac','Metal流体','Metal','cinematic_liquid',priority='P0',reason='compute+render时间区间重叠；总分分母需要核验')
add('mac','Vulkan 1M粒子绘制','Vulkan','stream',1048576,reason='前后吞吐下降54.6%，未达稳定；下降原因需验证，不能直接归为温控')
add('iphone','Metal 4M粒子绘制','Metal','stream',4194304,reason='运行前后吞吐下降8.82%；系统温度0→0仍出现下降')
add('iphone','Metal GPU Burn16步','Metal','gpu_burn',steps=16,reason='吞吐下降9.73%；温度0→1；已有稳定段但平均值混入前后变化')
add('iphone','Vulkan 1M粒子绘制','Vulkan','stream',1048576,reason='吞吐下降5.32%；稳定段可参考，但冷机API比较应核对')
add('iphone','Metal 1M粒子绘制','Metal','stream',1048576,priority='P2',reason='沿用旧并行计时；均值12.886与稳定段19.325差异较大；属于吞吐上升/条件变化，不是已证实降频')
add('iphone','Metal 16M粒子绘制','Metal','stream',16777216,priority='P2',reason='尚未达稳定；前后吞吐上升8.71%，不是已证实降频')
add('iphone','Vulkan GPU Burn64步','Vulkan','gpu_burn',steps=64,priority='P2',reason='原15秒成绩未达稳定，下降2.23%；需稳定性补查，不能直接叫热降频')
add('iphone','Vulkan GPU Burn256步','Vulkan','gpu_burn',steps=256,priority='P2',reason='已降温复测，但仍未达稳定，温度0→1；0.967可作为当轮值，不是稳定平台')
add('ipad','Metal 1M粒子绘制','Metal','stream',1048576,reason='吞吐下降9.09%；系统温度0→0，下降不等于已证实温控')
add('ipad','Vulkan 1M粒子绘制','Vulkan','stream',1048576,reason='吞吐下降14.24%，未达稳定')
add('ipad','Vulkan 4M粒子绘制','Vulkan','stream',4194304,reason='吞吐下降6.87%；已有稳定段，冷机比较需核对')
add('ipad','Vulkan GPU Burn64步','Vulkan','gpu_burn',steps=64,priority='P2',reason='15秒未达稳定；下降1.57%，不能直接判为降频')
assert len(items)==19
(root/'retest-needed.json').write_text(json.dumps(items,ensure_ascii=False,indent=2)+'\n')
labels={'mac':'Mac M4 Pro','iphone':'iPhone 13 Pro','ipad':'iPad Pro A12X'}
lines=['# 待复测与稳定性补查：2026-10-14','本次仅总结清单，没有启动新测试。P0为计时口径需要修复/核验；P1为明显前后吞吐变化，优先独立冷启动复测；P2为稳定性或旧计时条件补查，现有当轮值不自动作废。共19项：Mac8、iPhone7、iPad4；其中P0/P1共14项，P2共5项。',
'**吞吐下降不是硬件频率遥测。**记录里的throttlePct是前5个和后5个窗口的分数差，温控、系统负载、驱动调度与绘制争用都可能影响。温度状态0也不能证明没有降频；stableScore=0仅表示未检测到稳定段（流体除外），不是失败或零分。流体有意不做稳定平台判断，因为场景负载随进程改变。',
'| 设备 | 项目 | 优先级 | 原因 | 当前均值 / 稳定段 |',
'|---|---|---|---|---|']
for q in items:
 stable=f"{q['stableScore']:.3f}" if q['stableScore'] else '未确认/不适用'
 lines.append(f"| {labels[q['device']]} | {q['test']} | {q['priority']} | {q['reason']} | {q['score']:.3f} / {stable} {q['unit']} |")
lines+=['## iPhone已经补好、不因本次降频再重复的项目','- Metal GPU Burn64：热机1.128→降温1.555 Gpix-step/s；温度0→0，稳定段1.555，CV0.18%。\n- Metal GPU Burn256：热机0.797→降温1.586；温度0→0，稳定段1.581，CV0.60%，前后仅下降0.49%。\n- Metal流体：热机8.232→降温12.931 MParticle-step/s；温度0→0、阶段不重叠。流体不检测稳定平台，stableScore=0不意味着要无限重复。\n- iPad Vulkan1M/4M/16M纯计算：异常时间戳已触发同步回退，22.618/27.317/28.935 GB/s，计时核对和稳定段均正常，无需因旧虚高再重复。',
'## 复测方法与停止条件','只运行列出的项目，逐项在系统报告正常温度后开始，保持15秒合同/分辨率/工作量一致，并留出降温间隔。先核对GPU时间与同步提交等待、排除重复区间；再看完整曲线和前后变化。必要时增加一次相同15秒冷启动复测，不把改变时长的结果混入原合同。持续变化若反复重现，则应报告真实变化/稳定段，不能只挑最高的一次。未达平台但计时合理的结果保留为当轮观察，不能无限重测追求高分。',
'CPU、独立内存扫描、已稳定的纯计算和ES2不因当前证据重跑。轻档65K的显示/提交限制影响不宜靠重复跑消除，不纳入优先硬件排行。旧Android/鸿蒙只有摘要且原设备未连接，已有标注继续保留，不能伪造复测。',
'来源：最新选用32条设备清单（selected-apple-results.json），原始stableScore/stableVariancePct/throttlePct字段，iPhone首次和降温补测原生日志；上述降频判定方法见src/app_base.cpp的recordWindowSample。']
text=lines[0]
for prev,line in zip(lines,lines[1:]):text+= ('\n' if prev.startswith('|') and line.startswith('|') else '\n\n')+line
(root/'retest-needed.md').write_text(text+'\n')
print('Checklist: Mac8 + iPhone7 + iPad4 =19; timing7, drift7, stability5. No new tests launched.')
