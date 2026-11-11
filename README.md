# payton-tools

Debug and test tools written while porting LineageOS 22.2 with kernel 4.19
to the Moto X4 (payton). Most of them are generic enough for other
Qualcomm/Motorola devices of the same era (sdm630/636/660).

| Folder | What it is |
|---|---|
| [`qmipdc/`](qmipdc) | Minimal QMI PDC client over QRTR: list, load, select and activate modem MBN configs (carrier profiles, VoLTE) |
| [`imsboot/`](imsboot) | Windows batch that reboots the phone N times and records whether VoLTE (MMTEL voice) comes up on its own |
| [`snapshot/`](snapshot) | Bring-up snapshot scripts: system state collector, audio path dump, and a PowerShell wrapper that collects and uploads |

The phone-side scripts need root (`adb root` on a userdebug build).
Windows-side scripts expect to be run from the platform-tools folder,
next to `adb.exe`.
