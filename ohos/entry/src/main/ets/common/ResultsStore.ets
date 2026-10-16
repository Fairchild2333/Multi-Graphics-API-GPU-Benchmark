// Reads the results the engine saved: <files dir>/results/results.json, the
// same file format as on every other platform. The engine is the only writer.
// Every member is read as optional, so files of older builds (members
// missing) and of newer builds (members this app does not know) both load.

import { fileIo as fs } from '@kit.CoreFileKit';
import { tr } from './I18n';

export interface ResultJob { api: string; workload: string; particles: number; steps: number; headless: boolean; }

export class ResultEntry {
  id: string = '';
  timestamp: string = '';
  workload: string = '';
  workloadVersion: string = '';
  graphicsApi: string = '';
  deviceName: string = '';
  cpuName: string = '';
  score: number = 0;
  scoreUnit: string = '';
  // Make and model of the device, its system release and memory; empty or 0 in older results.
  deviceModel: string = '';
  osVersion: string = '';
  systemMemoryMB: number = 0;
  particleCount: number = 0;
  headless: boolean = false;
  durationSec: number = 0;
  workloadConfig: string = '';
  avgComputeMs: number = 0;
  avgRenderMs: number = 0;
  avgTotalGpuMs: number = 0;
  avgFrameTimeMs: number = 0;
  stableScore: number = 0;
  throughputDropPct: number = 0;

  reviewReasons(): string[] {
    const reasons: string[] = [];
    if (this.workload === 'membw' && this.graphicsApi === 'Vulkan' && this.workloadVersion.startsWith('membw_v3_streaming')) {
      if (this.workloadConfig.split(';').indexOf('memoryPlateauReached=1') < 0) reasons.push(tr('Memory plateau was not reached', '尚未达到内存带宽平台'));
      return reasons;
    }
    if (['stream', 'gpu_burn', 'cinematic_liquid'].indexOf(this.workload) < 0) return reasons;
    if (this.avgFrameTimeMs > 0 && this.avgTotalGpuMs > this.avgFrameTimeMs * 1.1) reasons.push(tr('GPU timing exceeds frame interval', 'GPU 计时超过帧间隔'));
    if (this.avgComputeMs + this.avgRenderMs > this.avgTotalGpuMs + 0.1) reasons.push(tr('Compute/render timing overlaps', '计算与渲染计时重叠'));
    if (this.headless && this.graphicsApi === 'Vulkan' && this.particleCount >= 1048576 && this.avgComputeMs < this.avgFrameTimeMs * 0.5)
      reasons.push(tr('Compute timing needs verification', '计算计时需要核验'));
    if (this.workload !== 'cinematic_liquid') {
      if (this.throughputDropPct > 5) reasons.push(tr('Throughput dropped by over 5%', '吞吐下降超过 5%'));
      if ((this.workload === 'gpu_burn' || this.particleCount >= 1048576) && this.stableScore <= 0) reasons.push(tr('Stable throughput not reached', '尚未达到稳定吞吐'));
    }
    return reasons;
  }
  matches(job: ResultJob): boolean {
    if (!isFinite(this.score) || this.score <= 0) return false;
    if (job.api === 'cpu') return job.workload === 'cpu_all' ? ['cpu_single_core', 'cpu_multi_core'].indexOf(this.workload) >= 0 : this.workload === 'cpu_membw';
    const api: string = job.api === 'gles2' ? 'OpenGL ES 2.0' : job.api === 'gles' ? 'OpenGL ES' : 'Vulkan';
    if (this.graphicsApi !== api || this.workload !== job.workload || this.headless !== job.headless) return false;
    if (job.workload === 'membw') {
      const fields: string[] = this.workloadConfig.split(';');
      const checks: string = fields.find((f: string) => f.startsWith('writeCopyChecks=')) ?? '';
      return this.workloadVersion.startsWith('membw_v3_streaming_') && fields.indexOf('dataVerified=1') >= 0 &&
        fields.indexOf('largeDatasetTraversal=ordered_full_dataset_passes') >= 0 &&
        Number(checks.substring('writeCopyChecks='.length)) > 0 && fields.indexOf('timestampAnomalies=0') >= 0;
    }
    if (this.durationSec < 11 || (job.particles > 0 && this.particleCount !== job.particles)) return false;
    return job.steps === 0 || this.workloadConfig.split(';').indexOf('steps=' + job.steps.toString()) >= 0 ||
      this.workloadVersion.indexOf(`fixed_steps_${job.steps}_`) >= 0;
  }

