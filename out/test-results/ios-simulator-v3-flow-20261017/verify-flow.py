"""Verify saved simulator flow evidence with production-model audit results."""
from pathlib import Path
from datetime import datetime
import hashlib,json,subprocess
root=Path(__file__).resolve().parent
repo=root.parents[2]
sha=lambda p: hashlib.sha256(Path(p).read_bytes()).hexdigest()
load=lambda p: json.loads(Path(p).read_text())
report=load(root/'final-documents/suite-runs/latest.json')
rows=load(root/'final-documents/results/results.json')
audit=load(root/'production-contract-audit.json')
exit_status=load(root/'ui-test-exit.json')
xc=load(root/'xcresult-summary.json')
assert exit_status['returncode']==0 and xc['result']=='Passed' and xc['passedTests']==xc['totalTestCount']==1 and xc['failedTests']==0
jobs=report['jobs']
assert len(jobs)==len(audit)==27 and len(rows)==len({r['id'] for r in rows})==28
assert report['status']=='review' and report.get('resumedFrom') is None and report['finishedAt']
assert all(j['status'] in ['passed','review'] for j in jobs)
assert sum(len(j['resultIDs']) for j in jobs)==28
assert all(a['contractPassed'] for a in audit)
assert [a['job'] for a in audit]==[j['id'] for j in jobs]
assert all(a['status']==j['status'] for a,j in zip(audit,jobs))
m=next(j for j in jobs if j['id']=='vulkan/memory_sweep')
assert m['status']=='review'
mem=next(r for r in rows if r['id']==m['resultIDs'][0])
assert mem['workloadVersion'].startswith('membw_v3_streaming_ios_preview_moltenvk')
config={}
for field in mem['workloadConfig'].split(';'):
 key,sep,value=field.partition('=')
 if sep and key not in config: config[key]=value
assert config['maxDatasetMiB']=='256' and config['memoryPlateauReached']=='0' and config['dataVerified']=='1'
assert config['writeCopyChecks']=='34' and config['largeDatasetTraversal']=='ordered_full_dataset_passes'
for row in rows:
 if row['graphicsApi']=='Vulkan' and row['workload'] in ['stream','gpu_burn']:
  assert 'timingMode=synchronized_wall_clock' in row['workloadConfig']
log=(root/'ui-test.log').read_text()
assert 'ONE_BUTTON_READY label=一键测试全部项目' in log and 'ONE_BUTTON_TAPPED' in log
screenshots=load(root/'cpu-screenshots.json')
assert {s['job'] for s in screenshots}=={'cpu_compute','cpu_memory'}
for shot in screenshots:
 assert shot['returnCode']==0 and shot['runningAfterScreenshot']==[shot['job']]
 b=(root/(shot['job']+'-progress.png')).read_bytes()
 assert b[:8]==b'\x89PNG\r\n\x1a\n'
 assert int.from_bytes(b[16:20],'big')<int.from_bytes(b[20:24],'big')
before=load(root/'hardware-histories-before.json')
after=[]
for h in before:
 p=repo/h['path']; b=p.read_bytes()
 after.append(dict(h,sha256=hashlib.sha256(b).hexdigest(),rows=len(json.loads(b))))
