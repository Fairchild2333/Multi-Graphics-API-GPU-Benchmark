"""Audit one unchanged CPU confirmation and the current iOS timed loops."""
from pathlib import Path
import json,re,hashlib,subprocess
root=Path(__file__).resolve().parent
repo=root.parents[2]
old=repo/'out/test-results/macos-targeted-retest-20261016'
def protocol(path,prefix):
 return [dict(x.split('=',1) for x in line.split('\t')[1:] if '=' in x) for line in path.read_text().splitlines() if line.startswith(prefix+'\t')]
newlog=root/'cpu-single-confirmation.log'
assert json.loads((root/'exit.json').read_text())['returnCode']==0
rows=json.loads((root/'data/results/results.json').read_text())
assert len(rows)==2 and all('_kept_branch_formal_r3_t15000ms_w200ms_sequence_standalone' in r['workloadVersion'] for r in rows)
new_res=protocol(newlog,'CPU_RESIDENCY'); old_res=protocol(old/'cpu-kept-branch.log','CPU_RESIDENCY')
new_core=[r for r in protocol(newlog,'CPU_RESULT') if r.get('kind')=='core']
old_core=[r for r in protocol(old/'cpu-kept-branch.log','CPU_RESULT') if r.get('kind')=='core']
items=[]
for kind in ['performance','efficiency']:
 n=next(r for r in new_core if r['core_type']==kind);o=next(r for r in old_core if r['core_type']==kind)
 nr=next(r for r in new_res if r['core_type']==kind and r['round']==n['median_round'])
 ore=next(r for r in old_res if r['core_type']==kind and r['round']==o['median_round'])
 rs=[r for r in new_res if r['core_type']==kind]
 assert len(rs)==3 and all(r['verified']=='1' and float(r['requested_lower_bound'])>=.99 and r['retries']=='0' for r in rs)
 item={'coreType':kind,'beforeScore':float(o['score']),'confirmationScore':float(n['score']),'scoreChangePct':(float(n['score'])/float(o['score'])-1)*100,'beforeEffectiveGHz':float(ore['effective_ghz']),'confirmationEffectiveGHz':float(nr['effective_ghz']),'frequencyChangePct':(float(nr['effective_ghz'])/float(ore['effective_ghz'])-1)*100,'beforeCyclesPerWorkUnit':float(ore['effective_ghz'])*1000/float(o['score']),'confirmationCyclesPerWorkUnit':float(nr['effective_ghz'])*1000/float(n['score']),'minimumResidencyLowerBound':min(float(r['requested_lower_bound']) for r in rs),'workerBusyFractionSelectedRound':float(nr['worker_cpu_seconds'])/float(n['seconds']),'policy':'user_interactive' if kind=='performance' else 'background_not_peak'}
 items.append(item)
lines=(root/'ios-cpu-objdump.txt').read_text().splitlines()
insns=[]
for line in lines:
 m=re.match(r'\s*([0-9a-f]+):\s+(.+)',line)
 if m:insns.append((int(m.group(1),16),m.group(2)))
by_addr={a:i for i,(a,_) in enumerate(insns)}
loops=[]
for i,(addr,op) in enumerate(insns):
 if not ('eor' in op and 'ror #47' in op):continue
 end=next(j for j in range(i+1,min(i+90,len(insns))) if re.match(r'b\.ne\s+0x[0-9a-f]+$',insns[j][1]))
 start=by_addr[int(insns[end][1].split('0x')[1],16)]
 backward=[int(m.group(1),16) for _,x in insns[start:end+1] if (m:=re.match(r'(?:tbz|tbnz)\s+.+,\s+0x([0-9a-f]+)$',x))]
 if backward:start=min(start,*(by_addr[a] for a in backward if a in by_addr))
 body=insns[start:end+1]
 fp=any(re.match(r'fmadd\s+s',x) for _,x in body) and any(re.match(r'fmadd\s+d',x) for _,x in body)
 branch=any(re.match(r'(?:tbz|tbnz)\s+\w+,\s+#0x0,',x) for _,x in body)
 loops.append({'anchorAddress':hex(addr),'startAddress':hex(body[0][0]),'endAddress':hex(body[-1][0]),'role':'timed_mixed_kernel' if fp else 'warmup_integer_only_dce','hasFP32AndFP64FMA':fp,'hasBit0Branch':branch,'conditionalSelectCount':sum(bool(re.match(r'(?:fcsel|csel)\s',x)) for _,x in body),'assembly':[f'{a:x}: {x}' for a,x in body]})
