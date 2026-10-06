// Reads the results the engine saved: <files dir>/results/results.json, the
// same file format as on every other platform.

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
    const raw: RawResult[] = JSON.parse(fs.readTextSync(path)) as RawResult[];
    for (const item of raw) {
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
