#!/system/bin/sh
# One-line IMS/boot timing summary for the current boot (run as root).
up=$(cut -d' ' -f1 /proc/uptime)
q=$(dmesg | grep -m1 "starting service 'vendor.qcrild'" | sed 's/^\[ *\([0-9.]*\)\].*/\1/')
m=$(dmesg | grep -m1 "modem: Brought out of reset" | sed 's/^\[ *\([0-9.]*\)\].*/\1/')
s=$(dmesg | grep -m1 "modem's SSCTL" | sed 's/^\[ *\([0-9.]*\)\].*/\1/')
d=$(awk "BEGIN{ if (\"$q\" != \"\" && \"$s\" != \"\") printf \"%.2f\", $s - $q; else print \"?\" }")
r=$(logcat -d -b radio | grep -c 'registered=true')
last=$(logcat -d -b radio | grep 'setImsRegistrationState: {registered=' | tail -1 | sed 's/.*registered=\([a-z]*\).*/\1/')
v=$(logcat -d -b radio | grep 'MmTel Capabilities - \[Voice:' | tail -1 | sed 's/.*\[Voice: \([a-z]*\).*/\1/')
[ "$v" = true ] && ok=SI || ok=NO
echo "VOLTE=$ok voice=${v:-?} ims_reg=${last:-?} reg_count=$r qcrild=$q modem_reset=$m ssctl=$s delta=$d uptime=$up"
