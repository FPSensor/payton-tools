#!/system/bin/sh
# Runs on the phone as root. Usage: sh collect.sh A|B|nosens
# A      : snapshot of the current state, nothing is changed
# nosens : empty the sensors multihal config, restart the HAL and
#          system_server (temporary, undone by a reboot)
# B      : snapshot after the nosens experiment
set -u
case "${1:-A}" in
qrtr)
    # Start the QRTR name service by hand, then restart the QMI clients
    # that are already stuck in their lookup.
    chmod 755 /data/local/tmp/qrtr-ns /data/local/tmp/qrtr-lookup
    pkill -f qrtr-ns
    /data/local/tmp/qrtr-ns -f > /data/local/tmp/qrtr-ns.log 2>&1 &
    sleep 3
    echo "--- qrtr-lookup after ns start"
    /data/local/tmp/qrtr-lookup | head -40
    for s in vendor.sensors.qti vendor.sensors-hal-1-0; do stop $s; done
    for s in vendor.sensors.qti vendor.sensors-hal-1-0; do start $s; done
    sleep 5
    /data/local/tmp/qrtr-lookup > /data/local/tmp/qrtr-lookup.txt 2>&1
    kill $(pidof system_server)
    echo "qrtr applied"
    exit 0
    ;;
nosens)
    : > /data/local/tmp/hals.conf
    chmod 644 /data/local/tmp/hals.conf
    mount -o bind /data/local/tmp/hals.conf /vendor/etc/sensors/hals.conf
    stop vendor.sensors-hal-1-0
    start vendor.sensors-hal-1-0
    sleep 3
    kill $(pidof system_server)
    echo "nosens applied"
    exit 0
    ;;
esac

O=/data/local/tmp/$1
rm -rf $O; mkdir -p $O; cd $O || exit 1

if [ "$1" = "F" ]; then
    # Fast snapshot: no debuggerd, a few seconds at most.
    logcat -d -b all > logcat.txt 2>&1
    dmesg > dmesg.txt 2>&1
    getprop > props.txt
    ps -A -T -o PID,TID,S,WCHAN,PCPU,NAME > ps.txt
    timeout 5 dumpsys SurfaceFlinger > sf.txt 2>&1
    echo done > status.txt
    echo "collected $O"
    exit 0
fi
H=$(pidof android.hardware.sensors@1.0-service)
Q=$(pidof sensors.qti)
S=$(pidof system_server)
echo "hal=$H qti=$Q ss=$S" > pids.txt
echo "[collect] native traces"
for p in $H $Q; do timeout 20 debuggerd -b $p > native-$p.txt 2>&1; done
echo "[collect] java traces"
for p in $S; do timeout 20 debuggerd -j $p > ss_java-$p.txt 2>&1; done
for p in $H $Q $S $(pidof surfaceflinger); do
    for t in /proc/$p/task/*; do
        echo "== $t $(cat $t/comm 2>/dev/null)"
        cat $t/stack 2>/dev/null
    done
done > kstacks.txt
echo "[collect] logcat, dmesg, props"
logcat -d -b all > logcat.txt
dmesg > dmesg.txt
getprop > props.txt
ps -A -T -o PID,TID,S,WCHAN,PCPU,NAME > ps.txt
cat /proc/net/protocols > protocols.txt
cat /proc/consoles > consoles.txt
ls -l /dev > devs.txt
timeout 15 dumpsys SurfaceFlinger > sf.txt 2>&1
echo done > status.txt
echo "collected $O"
