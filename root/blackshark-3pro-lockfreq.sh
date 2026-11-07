#!/system/bin/sh
# BlackShark 3 Pro (mobius, SM8250) benchmark freq lock - via Magisk su
# GPU: unlock JoyUI cap (lvl3=587M), pin level 0 = 905MHz
echo 0 > /sys/class/kgsl/kgsl-3d0/max_pwrlevel
echo 0 > /sys/class/kgsl/kgsl-3d0/min_pwrlevel
echo performance > /sys/class/kgsl/kgsl-3d0/devfreq/governor
echo 1 > /sys/class/kgsl/kgsl-3d0/force_clk_on
# CPU: performance governor pins each cluster at its max (JoyUI's power
# keeper resets scaling_min_freq, but cannot touch the governor)
for i in 0 1 2 3 4 5 6 7; do
  echo performance > /sys/devices/system/cpu/cpu$i/cpufreq/scaling_governor
done
echo "== GPU =="
cat /sys/class/kgsl/kgsl-3d0/max_pwrlevel /sys/class/kgsl/kgsl-3d0/min_pwrlevel /sys/class/kgsl/kgsl-3d0/devfreq/cur_freq
echo "== CPU =="
for i in 0 4 7; do
  cat /sys/devices/system/cpu/cpu$i/cpufreq/scaling_cur_freq
done
