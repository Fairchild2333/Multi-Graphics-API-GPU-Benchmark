#!/system/bin/sh
# katyusha launch.sh — invoked with root (uid 0) and SELinux permissive.
# Dynamically detects KMI and delegates KernelSU loading via ksud late-load.
cd "$(dirname "$0")"
umask 000
export ASH_STANDALONE=1

echo "[katyusha] root shell ready: uid=$(id -u 2>/dev/null)"
echo "[katyusha] SELinux: $(getenforce 2>/dev/null)"

# 1. Check if KernelSU is already in the kernel
if grep -q kernelsu /proc/modules 2>/dev/null; then
    echo "[katyusha] KernelSU module already loaded in kernel!"
else
    # 2. Locate or prepare ksud
    KSUD=""
    if [ -x "./ksud" ]; then
        KSUD="./ksud"
    elif [ -x "/data/adb/ksu/bin/ksud" ]; then
        KSUD="/data/adb/ksu/bin/ksud"
    fi

    # If still not found, search in installed KernelSU Manager APK packages
    if [ -z "$KSUD" ] || [ ! -x "$KSUD" ]; then
        MANAGER_KSUD=$(find /data/app -path '*/me.weishu.kernelsu*/lib/arm64/libksud.so' 2>/dev/null | head -1)
        if [ -n "$MANAGER_KSUD" ] && [ -f "$MANAGER_KSUD" ]; then
            cp "$MANAGER_KSUD" ./ksud 2>/dev/null
            chmod 755 ./ksud 2>/dev/null
            KSUD="./ksud"
            echo "[katyusha] Located ksud from KernelSU Manager: $MANAGER_KSUD"
        fi
    fi

    chmod 755 "$KSUD" 2>/dev/null

    # 3. Dynamic KMI auto-detection (supports 5.10, 5.15, 6.1, 6.6, etc.)
    KVER=$(uname -r | cut -d. -f1-2)
    AVER=$(uname -r | grep -o 'android[0-9]*' | head -1)
    if [ -n "$AVER" ] && [ -n "$KVER" ]; then
        KMI="${AVER}-${KVER}"
        echo "[katyusha] Detected KMI: $KMI (kernel $(uname -r))"
    else
        KMI=""
        echo "[katyusha] Detected kernel: $(uname -r)"
    fi

    # 4. Attempt dynamic late-load via ksud
    LOAD_OK=0
    if [ -n "$KSUD" ] && [ -x "$KSUD" ]; then
        echo "[katyusha] Invoking ksud late-load (auto-matching KernelSU LKM for $KMI)..."
        if [ -n "$KMI" ]; then
            "$KSUD" late-load --kmi "$KMI" --allow-shell 2>&1
        else
            "$KSUD" late-load --allow-shell 2>&1
        fi

        if grep -q kernelsu /proc/modules 2>/dev/null; then
            echo "[katyusha] [SUCCESS] ksud late-load successfully loaded KernelSU module!"
            LOAD_OK=1
        fi
    fi

    # 5. Fallback: If late-load didn't load and a local kernelsu.ko exists, try local insmod
    if [ "$LOAD_OK" -ne 1 ]; then
        TARGET_KO=""
        if [ -n "$KMI" ] && [ -f "./lkm-aarch64-${KMI}_kernelsu.ko" ]; then
            TARGET_KO="./lkm-aarch64-${KMI}_kernelsu.ko"
            echo "[katyusha] Found exact matching KMI module: $TARGET_KO"
        elif [ -f "./kernelsu.ko" ]; then
            TARGET_KO="./kernelsu.ko"
            echo "[katyusha] Using bundled default module: $TARGET_KO"
        fi

        if [ -n "$TARGET_KO" ] && [ -f "$TARGET_KO" ]; then
            echo "[katyusha] Attempting insmod with $TARGET_KO..."
            if [ -n "$KSUD" ] && [ -x "$KSUD" ]; then
                "$KSUD" insmod "$TARGET_KO" 2>&1
            else
                insmod "$TARGET_KO" 2>/dev/null
            fi

            if grep -q kernelsu /proc/modules 2>/dev/null; then
                echo "[katyusha] [SUCCESS] KernelSU injection succeeded with $TARGET_KO!"
                LOAD_OK=1
            fi
        fi
    fi
fi

# 6. Setup userspace components (/data/adb and ksud install)
if [ -x "./ksud" ]; then
    echo "[katyusha] Setting up /data/adb and ksud..."
    mkdir -p /data/adb/ksu/bin 2>/dev/null
    chmod 700 /data/adb 2>/dev/null
    cp ./ksud /data/adb/ksu/bin/ksud 2>/dev/null
    chmod 755 /data/adb/ksu/bin/ksud 2>/dev/null

    echo "[katyusha] Running 'ksud install'..."
    ./ksud install 2>&1

    # 6.5 Start the userspace su-session daemon.
    # Without it, adb-shell su works (kernel direct via --allow-shell) but every
    # APP-context su request dies in <10ms (no daemon endpoint to mediate the
    # session) — apps then fall back to the system installer / report no root.
    echo "[katyusha] Starting sulogd (userspace su-session daemon)..."
    "./ksud" debug sulogd >/dev/null 2>&1 &
    sleep 1
    if [ -f /data/adb/ksu/log/sulogd.lock ] || pgrep -f sulogd >/dev/null 2>&1; then
        echo "[katyusha] sulogd is up — app su should now work."
    else
        echo "[katyusha] [WARN] sulogd may not have started; app su could be denied."
    fi
fi

# 7. Final status report
if grep -q kernelsu /proc/modules 2>/dev/null; then
    echo ""
    echo "================================================="
    echo "[+] SUCCESS: KernelSU module is ACTIVE in kernel!"
    echo "[+] Open the KernelSU / KernelSU-NEXT App to manage permissions!"
    echo "================================================="
else
    echo ""
    echo "[-] Notice: KernelSU LKM not detected in /proc/modules."
    echo "[-] Temp root (uid 0 shell) is active in this process."
fi
