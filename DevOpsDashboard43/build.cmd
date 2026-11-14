@echo off
rem Build (and optionally upload) DevOpsDashboard43 with the Arduino IDE's bundled arduino-cli.
rem   build.cmd            - compile only
rem   build.cmd COM9       - compile and upload (the "USB TO UART" connector)
rem The core and library versions come from the "default" profile in sketch.yaml.
setlocal
set CLI=%LOCALAPPDATA%\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe
where arduino-cli >nul 2>nul && set CLI=arduino-cli
set OUT=%~dp0build
"%CLI%" compile --profile default --build-path "%OUT%" "%~dp0." || exit /b 1
if not "%1"=="" "%CLI%" upload --profile default -p %1 --input-dir "%OUT%" "%~dp0."
