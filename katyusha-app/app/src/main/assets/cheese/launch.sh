#!/system/bin/sh
# katyusha launch.sh — the cheese native (libcheese.so) execs us as
# argv[1]="sh" argv[2]=this script, running as uid 0 with SELinux permissive.
# Everything below stays in the app's private dir; no partition writes.
cd "$(dirname "$0")"
umask 000
export ASH_STANDALONE=1
echo "[katyusha] root shell ready: uid=$(id -u 2>/dev/null)"
exec /system/bin/sh "$0" "$@"
