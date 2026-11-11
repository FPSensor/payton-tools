# snapshot

Scripts used to collect the state of the phone during bring-up.

## collect.sh (phone, root)

```
sh collect.sh A        snapshot of the current state, nothing is changed
sh collect.sh nosens   empty the sensors multihal config and restart the
                       HAL and system_server (temporary, undone by a reboot)
sh collect.sh B        snapshot after the nosens experiment
sh collect.sh qrtr     start the QRTR name service by hand and restart the
                       sensors HAL (needs qrtr-ns and qrtr-lookup binaries
                       from linux-msm/qrtr in /data/local/tmp; not included)
```

Results go to `/data/local/tmp/<step>`.

## payton.ps1 (Windows, from platform-tools)

Wraps `collect.sh`: adb root, push, run, pull, and upload with `scp` to the
build server (edit `$Server` at the top).

```
.\payton.ps1 A         snapshot
.\payton.ps1 F         fast snapshot (no stack traces)
.\payton.ps1 nosens    sensors experiment, then run .\payton.ps1 B
.\payton.ps1 pstore    from TWRP: pull pstore and the persist bootlog
                       (see the payton-bootlog repo)
```

## snd-dump.sh (phone, root)

Snapshot of the audio path while something plays: PCM status, the cs47l35
and cs35l35 regmaps, the DAPM widgets that are on, tinymix and dmesg.
Start it, start playback within 8 s, and pull `/data/local/tmp/snd`.
It only reads the Cirrus codec regmaps on purpose: reading other devices'
registers with their clocks off hangs the bus.
