// Reads the results the engine saved: <files dir>/results/results.json, the
// same file format as on every other platform. The engine is the only writer.
// Every member is read as optional, so files of older builds (members
// missing) and of newer builds (members this app does not know) both load.

import { fileIo as fs } from '@kit.CoreFileKit';

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
