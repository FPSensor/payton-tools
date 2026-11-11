# Usage (from the platform-tools folder):
#   .\payton.ps1 F        fast snapshot (no stack traces), a few seconds
#   .\payton.ps1 A        snapshot only, nothing is changed
#   .\payton.ps1 nosens   sensors experiment (undone by a reboot)
#   .\payton.ps1 B        snapshot after the experiment
#   .\payton.ps1 pstore   from TWRP: pull pstore and the persist bootlog
param([string]$Step = "A")
$ErrorActionPreference = "Continue"
$Server = "user@your-build-server:/path/to/payton-logs/"
$Stamp = Get-Date -Format "HHmmss"

if ($Step -eq "pstore") {
    $Out = "pstore-$Stamp"
    Write-Host "[1/4] waiting for adb (TWRP)..."
    ./adb wait-for-device
    Write-Host "[2/4] pulling pstore"
    ./adb pull /sys/fs/pstore/ $Out
    Write-Host "[3/4] pulling /persist/bootlog"
    ./adb shell "mount /persist 2>/dev/null; true"
    ./adb pull /persist/bootlog "$Out\bootlog"
    Write-Host "[4/4] uploading (type the server password)"
    scp -r $Out $Server
    Write-Host "Uploaded $Out"
    exit
}

Write-Host "[1/5] waiting for adb..."
./adb wait-for-device
Write-Host "[2/5] adb root"
./adb root
Start-Sleep -Seconds 2
./adb wait-for-device
Write-Host "[3/5] running collect.sh $Step on the phone (up to ~2 min)"
./adb push collect.sh /data/local/tmp/collect.sh
if ($Step -eq "qrtr") {
    ./adb push qrtr-ns /data/local/tmp/qrtr-ns
    ./adb push qrtr-lookup /data/local/tmp/qrtr-lookup
}
./adb shell "sh /data/local/tmp/collect.sh $Step"
if ($Step -eq "nosens" -or $Step -eq "qrtr") {
    Write-Host "Done. Watch the screen, wait about 2 minutes, then run: .\payton.ps1 B"
    exit
}
$Out = "logs$Step-$Stamp"
Write-Host "[4/5] pulling"
./adb pull "/data/local/tmp/$Step" $Out
Write-Host "[5/5] uploading (type the server password)"
scp -r $Out $Server
Write-Host "Uploaded $Out"
