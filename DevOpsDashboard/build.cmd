@echo off
rem Build (and optionally upload) DevOpsDashboard with the Arduino IDE's bundled arduino-cli.
rem   build.cmd            - compile only
rem   build.cmd COM3       - compile and upload to COM3
setlocal
set CLI=%LOCALAPPDATA%\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe
where arduino-cli >nul 2>nul && set CLI=arduino-cli
set FQBN=esp32:esp32:esp32s3:PSRAM=opi,FlashSize=16M,PartitionScheme=custom,CDCOnBoot=cdc,FlashMode=qio
set OUT=%~dp0build
"%CLI%" compile --fqbn %FQBN% --build-path "%OUT%" "%~dp0." || exit /b 1
if not "%1"=="" "%CLI%" upload -p %1 --fqbn %FQBN% --input-dir "%OUT%" "%~dp0."
