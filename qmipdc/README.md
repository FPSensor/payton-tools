# qmipdc

Minimal QMI PDC (Persistent Device Configuration) client that talks to the
modem directly over QRTR (`AF_QIPCRTR`). No libqmi or vendor libraries are
needed, and it works with SELinux enforcing when run as root.

```
qmipdc list                    list software (MCFG) configs and the selected one
qmipdc info <id>               description/version of a loaded config
qmipdc load <file.mbn> <id>    load an MBN into the modem under <id>
qmipdc select <id>             mark <id> as the selected software config
qmipdc activate                activate the selected config (the modem restarts)
```

## Build

A static arm64 binary is in `prebuilt/`. To rebuild it:

```
aarch64-linux-gnu-gcc -static -O2 -Wall -o prebuilt/qmipdc qmipdc.c
```

## Use case: VoLTE with a carrier from another region

An X4 from one region (e.g. EMEA, XT1900-7) used with a carrier from another
region runs that region's generic `super.mbn`, which has IMS disabled, so
VoLTE never registers. This happens on any ROM, stock included.

The per-carrier MBNs live in `/vendor/fsg` (the fsg partition). Example for
Claro AR:

```
adb root
adb push prebuilt/qmipdc /data/local/tmp/
adb shell
cd /data/local/tmp
./qmipdc list
./qmipdc load /vendor/fsg/claro_arg.mbn claro_arg
./qmipdc select claro_arg
./qmipdc activate          # the modem restarts; check IMS status in *#*#4636#*#*
```

To go back to the region default:

```
./qmipdc select /readonly/vendor/fsg/payton/emea/customer/super.mbn
./qmipdc activate
```

The selection is stored in the modem EFS (modemst1/2, shared by both slots),
so it survives reboots and ROM changes. Erasing modemst resets it.
**Back up modemst1, modemst2, fsg_a, fsg_b and persist before touching it.**

## Notes

- `activate` on a config that is already active returns status 26
  (`QMI_ERR_NO_EFFECT`) and the modem reloads nothing. To force a reload,
  select and activate another config first, then switch back (both
  activates must return status 0).
- If IMS registers but VoLTE voice stays off after boot (the framework logs
  `MmTel Capabilities - [Voice: false ...]`), and `setprop ctl.restart
  vendor.qcrild` fixes it, the MBN is fine: qcrild started too early. On
  payton this was fixed by starting qcrild on `sys.boot_completed`
  (android_device_motorola_msm8998-common, "Start qcrild once boot has
  completed"). The modem restart caused by `activate` restarts qcrild too,
  which is why bouncing configs used to appear to help.

Message ids follow libqmi's `qmi-service-pdc.json`.
