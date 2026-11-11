#!/system/bin/sh
# Snapshot of the audio path while something plays. Run as root.
# Only touches the cs47l35 (spi7.0) and cs35l35 (2-0040) regmaps: reading
# other devices' registers with their clocks off hangs the bus.
O=/data/local/tmp/snd
rm -rf $O; mkdir -p $O
echo "sleeping 8s, start playback now"
sleep 8
D=/sys/kernel/debug
cat /proc/asound/pcm > $O/pcm.txt 2>&1
for f in /proc/asound/card0/pcm*/sub0/status /proc/asound/card0/pcm*/sub0/hw_params; do
    echo "== $f"; cat $f
done > $O/pcm-status.txt 2>&1
cat $D/regmap/spi7.0/registers > $O/regmap-cs47l35.txt 2>&1
cat $D/regmap/2-0040/registers > $O/regmap-cs35l35.txt 2>&1
for d in $(find $D/asoc -type d -name dapm); do
    echo "=== $d"
    for w in $d/*; do
        l=$(head -n 1 $w)
        case "$l" in *": On "*) echo "${w##*/}: $l";; esac
    done
done > $O/dapm-on.txt 2>&1
tinymix > $O/tinymix.txt 2>&1
dmesg | tail -n 400 > $O/dmesg.txt
chmod -R a+rX $O
echo done
