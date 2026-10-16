from pathlib import Path
import json
root=Path(__file__).resolve().parent
results={d:json.loads((root/f'{d}-retest-results.json').read_text()) for d in ['ipad','iphone']}
byjob={d:{r['job']:r for r in rows} for d,rows in results.items()}
old={}
for device,archive in [('ipad','ipadpro3-20261014'),('iphone','ios-api-completion-20261014')]:
    p=root.parent/archive
    m=json.loads((p/'final-phone/suite-runs/latest.json').read_text())
    rows={r['id']:r for r in json.loads((p/'suite-results.json').read_text())}
    old[device]={j['id']:rows[j['resultIDs'][0]] for j in m['jobs']}
verification=json.loads((root/'verification.json').read_text())
text=['# iPad A12X / iPhone A15 异常项定向复测',
'完成 iPad 11 项、iPhone 7 项；iPhone 首轮后四项出现较高温度（状态2），降温后又补测这四项。共18项设备测试、22条新记录。CPU、内存扫描、GPU缓存状态诊断、ES2和其余原有项目均未重测。',
'新版只改异常项涉及的计时：嵌入式 Vulkan 纯计算也校验独立同步耗时与连续提交的查询时间；查询严重偏小时切换同步墙钟。iOS Metal 绘制逐帧串行提交并等待计算、绘制完成，使用独立命令缓冲的GPU时间，标记 serial_timing_v2。MoltenVK GPU Burn 使用单帧提交，并在启动时以同步提交/等待校验渲染查询，标记 single_frame_timing_v2。计时方法改变的成绩不能与旧版混排成同一组。',
'\n## iPad 三条明确异常的更正',
'| Vulkan 纯计算 | 旧分数 GB/s（排除） | 复测 GB/s | Metal 原有同档 GB/s |',
'|---|---:|---:|---:|']
for n in [1048576,4194304,16777216]:
    job=f'vulkan/stream_headless-{n}'
    text.append(f"| {n:,} 粒子 | {old['ipad'][job]['score']:.3f} | {byjob['ipad'][job]['score']:.3f} | {old['ipad'][f'stream_headless-{n}']['score']:.3f} |")
text += ['三条均实际触发同步计时回退。16M校验中独立GPU查询24.1 ms、独立墙钟24.2 ms，但连续提交查询约0 ms，表明原连续提交的查询没有覆盖真实工作。复测16M计算23.193 ms、帧间隔23.212 ms，分数28.935 GB/s，数量级矛盾已消除。1M/4M亦与同步帧间隔吻合。墙钟包含提交/等待开销，不能单凭与Metal原生GPU时间的差值计算纯转译成本。',
'\n## Metal / Vulkan 重档实测',
'| 设备 | Metal 256步 Gpix-step/s | Vulkan 256步 Gpix-step/s | Metal 实际FPS | Vulkan 实际FPS |',
'|---|---:|---:|---:|---:|']
for d,label in [('ipad','iPad Pro A12X'),('iphone','iPhone 13 Pro A15')]:
    a=byjob[d]['gpu_burn-256'];b=byjob[d]['vulkan/gpu_burn-256']
    text.append(f"| {label} | {a['score']:.3f} | {b['score']:.3f} | {1000/a['avgFrameTimeMs']:.2f} | {1000/b['avgFrameTimeMs']:.2f} |")