assert before==after
(root/'hardware-histories-after.json').write_text(json.dumps(after,indent=2)+'\n')
live=Path('/Users/fairchild/Library/Application Support/GpuComputeBenchmark/results/results.json')
assert sha(live)==after[0]['sha256']
app=repo/'build-ios/DerivedData-iphonesimulator/Build/Products/Release-iphonesimulator/Mangekyo.app'
provenance={'commit':subprocess.check_output(['git','rev-parse','HEAD'],text=True,cwd=repo).strip(),'appSha256':sha(app/'Mangekyo'),'nativeLibrarySha256':sha(repo/'build-ios/Release-iphonesimulator/Release-iphonesimulator/libgpu_engine.a'),'readSpirvSha256':sha(app/'membw_read.comp.spv'),'uiTestSourceSha256':sha(repo/'ios-gui/MangekyoUITests/CompleteSuiteUITests.swift'),'appArchitecture':subprocess.check_output(['lipo','-archs',str(app/'Mangekyo')],text=True).strip(),'sourceDiff':subprocess.check_output(['git','diff','--','src','shaders','ios-gui'],text=True,cwd=repo)}
assert provenance['sourceDiff']==''
(root/'package-provenance.json').write_text(json.dumps(provenance,indent=2)+'\n')
passed=sum(j['status']=='passed' for j in jobs)
review=len(jobs)-passed
curves={name:sum(a[name] for a in audit) for name in ['cpuCurvePoints','metalMemoryPoints','vulkanMemoryPoints']}
assert curves['cpuCurvePoints']>0 and curves['metalMemoryPoints']>0 and curves['vulkanMemoryPoints']==77
assert any(a['hasPageMapping'] and a['hasStorage'] for a in audit)
verification={'status':'passed','scope':'SIMULATOR FUNCTIONAL FLOW ONLY; not physical-device performance acceptance','jobs':27,'records':28,'passedJobs':passed,'reviewJobs':review,'failedJobs':0,'expectedSmallMemoryPreview':1,'qualityReviewPreserved':True,'freshRealButtonRun':True,'savedCurves':curves,'fiveHardwareHistoriesUnchanged':True,'liveMacHistoryUnchanged':True,'sourceImplementationUnchanged':True,'xctestPassed':1}
(root/'verification.json').write_text(json.dumps(verification,indent=2)+'\n')
text=f'''# 最新 iOS 模拟器一键全套复核 · 2026-10-17

**功能流程通过：27/27 项执行完成，28 条唯一成绩，{passed} 项 passed、{review} 项 review、0 项 failed/cancelled。**
这是 iPhone 18 Pro Max / iOS 27.0 arm64 模拟器的流程验证，不是手机真机性能成绩。
本轮以 `35ea4eb` 的最新版 Release 构建运行，没有修改负载、计时、门槛或生产代码。

## 实际验证

- 新建独立模拟器；XCTest 只开启已有 Vulkan 转译开关，再点击真实“一键测试全部项目”。
- 中文按钮显示正常；没有使用自动运行、定向项目或断点续跑参数。
- 原生清单 `{report['id']}`，时间为 {report['startedAt']} 至 {report['finishedAt']}（原始 UTC，悉尼日期为 10 月 17 日）。
- XCTest 1/1 Passed，xcodebuild 退出 0；保存完整 native 日志和结果清单，导出点击前、完成后的截图。
- 当前生产模型逐项验证请求、版本、成绩组、正数分数、唯一 ID、结果数量及质量标记，27 项均匹配。
- CPU 算力与缓存/内存进度页为正常竖屏，截图留档；没有重现横屏黑屏或队列中断。
- 从 results.json 还原 CPU **{curves['cpuCurvePoints']} 点**、Metal 内存 **{curves['metalMemoryPoints']} 点**、Vulkan 内存 **{curves['vulkanMemoryPoints']} 点**；页面首次访问与存储明细均存在。
- Vulkan 粒子和 Burn 均保存 `synchronized_wall_clock` 计时标记。

## V3 小容量边界

本环境 Vulkan 单缓冲上限为 256 MiB。新记录使用
`membw_v3_streaming_ios_preview_moltenvk`，完整遍历标记存在，34 次数据校验通过、
77 点保存完整，但尾部 128/192/256 MiB 的方法混合，`memoryPlateauReached=0`。
App 正确标记“尚未达到内存带宽平台”并继续 CPU/GLES2 项目，没有误当正式通过。

其余 {review-1} 项 review 为稳定性/吞吐变化提示，原样保留；模拟器流程验收允许这些
质量提示，不把它们变成真机通过或设备降频结论，也没有为它们反复挑分。
能力清单仍为 27 项：不支持的 Vulkan 16M、流体、独立缓存诊断、GLES2 256 档未强行运行。
CPU 遍历宿主逻辑处理器，不能据此验证手机大核/小核归属。

## 数据边界与资料

- 五台 Apple 设备的主历史与本机实时 results.json 哈希均未变化；Mac 仍为 169 条。
- 模拟器成绩单独保存在本目录，未混入五台真实设备的成绩索引。
- A16 未完成项目、iPad8,1 Metal 16M 和各手机/iPad 的 V3 内存真机补测仍按主文档执行。
- `final-documents/`：完整成绩、原生计划、逐项状态、执行日志。
- `production-contract-audit.json` / `.log`：实际生产模型的匹配及曲线恢复检查。
- `ui-test.log`、`xcresult-summary.json`、`attachments/`、过程 PNG：一键实际执行及画面证据。
- `environment.json`、`test-command.json`、`package-provenance.json`、`verification.json`、前后历史校验：范围与可追溯信息。
- 完整 `ui-full.xcresult` 及编译产物继续留在本地；可读日志、原始 JSON 与截图入库。

## 逐项状态

| 项目 | 状态 | 提示 |
| --- | --- | --- |
'''
for j in jobs:
 error=j.get('error','').replace('|','\\|').replace('\n',' ')
 text+=f"| {j['id']} | {j['status']} | {error or '—'} |\n"
(root/'report.md').write_text(text)
print(json.dumps(verification,ensure_ascii=False,indent=2))