measured=[x for x in loops if x['role']=='timed_mixed_kernel']
assert len(loops)==5 and len(measured)==3
assert all(x['hasBit0Branch'] and x['conditionalSelectCount']==0 for x in measured)
(root/'ios-timed-loops.txt').write_text('\n\n'.join('\n'.join([x['role']+' '+x['anchorAddress']]+x['assembly']) for x in loops)+'\n')
probe=repo/'out/test-results/macos-measurement-fixes-20261016/final/thermal-probe'
thermal=json.loads(subprocess.check_output([str(probe)],text=True));assert thermal['thermalState']==0
result={'status':'passed','cpuMeasurements':items,'iosAssembly':{'loops':loops,'timedLoopsWithRealBranch':3,'timedLoopsWithConditionalSelect':0,'warmupOnlyLoops':2,'scope':'Current compiled iOS arm64 native object; no physical iPhone/iPad rerun'},'thermalAfter':thermal,'productionSourceChanged':False,'peakSingleCorePerformanceEstablished':False,'genericCpuRankingEstablished':False}
(root/'audit.json').write_text(json.dumps(result,indent=2)+'\n')
p=items[0]
(root/'report.md').write_text(f'''# M4 Pro 单核低分核对 · 2026-10-17

原大核207.532是当次约4.03GHz下的有效结果，不应写成M4 Pro单核最高能力。
用户提出低分疑问后，以同一个已打包Release worker、原内核、原15秒/0.2秒预热/
三轮中位数，只执行一次标准per-core流程；默认同时保存大核和后台小核。
没有为追高修改选项、门槛或重复挑分，没有重跑全核或其他稳定项目。

| 项目 | 原记录 | 本次确认 | 说明 |
| --- | ---: | ---: | --- |
| 大核单线程 MWork/s | 207.532 | 216.737 | +{p['scoreChangePct']:.2f}% |
| 选中轮有效频率 GHz | {p['beforeEffectiveGHz']:.4f} | {p['confirmationEffectiveGHz']:.4f} | +{p['frequencyChangePct']:.2f}% |
| 每工作单元周期数 | {p['beforeCyclesPerWorkUnit']:.3f} | {p['confirmationCyclesPerWorkUnit']:.3f} | 基本不变 |
| 后台小核 MWork/s | 69.902 | 76.322 | 后台策略，不作正常频率峰值 |

大核三轮驻留下界均≥99.969%，选中轮工作线程占用约{p['workerBusyFractionSelectedRound']*100:.2f}%；
热状态0→0。没有发现跑错小核、时间单位或工作量漏算的证据。主要分差与观测
频率变化接近，但不能把这一关系推广成已确认全部波动来源，也没有证明达到最高频率。

## 与旧分数及其他设备的关系

旧Mac约312/3582版本被编译为条件选择，缺少测试要求的实际数据依赖分支。
现有`_kept_branch`只强制保留原分支，空内联汇编不增加额外算术工作。
当前iOS原生对象中，三个正式混合测量循环也保留bit0分支、FP32和FP64 FMA，
循环内无csel/fcsel；另两个整数预热循环被优化为条件选择，不计入正式工作量。
因此不能笼统用整个文件的条件选择数量推断两平台正式内核不一致。
本轮未接手机，当前iOS对象核对不冒充A15旧安装包或真机的额外复测。

这个MWork/s来自本项目的串行依赖整数/浮点/分支混合内核，不是覆盖各类负载的
通用CPU综合分。当前A15大核147.120与本次216.737相差约47.3%，只能表述为
此内核的观察；并未独立归因全部架构差异。A15正常性能小核与Mac后台小核不能排名。

本轮结论：保存最新216.737并保留原207.532，没有证据需要为了“看上去偏低”
继续改测法或反复挑分。单核峰值/通用CPU排名未建立。

## 证据

`command.json`、`environment.json`：原始命令、worker哈希与条件；
`cpu-single-confirmation.log`、`data/results/results.json`：全部执行与两条真实成绩；
`audit.json`、`ios-timed-loops.txt`、`ios-cpu-objdump.txt`：逐轮驻留/频率及原生循环核查。
''')
print(json.dumps(items,indent=2))
print('iOS: 3/3 timed mixed loops retain branch and FP32/FP64 arithmetic; no production source change')
