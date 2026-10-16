from pathlib import Path
import csv
import hashlib
import json
import math
import re
import sys

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT / "plot-deps"))
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

ORIGINAL = {
    "stream-1048576": "20261014-012110-696",
    "stream-4194304": "20261014-030312-355",
    "stream-16777216": "20261014-030328-879",
    "gpu_burn-16": "20261014-030344-824",
    "vulkan/stream-1048576": "20261014-012908-798",
    "vulkan/gpu_burn-64": "20261014-013107-618",
    "vulkan/gpu_burn-256": "20261014-030946-556",
}
LABELS = {
    "stream-1048576": "Metal particles 1M", "stream-4194304": "Metal particles 4M",
    "stream-16777216": "Metal particles 16M", "gpu_burn-16": "Metal GPU Burn 16",
    "vulkan/stream-1048576": "Vulkan particles 1M",
    "vulkan/gpu_burn-64": "Vulkan GPU Burn 64",
    "vulkan/gpu_burn-256": "Vulkan GPU Burn 256",
}
CHINESE = {
    "stream-1048576": "Metal 粒子 1M", "stream-4194304": "Metal 粒子 4M",
    "stream-16777216": "Metal 粒子 16M", "gpu_burn-16": "Metal GPU Burn 16",
    "vulkan/stream-1048576": "Vulkan 粒子 1M",
    "vulkan/gpu_burn-64": "Vulkan GPU Burn 64",
    "vulkan/gpu_burn-256": "Vulkan GPU Burn 256",
}
PATTERN = re.compile(r"\[GPU Timing\] Compute: ([0-9.]+) ms \| Render: ([0-9.]+) ms \| Total: ([0-9.]+) ms \| FPS: (\d+) \| T: ([0-9.]+) s(.*)")

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

before_path = ROOT / "before/results/results.json"
before = json.loads(before_path.read_text())
old = {r["id"]: r for r in before}
folders = sorted(p for p in ROOT.iterdir() if p.is_dir() and p.name.startswith(("first-", "repeat-")) and (p / "documents/results/results.json").exists())
assert len(folders) >= 7
final_path = ROOT / ("repeat-final" if (ROOT / "repeat-final").exists() else "first-final") / "results/results.json"
after = json.loads(final_path.read_text())
records = {r["id"]: r for r in after}
assert len(records) == len(after), "Duplicate result IDs"
assert all(records[r["id"]] == r for r in before), "Old history changed"
audit = []
points = []
selected = {}
ids = set()
for folder in folders:
    manifest = json.loads((folder / "documents/suite-runs/latest.json").read_text())
    assert manifest["status"] in ("passed", "review") and len(manifest["jobs"]) == 1
    job = manifest["jobs"][0]
    assert job["id"] in ORIGINAL and len(job["resultIDs"]) == 1
    r = records[job["resultIDs"][0]]
    ids.add(r["id"])
    config = dict(s.split("=", 1) for s in r["workloadConfig"].split(";") if "=" in s)
    assert r["platform"] == "iOS" and r["deviceModel"] == "iPhone14,2"
    assert r["resWidth"] == 1280 and r["resHeight"] == 720 and not r["isSoftware"]
    assert r["warmupSec"] == 2 and r["durationSec"] >= 12 and r["timingSamples"] > 0
    assert math.isfinite(r["score"]) and r["score"] > 0
    assert r["avgComputeMs"] + r["avgRenderMs"] <= r["avgTotalGpuMs"] + .1
    assert r["avgTotalGpuMs"] <= r["avgFrameTimeMs"] * 1.1
    log_path = folder / "documents/suite-runs" / (manifest["id"] + ".log")
    log = log_path.read_text()
    if r["graphicsApi"] == "Metal":
        assert r["framesInFlight"] == 1
        assert r["workloadVersion"].endswith("_serial_timing_v2")
        assert config["timingMode"] == "serialized_command_buffer_gpu_time"
    else:
        assert config["apiImplementation"] == "MoltenVK" and config["translatedTo"] == "Metal"
        if r["workload"] == "gpu_burn":
            assert r["framesInFlight"] == 1
            assert config["timingMode"] in ("validated_single_frame_gpu_query", "synchronized_wall_clock")
        else:
            assert r["framesInFlight"] == 2
            assert config["timingMode"] in ("gpu_timestamp_query", "synchronized_wall_clock")
    checks = []
    for line in log.splitlines():
        if "timestamp check:" not in line and "Timestamp check:" not in line:
            continue
        values = [float(value) for value in re.findall(r"([0-9.]+) ms", line)]
        assert len(values) >= 2
        ratio = values[0] / values[-1]
        if config["timingMode"] != "synchronized_wall_clock":
            assert .5 < ratio < 2, (job["id"], line)
        checks.append({"gpuMs": values[0], "wallMs": values[-1], "gpuWallRatio": ratio, "logLine": line})
    entry = {"run": folder.name, "job": job["id"], "manifestID": manifest["id"],
             "status": job["status"], "reviewReasons": job["error"],
             "thermalStart": job["thermalStateStart"], "thermalEnd": job["thermalStateEnd"],
             "resultID": r["id"], "gpuFrameRatio": r["avgTotalGpuMs"] / r["avgFrameTimeMs"],
             "timestampChecksMs": checks, "timingChecksPassed": True, "result": r,
             "log": str(log_path.relative_to(ROOT))}
    for line in log.splitlines():
        m = PATTERN.search(line)
        if not m:
            continue
        compute, render, total, fps, seconds = map(float, m.groups()[:5])
        score = (40 * r["particleCount"] / compute / 1e6 if r["workload"] == "stream"
                 else 1280 * 720 * int(config["steps"]) * 2 / render / 1e6)
        points.append({"run": folder.name, "job": job["id"], "resultID": r["id"],
            "seconds": seconds, "warmup": "warm-up" in m.group(6), "computeMs": compute,
            "renderMs": render, "totalGpuMs": total, "reportedFps": fps,
            "derivedScore": score, "scoreUnit": r["scoreUnit"]})
    entry["windowCount"] = sum(p["run"] == folder.name and not p["warmup"] for p in points)
    audit.append(entry)
    selected[job["id"]] = entry