text += ['两机本轮重档均为Metal更快，帧率也与分数方向一致。iPad Vulkan渲染启动校验535.25 ms GPU /533.11 ms同步提交等待；降温后的iPhone为424.1/441.3 ms。iPhone先前2.098分数与1.7 FPS的矛盾未在单帧复测中重现；现为0.967分数、约1.69 FPS，不能沿用旧版“Vulkan快2.6倍”的说法。',
'\n## 各项前后与计时核验',
'所有选用的新结果均符合：计算+绘制不超过总跨度（仅允许0.1ms存档舍入）；总跨度/墙钟帧间隔≤1.05；无>100%的利用率异常；Metal各阶段不再重叠。Vulkan纯计算的分数与帧间隔吞吐差在10%以内。GPU Burn两机固定1280×720、256步、两次绘制。单个测试15秒、原有预热和工作量保持一致。',
'| 设备 | 测试 | 旧分数 | 复测分数 | 单位 | GPU总跨度 / 帧间隔 ms |',
'|---|---|---:|---:|---|---|']
for d,label in [('ipad','iPad'),('iphone','iPhone')]:
    for r in results[d]:
        previous=old[d][r['job']]
        text.append(f"| {label} | {r['job']} | {previous['score']:.3f} | {r['score']:.3f} | {r['scoreUnit']} | {r['avgTotalGpuMs']:.3f} / {r['avgFrameTimeMs']:.3f} |")
text += ['粒子绘制的GB/s仅表示计算阶段吞吐，不能当作渲染FPS。例如16M时iPad计算分数27.198高于iPhone15.884，但整帧分别约1.99与4.29 FPS。流体新版分数为iPad6.903、iPhone12.931 MParticle-step/s；计算+绘制分别464.875、248.186 ms，总跨度465.732、249.198 ms，原重复计时问题已消除。',
'\n## 温度与结果保存',
'选用的18项均在系统报告正常温度（0）时开始；iPad全部正常温度结束，iPhone16步和Vulkan重档结束为较热（1），其余正常。iPhone粒子首轮仍有吞吐漂移，Vulkan重档未报告稳定平台；这些是本轮15秒观察，不能当作恒温、冷机峰值或长期持续性能。没有将热机分数平均进降温补测的分数。',
'原iPad32条、iPhone56条历史均逐字段保留。新增后分别43条、67条。旧的异常数字保留用于追溯；新的 curated-suite-results.json 将各32条完整清单中的指定异常项替换为新记录，其余选择原样沿用。此清单是分次测量汇总，不能视为一次不间断等温测试。',
'- `ipad-final/results/results.json`、`iphone-final/results/results.json`：设备完整原始历史副本，设备本身也已保存到 Documents/results/results.json。\n- `ipad-retest-results.json`、`iphone-retest-results.json`：本次选用的11/7条复测结果。\n- `ipad-curated-suite-results.json`、`iphone-curated-suite-results.json`：更新异常项后的各32条完整成绩清单。\n- `ipad-final/suite-runs`、`iphone-first-retest/suite-runs`、`iphone-final/suite-runs`：原生日志、任务与温度清单。\n- `verification.json`：追加保存、旧记录不变、计时间隔、角色与范围核验。\n- `compat-tests.log`：结果兼容与Vulkan内存序列化测试2/2通过；`swift-check`以生产模型验证两套更新成绩清单。',
'两台应用已回到普通界面。Mac结果和前次测试档案的原始JSON未改变。']
report=text[0]
for previous,part in zip(text,text[1:]):
    report+=('\n' if previous.startswith('|') and part.startswith('|') else '\n\n')+part
(root/'report.md').write_text(report+'\n')
comparison=[]
for job in sorted(set(byjob['ipad'])&set(byjob['iphone'])):
    a=byjob['ipad'][job];b=byjob['iphone'][job]
    assert a['workloadVersion']==b['workloadVersion']
    comparison.append({'job':job,'ipadScore':a['score'],'iphoneScore':b['score'],'unit':a['scoreUnit'],'ipadToIphone':a['score']/b['score'],'ipadFPS':1000/a['avgFrameTimeMs'],'iphoneFPS':1000/b['avgFrameTimeMs'],'version':a['workloadVersion'],'ipadID':a['id'],'iphoneID':b['id']})
(root/'retest-comparison.json').write_text(json.dumps(comparison,ensure_ascii=False,indent=2)+'\n')
print('Report and seven matched retest comparisons written.')