  /** "HUAWEI Mate 60 Pro (ALN-AL00) · HarmonyOS 5.0.0 (API 12) · 11.2 GB RAM", from what the result has. */
  machineLine(): string {
    const parts: string[] = [];
    if (this.deviceModel.length > 0) {
      parts.push(this.deviceModel);
    }
    // A CPU result already shows the chip as its device name.
    if (this.cpuName.length > 0 && this.cpuName !== this.deviceName && !this.cpuName.startsWith('Unknown')) {
      parts.push(this.cpuName);
    }
    if (this.osVersion.length > 0) {
      parts.push(this.osVersion);
    }
    if (this.systemMemoryMB > 0) {
      parts.push((this.systemMemoryMB / 1024).toFixed(1) + ' GB RAM');
    }
    return parts.join(' · ');
  }
}

interface RawResult {
  id?: string;
  timestamp?: string;
  workload?: string;
  workloadVersion?: string;
  graphicsApi?: string;
  deviceName?: string;
  cpuName?: string;
  score?: number;
  scoreUnit?: string;
  deviceModel?: string;
  osVersion?: string;
  systemMemoryMB?: number;
  particleCount?: number; headless?: boolean; durationSec?: number; workloadConfig?: string;
  avgComputeMs?: number; avgRenderMs?: number; avgTotalGpuMs?: number; avgFrameTimeMs?: number;
  stableScore?: number; throttlePct?: number;
}

export function resultsPath(filesDir: string): string {
  return filesDir + '/results/results.json';
}

/** Newest first. An unreadable or missing file gives an empty list. */
export function loadResults(filesDir: string): ResultEntry[] {
  const entries: ResultEntry[] = [];
  try {
    const path: string = resultsPath(filesDir);
    if (!fs.accessSync(path)) {
      return entries;
    }
    const parsed: Object | null = JSON.parse(fs.readTextSync(path));
    if (parsed === null || !Array.isArray(parsed)) {
      return entries;   // not the list of results this app reads
    }
    const raw: RawResult[] = parsed as RawResult[];
    for (const item of raw) {
      if (!item) {
        continue;   // an entry that is no result
      }
      const entry: ResultEntry = new ResultEntry();
      entry.id = item.id ?? '';
      entry.timestamp = item.timestamp ?? '';
      entry.workload = item.workload ?? '';
      entry.workloadVersion = item.workloadVersion ?? '';
      entry.graphicsApi = item.graphicsApi ?? '';
      entry.deviceName = item.deviceName ?? '';
      entry.cpuName = item.cpuName ?? '';
      entry.score = item.score ?? 0;
      entry.scoreUnit = item.scoreUnit ?? '';
      entry.deviceModel = item.deviceModel ?? '';
      entry.osVersion = item.osVersion ?? '';
      entry.systemMemoryMB = item.systemMemoryMB ?? 0;
      entry.particleCount = item.particleCount ?? 0; entry.headless = item.headless ?? false;
      entry.durationSec = item.durationSec ?? 0; entry.workloadConfig = item.workloadConfig ?? '';
      entry.avgComputeMs = item.avgComputeMs ?? 0; entry.avgRenderMs = item.avgRenderMs ?? 0;
      entry.avgTotalGpuMs = item.avgTotalGpuMs ?? 0; entry.avgFrameTimeMs = item.avgFrameTimeMs ?? 0;
      entry.stableScore = item.stableScore ?? 0; entry.throughputDropPct = item.throttlePct ?? 0;
      entries.push(entry);
    }
  } catch (e) {
    return [];
  }
  entries.sort((a: ResultEntry, b: ResultEntry) => {
    if (a.timestamp === b.timestamp) {
      return 0;
    }
    return a.timestamp < b.timestamp ? 1 : -1;
  });
  return entries;
}

export function clearResults(filesDir: string): void {
  try {
    const path: string = resultsPath(filesDir);
    if (!fs.accessSync(path)) {
      return;
    }
    const file: fs.File = fs.openSync(path, fs.OpenMode.READ_WRITE | fs.OpenMode.TRUNC);
    fs.writeSync(file.fd, '[]\n');
    fs.closeSync(file);
  } catch (e) {
    return;
  }
}