assert set(selected) == set(ORIGINAL)
assert set(records) - set(old) == ids and len(after) == len(before) + len(ids)
verification = {"device": "iPhone 13 Pro / A15", "sourceCommit": "5c4e7df718728e498080b9d154abd8275ffeaace",
    "uniqueJobs": len(selected), "newRecords": len(ids), "historyBefore": len(before), "historyAfter": len(after),
    "oldRecordsUnchanged": True, "aggregateTimingChecksPassed": True,
    "validationScope": "Saved average phase sums and GPU/frame interval; startup timestamp calibration and fallback. No per-frame hardware frequency telemetry.",
    "synchronizedFallbackRuns": [e['run'] for e in audit if 'timingMode=synchronized_wall_clock' in e['result']['workloadConfig']],
    "timestampReviewJob": "vulkan/gpu_burn-256",
    "beforeSha256": sha(before_path), "afterSha256": sha(final_path),
    "remainingReviewJobs": [key for key, value in selected.items() if value["status"] == "review"]}
(ROOT / "verification.json").write_text(json.dumps(verification, ensure_ascii=False, indent=2) + "\n")
(ROOT / "audit.json").write_text(json.dumps(audit, ensure_ascii=False, indent=2) + "\n")
(ROOT / "selected-results.json").write_text(json.dumps([dict(job=k, **v["result"]) for k, v in selected.items()], ensure_ascii=False, indent=2) + "\n")
prior_curated = json.loads((ROOT.parent / "apple-targeted-retest-20261014/iphone-curated-suite-results.json").read_text())
replacement = {ORIGINAL[k]: v["result"] for k, v in selected.items()}
curated = [replacement.get(r["id"], r) for r in prior_curated]
assert len(curated) == 32 and sum(r != oldr for r, oldr in zip(curated, prior_curated)) == 7
(ROOT / "curated-suite-results.json").write_text(json.dumps(curated, ensure_ascii=False, indent=2) + "\n")
with (ROOT / "curves.csv").open("w") as stream:
    writer = csv.DictWriter(stream, fieldnames=list(points[0]))
    writer.writeheader()
    writer.writerows(points)

