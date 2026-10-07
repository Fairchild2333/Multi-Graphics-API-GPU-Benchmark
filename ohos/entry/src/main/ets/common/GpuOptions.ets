// The GPU tests and their sizes, as in the desktop app: the same names, the
// same presets, the same limits.
//
// The engine host takes a size as part of the name of the test, for example
// "stream?particles=4194304" or "gpu_burn?steps=64" (src/embedded_request.h).

import { tr } from './I18n';

export class GpuTest {
  id: string;
  nameEn: string;
  nameZh: string;

  constructor(id: string, nameEn: string, nameZh: string) {
    this.id = id;
    this.nameEn = nameEn;
    this.nameZh = nameZh;
  }
}

export const GPU_WORKLOADS: GpuTest[] = [
  new GpuTest('stream', 'Particle — Memory Throughput', '粒子 —— 内存吞吐'),
  new GpuTest('gpu_burn', 'Plasma x Kaleidoscope — GPU Burn', '等离子晶核 × 万花镜 —— GPU Burn'),
  new GpuTest('cinematic_liquid', 'Fluid — Interactive Pool', '流体 —— 互动水池'),
  new GpuTest('membw', 'Memory Bandwidth — VRAM / Cache', '显存带宽 —— 显存 / 缓存'),
];

export function gpuTestName(id: string): string {
  for (const test of GPU_WORKLOADS) {
    if (test.id === id) {
      return tr(test.nameEn, test.nameZh);
    }
  }
  return id;
}

/** A preset: its value, and its name in English and Chinese. */
export class Preset {
  value: number;
  nameEn: string;
  nameZh: string;

  constructor(value: number, nameEn: string, nameZh: string) {
    this.value = value;
    this.nameEn = nameEn;
    this.nameZh = nameZh;
  }
}

/** Stands for "Custom…" in a row of presets. */
export const CUSTOM: number = -1;

export const PARTICLE_PRESETS: Preset[] = [
  new Preset(65536, 'Light — 65K', '轻量 —— 65K'),
  new Preset(1048576, 'Medium — 1M', '中等 —— 1M'),
  new Preset(4194304, 'Heavy — 4M', '重载 —— 4M'),
  new Preset(16777216, 'Extreme — 16M', '极限 —— 16M'),
];

export const STEP_PRESETS: Preset[] = [
  new Preset(16, 'Light — 16', '轻量 — 16'),
  new Preset(64, 'Medium — 64', '中等 — 64'),
  new Preset(256, 'Heavy — 256', '重载 — 256'),
];

export const SECOND_PRESETS: Preset[] = [
  new Preset(3, '3 s', '3 秒'),
  new Preset(15, '15 s', '15 秒'),
  new Preset(60, '60 s', '60 秒'),
];

// What the desktop app selects at first.
export const DEFAULT_PARTICLES: number = 1048576;
export const DEFAULT_STEPS: number = 16;
export const DEFAULT_SECONDS: number = 15;

export class Limits {
  min: number;
  max: number;

  constructor(min: number, max: number) {
    this.min = min;
    this.max = max;
  }
}

export const PARTICLE_LIMITS: Limits = new Limits(1, 67108864);
export const STEP_LIMITS: Limits = new Limits(16, 2048);
export const SECOND_LIMITS: Limits = new Limits(1, 3600);

/** A whole number written with digits only, or -1. */
function wholeNumber(text: string): number {
  const trimmed: string = text.trim();
  if (trimmed.length === 0 || trimmed.length > 9) {
    return -1;
  }
  for (let i = 0; i < trimmed.length; i++) {
    const code: number = trimmed.charCodeAt(i);
    if (code < 48 || code > 57) {
      return -1;
    }
  }
  return Number.parseInt(trimmed, 10);
}

/** The value of a choice: the preset, or the number typed for "Custom…"; -1 when that is not valid. */
export function resolveChoice(choice: number, custom: string, limits: Limits): number {
  if (choice !== CUSTOM) {
    return choice;
  }
  const value: number = wholeNumber(custom);
  return value >= limits.min && value <= limits.max ? value : -1;
}

