@echo off
setlocal enabledelayedexpansion
rem Reboots the phone N times (default 10) and records whether IMS registers.
rem Put this .bat and imscheck.sh in the platform-tools folder.
set N=10
if not "%~1"=="" set N=%~1
if not exist imscheck.sh (
  echo Falta imscheck.sh en esta carpeta. Bajalo con scp y volve a correr.
  exit /b 1
)
if not exist imsboot mkdir imsboot
.\adb root
timeout /t 3 /nobreak >nul
.\adb wait-for-device
.\adb shell setprop persist.vendor.radio.adb_log_on 0
.\adb push imscheck.sh /data/local/tmp/imscheck.sh || exit /b 1
echo ==== %DATE% %TIME% ====>>imsboot\summary.txt
for /L %%i in (1,1,%N%) do call :run %%i
echo.
echo ===== RESUMEN =====
type imsboot\summary.txt
goto :eof

:run
echo.
echo === Boot %1 de %N% ===
.\adb reboot
timeout /t 20 /nobreak >nul
.\adb wait-for-device
:waitboot
set BC=
for /f "delims=" %%a in ('.\adb shell getprop sys.boot_completed 2^>nul') do set BC=%%a
if not "!BC!"=="1" (
  timeout /t 3 /nobreak >nul
  goto waitboot
)
.\adb root >nul
timeout /t 3 /nobreak >nul
.\adb wait-for-device
echo Booteado, esperando 90 s a que registre IMS...
timeout /t 90 /nobreak >nul
.\adb shell sh /data/local/tmp/imscheck.sh > imsboot\tmp.txt
set RES=
set /p RES=<imsboot\tmp.txt
echo boot %1: !RES!
echo boot %1: !RES!>>imsboot\summary.txt
.\adb shell dmesg > imsboot\boot%1-dmesg.txt
.\adb logcat -d -b all > imsboot\boot%1-logcat.txt
goto :eof
