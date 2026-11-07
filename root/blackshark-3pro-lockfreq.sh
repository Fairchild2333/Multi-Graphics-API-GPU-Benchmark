#!/system/bin/sh
# BlackShark 3 Pro (mobius, SM8250) benchmark freq lock - via Magisk su
# GPU: unlock JoyUI cap (lvl3=587M) and pin level 0 = 905MHz
echo 0 > /sys/class/kgsl/kgsl-3d0/max_pwrlevel
echo 0 > /sys/class/kgsl/kgsl-3d0/min_pwrlevel
echo performance > /sys/class/kgsl/kgsl-3d0/devfreq/governor
echo 1 > /sys/class/kgsl/kgsl-3d0/force_clk_on
# CPU: pin all clusters at max (little 1.8G, mid 2.42G, prime 2.84G)
for i in 0 1 2 3; do echo 1804800 > /sys/devices/system/cpu/cpu$i/cpufreq/scaling_min_freq; done
for i in 4 5 6; do echo 2419200 > /sys/devices/system/cpu/cpu$i/cpufreq/scaling_min_freq; done
echo 2841600 > /sys/devices/system/cpu/cpu7/cpufreq/scaling_min_freq
echo "== GPU =="
cat /sys/class/kgsl/kgsl-3d0/max_pwrlevel /sys/class/kgsl/kgsl-3d0/min_pwrlevel /sys/class/kgsl/kgsl-3d0/devfreq/cur_freq
echo "== CPU =="
for i in 0 4 7; do cat /sys/devices/system/cpu/cpu$i/cpufreq/scaling_cur_freq; done
