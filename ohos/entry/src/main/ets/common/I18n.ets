// Interface text in English and Simplified Chinese. The language follows the
// system, like the "Auto" setting of the desktop front ends; every other
// language gets English. The wording matches the Windows and Android apps.

import { i18n } from '@kit.LocalizationKit';

let chineseCache: boolean | undefined = undefined;

function isChinese(): boolean {
  if (chineseCache === undefined) {
    let language: string = 'en';
    try {
      language = i18n.System.getSystemLanguage();
    } catch (e) {
      language = 'en';
    }
    chineseCache = language.startsWith('zh');
  }
  return chineseCache;
}

/** Called when the system configuration changes, so the next text is in the new language. */
export function resetLanguage(): void {
  chineseCache = undefined;
}

export function tr(en: string, zh: string): string {
  return isChinese() ? zh : en;
}

/** Display name of a test. */
export function workloadTitle(id: string): string {
  if (id === 'stream') {
    return tr('Particle — memory throughput', '粒子 —— 内存吞吐');
  } else if (id === 'gpu_burn') {
    return 'GPU Burn';
  } else if (id === 'cinematic_liquid') {
    return tr('Fluid — interactive pool', '流体 —— 互动水池');
  } else if (id === 'membw') {
    return tr('Memory bandwidth — VRAM / cache', '显存带宽 —— 显存 / 缓存');
  } else if (id === 'cpu_single_core') {
    return tr('CPU — single-core', 'CPU —— 单核');
  } else if (id === 'cpu_multi_core') {
    return tr('CPU — all-core', 'CPU —— 全核');
  } else if (id === 'cpu_membw') {
    return tr('CPU — cache & memory bandwidth', 'CPU —— 缓存与内存带宽');
  }
  return id;
}

class Phrase {
  en: string;
  zh: string;

  constructor(en: string, zh: string) {
    this.en = en;
    this.zh = zh;
  }
}

// "VRAM rate:" comes before "RAM rate:", which it contains.
const SCORE_LABELS: Phrase[] = [
  new Phrase('VRAM rate:', '显存速率:'),
  new Phrase('RAM rate:', '内存速率:'),
  new Phrase('Memory rate:', '显存速率:'),
  new Phrase('Avg FPS:', '平均帧率:'),
  new Phrase('Compute rate:', '计算速率:'),
  new Phrase('Burn rate:', 'Burn 速率:'),
  new Phrase('Stress rate:', '压力速率:'),
  new Phrase('Fill rate:', '填充速率:'),
  new Phrase('Render rate:', '渲染速率:'),
  new Phrase('Vol rate:', '体积速率:'),
  new Phrase('Fluid rate:', '流体速率:'),
  new Phrase('Liquid rate:', '流体速率:'),
];

function replaceAll(text: string, from: string, to: string): string {
  return text.split(from).join(to);
}

/**
 * The engine prints its summary in English. This replaces the label at the
 * start of a score line, such as "VRAM rate:", and leaves the figures alone.
 */
export function localizeScoreLine(line: string): string {
  if (!isChinese()) {
    return line;
  }
  for (const label of SCORE_LABELS) {
    if (line.startsWith(label.en)) {
      let rest: string = line.substring(label.en.length).trim();
      rest = replaceAll(rest, 'ms compute', 'ms 计算');
      rest = replaceAll(rest, 'ms render', 'ms 渲染');
      return label.zh + ' ' + rest;
    }
  }
  return line;
}

const ENGINE_PHRASES: Phrase[] = [
  new Phrase('no page file', '没有页面文件'),
  new Phrase('zram (memory is compressed in RAM; no drive is involved)', 'zram（内存在 RAM 里压缩，不经过硬盘）'),
  new Phrase('no swap', '没有交换分区'),
  new Phrase('unknown', '未知'),
  new Phrase('no temporary directory', '没有可用的临时目录'),
  new Phrase('the directory is on tmpfs, which is memory and not a drive', '目录位于 tmpfs，它是内存而不是硬盘'),
  new Phrase('the directory does not exist', '目录不存在'),
  new Phrase('not enough free space for the test file', '剩余空间不足以写测试文件'),
  new Phrase('allocation failed', '内存分配失败'),
  new Phrase('the test file could not be created', '无法创建测试文件'),
  new Phrase('a read or write on the test file failed', '读写测试文件时出错'),
  new Phrase('switched off', '已关闭'),
];

/**
 * The engine names the paging target and the reason for skipping the storage
 * test in English. These are all the phrases it uses.
 */
export function localizeEngineText(text: string): string {
  if (!isChinese()) {
    return text;
  }
  if (text.startsWith('swap on ')) {
    return text.substring('swap on '.length) + ' 上的交换分区';
  }
  for (const phrase of ENGINE_PHRASES) {
    if (text === phrase.en) {
      return phrase.zh;
    }
  }
  return text;
}
