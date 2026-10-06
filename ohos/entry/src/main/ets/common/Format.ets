// Number and size formatting shared by the tables and the charts.

/** A whole number with thousands separators. */
export function formatGrouped(value: number): string {
  if (!isFinite(value)) {
    return '-';
  }
  const digits: string = Math.round(Math.abs(value)).toString();
  let out: string = value < 0 ? '-' : '';
  for (let i = 0; i < digits.length; i++) {
    if (i > 0 && (digits.length - i) % 3 === 0) {
      out += ',';
    }
    out += digits.charAt(i);
  }
  return out;
}

/** "1,863", "74.7", "0.024": three significant digits. */
export function formatNumber(value: number): string {
  if (!isFinite(value)) {
    return '-';
  }
  if (value >= 1000) {
    return formatGrouped(value);
  } else if (value >= 100) {
    return value.toFixed(0);
  } else if (value >= 10) {
    return value.toFixed(1);
  } else if (value >= 1) {
    return value.toFixed(2);
  } else if (value > 0) {
    return value.toFixed(3);
  }
  return '0';
}

/** Axis tick: "0.5", "2", "200", "1,000". */
export function formatTick(value: number): string {
  if (!isFinite(value)) {
    return '-';
  }
  if (value >= 1000) {
    return formatGrouped(value);
  }
  if (value === 0) {
    return '0';
  }
  return parseFloat(value.toPrecision(3)).toString();
}

/** "48 KiB", "1.5 MiB", "1 GiB". */
export function formatBytes(bytes: number): string {
  if (!isFinite(bytes) || bytes < 0) {
    return '-';
  }
  const kib: number = bytes / 1024;
  if (kib < 1024) {
    return Math.round(kib).toString() + ' KiB';
  }
  const mib: number = kib / 1024;
  if (mib >= 1024 && mib % 1024 < 0.01) {
    return Math.round(mib / 1024).toString() + ' GiB';
  }
  if (mib % 1 < 0.01) {
    return Math.round(mib).toString() + ' MiB';
  }
  return mib.toFixed(1) + ' MiB';
}
