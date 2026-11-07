#!/system/bin/sh
# BlackShark 3 Pro (mobius) - restore JoyUI daily-use frequency defaults
# GPU: back to JoyUI cap (level 3 = 587MHz) and msm-adreno-tz
echo msm-adreno-tz > /sys/class/kgsl/kgsl-3d0/devfreq/governor
echo 3 > /sys/class/kgsl/kgsl-3d0/max_pwrlevel
echo 8 > /sys/class/kgsl/kgsl-3d0/min_pwrlevel
echo 0 > /sys/class/kgsl/kgsl-3d0/force_clk_on
# CPU: back to schedutil
for i in 0 1 2 3 4 5 6 7; do
  echo schedutil > /sys/devices/system/cpu/cpu$i/cpufreq/scaling_governor
done
echo "== GPU =="
cat /sys/class/kgsl/kgsl-3d0/max_pwrlevel /sys/class/kgsl/kgsl-3d0/min_pwrlevel /sys/class/kgsl/kgsl-3d0/devfreq/cur_freq
echo "== CPU =="
for i in 0 4 7; do
  echo -n "cpu$i: "
  cat /sys/devices/system/cpu/cpu$i/cpufreq/scaling_governor /sys/devices/system/cpu/cpu$i/cpufreq/scaling_cur_freq
done
