"""Reproduce the MP6 read-only audit. Never communicates with a device."""
from pathlib import Path
import bisect
import hashlib
import json
import re
import struct
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

def instruction(offset, expected):
    item = next(decoder.disasm(image[offset:offset + 4], base + offset))
    actual = f'{item.mnemonic} {item.op_str}'.strip()
    assert actual == expected, (hex(offset), actual, expected)
    return {'image_offset': hex(offset), 'bytes': image[offset:offset + 4].hex(), 'instruction': actual}

def function(name):
    start = symbols[name][0]
    assert len(symbols[name]) == 1
    index = bisect.bisect_right(addresses, start)
    end = addresses[index]
    return list(decoder.disasm(image[start - base:end - base], start))

def frame(name):
    # Reproduce extract_rs/disasm.rs::first_sp_frame on the same function.
    total = 0
    sub_seen = False
    for item in function(name):
        text = f'{item.mnemonic} {item.op_str}'
        found = re.search(r'stp x29, x30, \[sp, #-0x([0-9a-f]+)\]!', text)
        if found:
            total += int(found.group(1), 16)
        found = re.search(r'sub sp, sp, #0x([0-9a-f]+)', text)
        if found and not sub_seen:
            total += int(found.group(1), 16)
            sub_seen = True
    assert total
    return total

# These are target-image observations, not neighboring-kernel defaults.
fields = {
    'prio': (0x84, [(0x1e530c, 'ldr w8, [x20, #0x84]'), (0x1a0820, 'str w25, [x20, #0x84]')]),
    'normal_prio': (0x8c, [(0x1a0480, 'ldr w25, [x20, #0x8c]')]),
    'sched_task_group': (0x310, [(0x1a45e0, 'str x8, [x19, #0x310]'), (0x1a4624, 'ldr x23, [x19, #0x310]')]),
    'pi_lock': (0x86c, [(0x1e52dc, 'add x24, x2, #0x86c'), (0x1e5a58, 'add x21, x20, #0x86c')]),
    'pi_waiters': (0x880, [(0x1e55a0, 'add x1, x23, #0x880'), (0x1e5b6c, 'add x1, x21, #0x880')]),
    'pi_top_task': (0x890, [(0x1a0494, 'ldr x8, [x20, #0x890]'), (0x1a06d0, 'str x21, [x20, #0x890]')]),
    'pi_blocked_on': (0x898, [(0x1e53e0, 'str x22, [x20, #0x898]'), (0x1e5ac4, 'str xzr, [x20, #0x898]')]),
    'real_cred': (0x778, [(0x181a04, 'ldr x19, [x20, #0x778]'), (0x181c28, 'add x8, x20, #0x778')]),
    'cred': (0x780, [(0x181a08, 'ldr x8, [x20, #0x780]'), (0x1812a0, 'ldr x20, [x8, #0x780]')]),
    'comm': (0x790, [(0x565c04, 'add x22, x19, #0x790')]),
    'seccomp': (0x848, [(0x2ff1bc, 'ldr w9, [x8, #0x848]')]),
}
task = {key: {'offset': value, 'hex': hex(value), 'evidence': [instruction(*x) for x in witnesses]}
        for key, (value, witnesses) in fields.items()}

cred_offset = symbols['init_cred'][0] - base
read64 = lambda offset: struct.unpack_from('<Q', image, cred_offset + offset)[0]
assert read64(0x28) == 0 and read64(0x48) == 0
caps = [read64(x) for x in (0x30, 0x38, 0x40)]
assert caps == [0x1ffffffffff] * 3
refs = [{'offset': offset, 'image': read64(offset), 'symbol': name}
        for offset, name in [(0x80, 'root_user'), (0x88, 'init_user_ns'), (0x90, 'init_groups')]]
for ref in refs:
    assert ref['image'] == symbols[ref['symbol']][0]
cred = {'copy_size': 0xa8, 'caps_offset': 0x30, 'caps_count': 3,
        'caps_value': caps[0], 'refs': refs,
        'size_evidence': [instruction(0x26b4c9c, 'mov w1, #0xa8'),
                          instruction(0x1812a4, 'mov w2, #0xa8')],
        'note': 'Static candidate only; no device or exploit validation.'}

