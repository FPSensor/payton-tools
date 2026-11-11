# imsboot

Repeated-boot test for VoLTE. `imsboot.bat` reboots the phone N times and,
for every boot, records whether IMS came up with MMTEL voice (VoLTE) on its
own, plus the boot timing of qcrild and the modem.

## Use

Put `imsboot.bat` and `imscheck.sh` in the platform-tools folder (next to
`adb.exe`) and run:

```
.\imsboot.bat        # 10 boots
.\imsboot.bat 5      # 5 boots
```

Each boot takes about 2–3 minutes: reboot, wait for `sys.boot_completed`,
`adb root`, wait 90 s, check. The script also turns off
`persist.vendor.radio.adb_log_on` so the radio log buffer doesn't rotate
before the check.

Results go to `imsboot\`:

- `summary.txt`: one line per boot, for example
  `boot 3: VOLTE=NO voice=false ims_reg=true reg_count=1 qcrild=9.69 modem_reset=13.43 ssctl=16.02 delta=6.33 uptime=...`
- `bootN-dmesg.txt` and `bootN-logcat.txt`: full logs of every boot.

## Fields

| Field | Meaning |
|---|---|
| `VOLTE` | `SI` if the last `MmTel Capabilities - [Voice: ...]` line says `true`. This is the criterion that matters |
| `voice` | That last Voice value |
| `ims_reg` | Last `setImsRegistrationState` value. IMS can be registered without voice, so don't use this alone |
| `qcrild`, `modem_reset`, `ssctl` | dmesg times (s) when qcrild started, the modem left reset, and its SSCTL QMI service appeared |
| `delta` | `ssctl - qcrild` |

`imscheck.sh` can also be run by hand on the phone as root:
`sh /data/local/tmp/imscheck.sh`.
