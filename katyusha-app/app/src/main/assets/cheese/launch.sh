#!/system/bin/sh
# katyusha launch.sh — invoked with root (uid 0) and SELinux permissive.
# Everything below stays in the app's private dir or sets up KernelSU.
cd "$(dirname "$0")"
umask 000
export ASH_STANDALONE=1

echo "[katyusha] root shell ready: uid=$(id -u 2>/dev/null)"
echo "[katyusha] SELinux: $(getenforce 2>/dev/null)"

# 1. Check if KernelSU is already in the kernel
if grep -q kernelsu /proc/modules 2>/dev/null; then
    echo "[katyusha] KernelSU module already loaded in kernel!"
else
    # 2. Attempt to load kernelsu.ko via direct insmod or ksud
    if [ -f "./kernelsu.ko" ]; then
        echo "[katyusha] Attempting to load kernelsu.ko into kernel..."
        
        # Try direct insmod first
        insmod ./kernelsu.ko 2>/dev/null
        
        if grep -q kernelsu /proc/modules 2>/dev/null; then
            echo "[katyusha] [SUCCESS] Direct insmod kernelsu.ko succeeded!"
        elif [ -x "./ksud" ]; then
            chmod 755 ./ksud 2>/dev/null
            echo "[katyusha] Direct insmod returned $?; trying 'ksud insmod ./kernelsu.ko'..."
            ./ksud insmod ./kernelsu.ko 2>&1
            
            if ! grep -q kernelsu /proc/modules 2>/dev/null; then
                echo "[katyusha] Trying 'ksud late-load --kmi android12-5.10'..."
                ./ksud late-load --kmi android12-5.10 --allow-shell 2>&1
            fi
        fi
    else
        echo "[katyusha] kernelsu.ko not found in $(pwd)"
    fi
fi

# 3. If ksud is present, setup userspace components
if [ -x "./ksud" ]; then
    echo "[katyusha] Setting up /data/adb and ksud..."
    mkdir -p /data/adb/ksu/bin 2>/dev/null
    chmod 700 /data/adb 2>/dev/null
    cp ./ksud /data/adb/ksu/bin/ksud 2>/dev/null
    chmod 755 /data/adb/ksu/bin/ksud 2>/dev/null
    
    # Run ksud install to set up su binary and environment
    echo "[katyusha] Running 'ksud install'..."
    ./ksud install 2>&1
fi

# 4. Status report
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