futex_names = ['__arm64_sys_futex', 'do_futex', 'futex_wait_requeue_pi']
mcast_names = ['__arm64_sys_setsockopt', '__sys_setsockopt', 'sock_common_setsockopt', 'udp_setsockopt', 'ip_setsockopt']
all_frames = {name: frame(name) for name in futex_names + mcast_names + ['__arm64_sys_pselect6', 'core_sys_select']}
waiter_local = 0x90
waiter_depth = sum(all_frames[x] for x in futex_names) - waiter_local
instruction(0x291e54, 'add x27, sp, #0x90')
instruction(0x291e58, 'add x8, x27, #0x18')
mcast_depth = sum(all_frames[x] for x in mcast_names) - 0x20
mcast_offset = mcast_depth - waiter_depth
assert mcast_offset == 416 and waiter_depth == 448
instruction(0x14f0f68, 'add x0, sp, #0x20')
instruction(0x14f0f6c, 'mov w2, #0x108')
instruction(0x14f0f60, 'cmp w20, #0x108')
instruction(0x1e5308, 'stp x20, x19, [x22, #0x30]')
assert mcast_offset + 0x38 + 8 > 264

select_depth = all_frames['__arm64_sys_pselect6'] + all_frames['core_sys_select'] - 0x50
select_delta = select_depth - waiter_depth
assert select_delta == 80
select_task_word = (select_delta + 0x30) // 8
select_lock_word = (select_delta + 0x38) // 8
assert select_task_word == 16 and select_lock_word == 17
instruction(0x582c00, 'add x23, sp, #0x50')
instruction(0x582da4, 'cmp x23, x8')
instruction(0x582b68, 'cmp x26, #0x2b')
runtime_select = (ROOT / 'upstream-check/source/src/core/route/select_stack_route.h').read_text()
assert '#define PSELECT_ROUTE_NFDS 320' in runtime_select
assert 40 < 0x2b < 48  # 320 fds => 40 bytes per input set; increasing requires heap.

tcp_evidence = [instruction(0x150699c, 'cmp w26, #0x19'),
                instruction(0x1506a80, 'mov w20, #0x18'),
                instruction(0x1506a84, 'mov w26, w20'),
                instruction(0x15076b8, 'mov x2, x20')]

report = {
    'release': '5.10.101-android12-9-gece0cc04d1a5',
    'status': 'read-only static analysis, not an importable GhostLock profile',
    'upstream_commit': '67215cfe204940fe71ff67213e76329e1adce7d2',
    'kernel_sha256': hashlib.sha256(image).hexdigest(),
    'symbol_count': sum(len(x) for x in symbols.values()),
    'image_base': hex(base),
    'task_struct_candidates': task,
    'task_struct_not_derived': ['pid', 'tgid', 'tasks', 'atomic_flags'],
    'cred_candidate': cred,
    'primitive_pattern': {
        'current_read': instruction(0x1e5a40, 'mrs x20, sp_el0'),
        'clears_current_pi_blocked_on': instruction(0x1e5ac4, 'str xzr, [x20, #0x898]'),
        'note': 'Matches upstream static unpatched-pattern check; does not prove end-to-end exploitability.'},
    'stack_frames': all_frames,
    'routes': {
        'multicast_waiter': {'status': 'current IPv4 UDP copy window cannot reach the task/lock fields',
                             'buffer_depth': mcast_depth, 'waiter_depth': waiter_depth,
                             'waiter_offset': mcast_offset, 'copy_size': 264,
                             'task_field_offset': 0x30, 'lock_field_offset': 0x38,
                             'required_end': mcast_offset + 0x38 + 8},
        'select_stack': {'status': 'required task/lock words outside user copied input sets',
                         'buffer_depth': select_depth, 'waiter_depth': waiter_depth,
                         'delta_bytes': select_delta, 'raw_waiter_word': select_delta // 8,
                         'nfds': 320, 'input_words': 15,
                         'task_word': select_task_word, 'lock_word': select_lock_word},
        'tcp_zerocopy': {'status': 'current GhostLock payload fields not copied by this old kernel ABI',
                         'kernel_copy_max': 24, 'upstream_task_payload_offset': 40,
                         'upstream_lock_payload_offset': 48, 'evidence': tcp_evidence}},
    'limits': ['No BTF or original DWARF was recovered.', 'No code was installed or run on any phone.',
               'No working exploit profile was generated.', 'Alternate syscall paths or new runtime adaptations are not ruled out.']}
(OUT / 'static-offsets-and-route-audit.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps({'symbol_count': report['symbol_count'], 'task_fields': len(task),
                  'cred_copy_size': cred['copy_size'], 'cred_refs': len(refs),
                  'routes': report['routes']}, indent=2))