/** True when `value` is one of `presets`. */
export function isPreset(presets: Preset[], value: number): boolean {
  for (const preset of presets) {
    if (preset.value === value) {
      return true;
    }
  }
  return false;
}

/**
 * What the engine is asked to run. A headless particle test computes only: it
 * shows no picture and runs through the command-line entry.
 */
export function gpuRequest(testId: string, particles: number, steps: number, headless: boolean): string {
  if (testId === 'stream') {
    return `${headless ? 'stream_headless' : 'stream'}?particles=${particles}`;
  }
  if (testId === 'gpu_burn') {
    return `gpu_burn?steps=${steps}`;
  }
  return testId;
}

/** The name the engine saves a run under. */
export function gpuSavedAs(testId: string, steps: number, seconds: number): string {
  if (testId === 'stream') {
    return 'stream_v1_ohos_preview';
  }
  if (testId === 'gpu_burn') {
    return `gpu_burn_v3_fixed_steps_${steps}_kaleidoscope_ohos_preview`;
  }
  if (testId === 'cinematic_liquid') {
    return seconds === 15 ? 'cinematic_liquid_v2_physical_scene_v8_ohos_preview'
      : 'cinematic_liquid_v2_physical_scene_v8_preview_ohos_preview';
  }
  if (testId === 'membw') {
    return 'membw_v1_ohos_preview';
  }
  return testId;
}

/** The part of a request before its options. */
export function gpuRequestId(request: string): string {
  const mark: number = request.indexOf('?');
  return mark < 0 ? request : request.substring(0, mark);
}

/** A request as hdc names it, taken apart: the test of the page, and the size it asks for (0: none). */
export class ParsedRequest {
  testId: string;
  particles: number;
  steps: number;
  headless: boolean = false;

  constructor(testId: string, particles: number, steps: number) {
    this.testId = testId;
    this.particles = particles;
    this.steps = steps;
  }
}

/** The arguments of a test that runs through the engine's command-line entry. */
export function gpuCommandLine(request: string, seconds: number): string[] {
  const id: string = gpuRequestId(request);
  if (id === 'stream_headless') {
    const particles: number = parseGpuRequest(request).particles;
    return ['--backend', 'vulkan', '--gpu', '0', '--workload', 'stream', '--particles', particles.toString(),
      '--headless', '--time', seconds.toString(), '--no-renderdoc'];
  }
  return ['--workload', id, '--backend', 'vulkan', '--gpu', '0'];
}

export function parseGpuRequest(request: string): ParsedRequest {
  const id: string = gpuRequestId(request);
  let particles: number = 0;
  let steps: number = 0;
  const mark: number = request.indexOf('?');
  if (mark >= 0) {
    for (const pair of request.substring(mark + 1).split('&')) {
      const equals: number = pair.indexOf('=');
      if (equals < 0) {
        continue;
      }
      const value: number = wholeNumber(pair.substring(equals + 1));
      if (value <= 0) {
        continue;
      }
      if (pair.substring(0, equals) === 'particles') {
        particles = value;
      } else if (pair.substring(0, equals) === 'steps') {
        steps = value;
      }
    }
  }
  if (id === 'stream' || id === 'particle') {
    return new ParsedRequest('stream', particles > 0 ? particles : DEFAULT_PARTICLES, 0);
  }
  if (id === 'stream_heavy') {
    return new ParsedRequest('stream', particles > 0 ? particles : 1048576, 0);
  }
  if (id === 'stream_headless') {
    const asked: ParsedRequest = new ParsedRequest('stream', particles > 0 ? particles : DEFAULT_PARTICLES, 0);
    asked.headless = true;
    return asked;
  }
  if (id === 'gpu_burn') {
    return new ParsedRequest('gpu_burn', 0, steps > 0 ? steps : DEFAULT_STEPS);
  }
  if (id === 'gpu_burn_medium') {
    return new ParsedRequest('gpu_burn', 0, steps > 0 ? steps : 64);
  }
  if (id === 'gpu_burn_heavy') {
    return new ParsedRequest('gpu_burn', 0, steps > 0 ? steps : 256);
  }
  return new ParsedRequest(id, 0, 0);
}
