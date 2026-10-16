"""Summarize real iPad files, keeping review and failure states intact."""
import hashlib
import json
import math
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parent
documents = ROOT / (sys.argv[1] if len(sys.argv) > 1 else 'ui-watch-01/documents')
manifest = json.loads((documents / 'suite-runs/latest.json').read_text())
raw = (documents / 'results/results.json').read_bytes()
rows = json.loads(raw)
assert len({r['id'] for r in rows}) == len(rows)
by_id = {r['id']: r for r in rows}
before_file = ROOT / 'before/results/results.json'
before = json.loads(before_file.read_text()) if before_file.exists() else []
assert all(by_id.get(r['id']) == r for r in before)
selected = []
for job in manifest['jobs']:
    for identifier in job['resultIDs']:
        row = by_id[identifier]
        assert row['deviceModel'] == 'iPad8,1' and row['score'] > 0 and math.isfinite(row['score'])
        selected.append(dict(job=job['id'], **row))
counts = Counter(j['status'] for j in manifest['jobs'])
log = (ROOT / 'ui-click.log').read_text()
verification = dict(deviceModel='iPad8,1', manifestID=manifest['id'], manifestStatus=manifest['status'],
    jobCount=len(manifest['jobs']), statuses=dict(counts), historyRecords=len(rows),
    originalRecords=len(before), originalRecordsUnchanged=True, selectedRecords=len(selected),
    actualButtonTapped='ONE_BUTTON_TAPPED' in log, automaticChineseObserved='ONE_BUTTON_READY label=一键测试全部项目' in log,
    resultsSha256=hashlib.sha256(raw).hexdigest(), independentGpuCacheAttribution='unverified_out_of_scope_per_user')
(ROOT / 'selected-results.json').write_text(json.dumps(selected,ensure_ascii=False,indent=2)+'\n')
(ROOT / 'verification.json').write_text(json.dumps(verification,ensure_ascii=False,indent=2)+'\n')

def fields(row):
    return dict(p.split('=',1) for p in row.get('workloadConfig','').split(';') if '=' in p)

lines=['# 11 英寸 A12X iPad 真机一键测试（2026-10-16）','',
    '设备：Van 的 iPad，iPad Pro 11 英寸（iPad8,1），iPadOS 26.7.1。与此前 iPad8,7 是两台不同的 A12X 设备。','',
    f"实际按钮自动点击：{'已确认' if verification['actualButtonTapped'] else '尚未确认'}。本轮 {len(manifest['jobs'])} 项："+
    '，'.join(f'{k} {v}' for k,v in sorted(counts.items()))+'。保留原始状态，没有把稳定性提示改为通过。','',
    'Auto 已改为读取系统首选语言，并在安装包声明英文、简体中文与日文；恢复启动时保存的语言选择。'+
    '界面测试实际读到中文按钮“一键测试全部项目”，随后点击；没有使用自动开始全套或定向测试的启动参数。','',
    f"完整历史 {len(rows)} 条，本轮按钮清单关联 {len(selected)} 条。启动前没有已安装的测试 App，原历史为 0；此前 A15 和另一台 A12X 的档案没有导入此设备。",'',
    '## 全套项目与成绩','',
    '| 项目 | 状态 | 成绩 | 提示 |','| --- | --- | --- | --- |']
for job in manifest['jobs']:
    scores=[]
    for identifier in job['resultIDs']:
        row=by_id[identifier]
        f=fields(row)
        label=f.get('coreType','') if row['graphicsApi']=='CPU' else ''
        scores.append(f"{label} {row['score']:.3f} {row['scoreUnit']}".strip())
    lines.append('| '+job['id']+' | '+job['status']+' | '+' / '.join(scores)+' | '+job['error'].replace('|','/').replace('\n',' ')+' |')
lines+=['','## 测量口径','',
    '- Vulkan 通过 MoltenVK 转译到 Metal，保留转译标签，与原生 Metal 分开分组。',
    '- 粒子 GB/s 是既定工作量的吞吐指数，不直接代表物理显存或缓存总线带宽。',
    '- GPU 缓存诊断保留工作集曲线与未归属标记，不声称独立测出了 L2／L3／SoC-SLC。',
    '- A12X 小核算力默认 Utility 无辅助线程，CPU 小核缓存／内存沿用后台策略；后者不当作正常频率峰值。',
    '- 热状态正常不代表硬件频率恒定；review 的成绩保留，不按最高分挑选。','',
    '## 归档','',
    '- [完整设备 Documents](ui-watch-01/documents/)',
    '- [全部原始成绩](ui-watch-01/documents/results/results.json)',
    '- [本轮关联成绩](selected-results.json)',
    '- [核对记录](verification.json)',
    '- [生产模型合同核对](production-contract-audit.json)',
    '- [XCTest 实际点击日志](ui-click.log)',
    '- [XCTest 结果与截图](ui-click-01.xcresult/)',
    '- [一键流程原始清单](ui-watch-01/latest.json)','']
(ROOT/'report.md').write_text('\n'.join(lines))
print(json.dumps(verification,ensure_ascii=False,indent=2))