fig, axes = plt.subplots(len(ORIGINAL), 2, figsize=(13, 2.65 * len(ORIGINAL)), layout="constrained")
for index, key in enumerate(ORIGINAL):
    left, right = axes[index]
    runs = [e for e in audit if e["job"] == key]
    for run_index, e in enumerate(runs):
        samples = [p for p in points if p["run"] == e["run"]]
        xs = [p["seconds"] for p in samples]
        style = "-" if run_index == 0 else "--"
        tag = "first" if run_index == 0 else "repeat"
        for metric, label, color in [("computeMs", "Compute", "#236fbb"), ("renderMs", "Render", "#dd853a"), ("totalGpuMs", "Total GPU", "#334755")]:
            left.plot(xs, [p[metric] for p in samples], linestyle=style, marker=".", color=color, label=f"{label} ({tag})", linewidth=1.3)
        right.plot(xs, [p["derivedScore"] for p in samples], linestyle=style, marker=".", label=tag, linewidth=1.6)
    r = selected[key]["result"]
    left.set_title(LABELS[key]); left.set_ylabel("Time (ms)")
    stable_label = f"{r['stableScore']:.3f}" if r['stableScore'] > 0 else "not detected"
    right.set_title(f"Latest avg {r['score']:.3f} / stable {stable_label}; drop {r['throttlePct']:.2f}%")
    right.set_ylabel(r["scoreUnit"])
    left.legend(fontsize=7, ncol=2)
    if len(runs) > 1:
        right.legend(fontsize=8)
    for ax in (left, right):
        ax.axvspan(0, 2, color="#d4dce2", alpha=.45)
        ax.grid(alpha=.2); ax.set_xlabel("Elapsed seconds"); ax.spines[["top", "right"]].set_visible(False)
fig.suptitle("A15 targeted retests | Original workloads | 1280x720 | 15s incl. 2s warm-up", fontsize=15)
fig.supxlabel("Grey: warm-up. Curves use rounded log windows; final scores come from saved JSON. Vulkan uses MoltenVK.\nBurn 64 repeat uses synchronized elapsed time instead of GPU query timing.", fontsize=9)
fig.savefig(ROOT / "a15-curves.png", dpi=150)
fig.savefig(ROOT / "a15-curves.pdf")

rows = ["# A15／iPhone 13 Pro 定向复测（2026-10-14）", "",
    f"7 项均已完成，其中 4 项额外复核一次，共新增 {len(ids)} 条成绩。原有 {len(before)} 条逐项校验未改动，手机历史现在共 {len(after)} 条。保存字段中的平均计时区间核对通过；Vulkan Burn 256 仍保留逐帧计时诊断要求。", "",
    "当前构建：Release、`5c4e7df`，iOS 27.0.1，1280×720，原始着色器，15 秒运行含 2 秒预热。每项独立启动，间隔 20 秒，并由应用在开始前检查系统热状态；若非正常则等待降温。", "",
    "Vulkan 是 MoltenVK 转到 Metal。Metal 单帧序列化计时；Vulkan 粒子使用双帧，Burn 使用单帧并验证 GPU 时间戳。请求关闭垂直同步，但 iOS 的显示路径仍跟随屏幕同步，结果中的 `vsync=true` 反映平台行为。", "",
    "| 项目 | 原均值 | 本轮均值 | 已检测稳定段 | 前后下降 | 热状态 | 判断 |", "| --- | ---: | ---: | ---: | ---: | --- | --- |"]
