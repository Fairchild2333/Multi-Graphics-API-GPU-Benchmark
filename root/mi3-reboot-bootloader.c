#include <sys/syscall.h>
#include <linux/reboot.h>
#include <unistd.h>
#include <stdio.h>
int main(void) {
    /* direct syscall: __NR_reboot(magic1, magic2, cmd, arg)
     * CMD_RESTART2 carries the reboot reason string to the bootloader.
     * Bypasses the MIUI reboot binary and init property checks. */
    long rc = syscall(__NR_reboot, (long)LINUX_REBOOT_MAGIC1,
                      (long)LINUX_REBOOT_MAGIC2,
                      (long)LINUX_REBOOT_CMD_RESTART2,
                      (long)"bootloader");
    printf("syscall returned %ld\n", rc);
    return (int)rc;
}
