"""Dump additional functions needed for the sigreturn/poll route geometry audit.
Read-only: never communicates with a device. Outputs .disasm.txt next to the audit."""
from pathlib import Path
import bisect
import hashlib
import json
import sys

OUT = Path(__file__).resolve().parent
ROOT = OUT.parent
sys.path.insert(0, str(ROOT / 'tools' / 'analysis-pydeps'))
from capstone import Cs, CS_ARCH_ARM64, CS_MODE_LITTLE_ENDIAN

image = (OUT / 'Image').read_bytes()
assert hashlib.sha256(image).hexdigest() == '136177a1c360eb64a8556cb4e6385a06e34c2ceabc05c7da714428837309bf6f'
symbols = json.loads((OUT / 'symbols.json').read_text())
base = symbols['_text'][0]
addresses = sorted({v for vs in symbols.values() for v in vs if base <= v < base + len(image)})
decoder = Cs(CS_ARCH_ARM64, CS_MODE_LITTLE_ENDIAN)

NAMES = [
    'parse_user_sigframe',
    'restore_sve_fpsimd_context',
    'fpsimd_update_current_state',
    'valid_user_regs',
    'set_current_blocked',
    'restore_sigcontext',
    'restore_esr_context',
    'restore_extra_context',
    'fpsimd_to_sve_state',
    'sve_init_vq_map',  # may be absent; skip quietly
]

for name in NAMES:
    if name not in symbols:
        print(f'MISSING {name}')
        continue
    start = symbols[name][0]
    index = bisect.bisect_right(addresses, start)
    end = addresses[index]
    items = list(decoder.disasm(image[start - base:end - base], start))
    lines = [f'{i.address:08x}: {i.mnemonic} {i.op_str}'.rstrip() for i in items]
    (OUT / f'{name}.disasm.txt').write_text('\n'.join(lines) + '\n', encoding='utf-8')
    print(f'{name}: start={start:#x} end={end:#x} size={end - start:#x} insns={len(lines)}')