for key in ORIGINAL:
    e = selected[key]; r = e["result"]; prior = old[ORIGINAL[key]]
    stable = f"{r['stableScore']:.3f}" if r["stableScore"] > 0 else "未检测到"
    state = "通过" if e["status"] == "passed" else "保留复核"
    if e["status"] == "passed" and "timingMode=synchronized_wall_clock" in r["workloadConfig"]:
        state = "通过（同步计时）"
    rows.append(f"| {CHINESE[key]} | {prior['score']:.3f} | {r['score']:.3f} | {stable} | {r['throttlePct']:.2f}% | {e['thermalStart']}→{e['thermalEnd']} | {state} |")
rows += ["", "粒子单位为 GB/s（计算阶段按每粒子 40 B 推导，不能当作独立内存扫描带宽）；Burn 单位为 Gpix-step/s。旧计时／提交帧数与本轮不同的项目，变化幅度不能全归因于性能提升。", "", "## 所有观察值", "",
    "| 项目 / 轮次 | 成绩编号 | 均值 | 稳定段 | CV | 前后下降 | 热状态 |", "| --- | --- | ---: | ---: | ---: | ---: | --- |"]
for e in audit:
    r=e["result"]
    rows.append(f"| {CHINESE[e['job']]} / {'首次' if e['run'].startswith('first-') else '复核'} | `{r['id']}` | {r['score']:.3f} | {r['stableScore']:.3f} | {r['stableVariancePct']:.2f}% | {r['throttlePct']:.2f}% | {e['thermalStart']}→{e['thermalEnd']} |")
rows += ["", "下降比例来自最早与最后各 5 个计时窗口，不是硬件频率遥测；负数表示吞吐上升。`stableScore=0` 表示未检测到稳定段。检测到的稳定段可能在早期，不能在后期下降时当作最终持续成绩。系统热状态 0 是正常状态，不能据此断言 GPU 没有调频。", "",
    "## 本轮判断", "",
    "Metal 4M、16M、Burn 16 达到稳定段且没有大于 5% 的前后吞吐下降，原复测候选可以关闭。两种 API 的 1M 绘制下降都在第二轮重现，计时核对通过，但不能当作固定负载的稳定硬件峰值。", "",
    "1M 曲线显示：绘制时间随运行缩短，FPS 接近 60 后计算时间增加。当前证据提示动态绘制负载和显示同步需要纳入解释；没有固定画面密度的 A15 独立对照，也没有硬件频率遥测，不能把原因写成已证实的温控降频。Mac 的隔离诊断不代替这台手机的验证。", "",
    "Vulkan Burn 64 复核时，GPU 时间戳报告 0.216 ms，而同步提交／等待实际为 115.976 ms，已触发自动回退。回退后均值 1.042 Gpix-step/s、稳定段 1.041、CV 0.82%，通过；它按同步实际耗时计分，不能与第一轮 GPU 查询计时的 1.121 当作同一口径。", "",
    "Vulkan Burn 256 两轮均无稳定段，均值 1.367／1.623 Gpix-step/s、前后下降 9.10%／14.98%。第二轮启动时校准为 425.549 ms GPU／426.142 ms 同步等待，随后各窗口仍有明显波动，平均 GPU 时间为 296.257 ms、帧间隔为 432.945 ms。初始校准和平均区间自洽不足以证明整轮查询都可靠；该项目还需要逐帧同步计时对照，当前均值只保留作观察值。", "",
    "此次候选测试已经完成，不再重复标准 15 秒测试挑选更高分；后续如要解释剩余波动，应做独立计时／调度／负载对照。", "",
    "## 原始证据", "", "- [完整手机成绩](" + str(final_path.relative_to(ROOT)) + ")",
    "- [本次核对与文件指纹](verification.json)", "- [逐项计时、热状态、日志路径](audit.json)",
    "- [当前 32 条全套选定成绩](curated-suite-results.json)", "- [完整曲线 CSV](curves.csv)",
    "- [曲线 PNG](a15-curves.png) / [PDF](a15-curves.pdf)", "", "![A15 curves](a15-curves.png)"]
(ROOT / "report.md").write_text("\n".join(rows) + "\n")
print(json.dumps(verification, ensure_ascii=False, indent=2))
