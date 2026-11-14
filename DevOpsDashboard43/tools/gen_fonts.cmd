@echo off
rem Regenerate the LVGL fonts in ..\src\fonts from Inter 4.1 (SIL Open Font License 1.1).
rem Needs Node.js (npx) and internet access; Inter and lv_font_conv are fetched into %TEMP%.
rem   tools\gen_fonts.cmd
rem Text fonts: printable ASCII + degree sign + middle dot + en dash.
rem Number fonts (SemiBold) carry only the glyphs they show, to save flash.
setlocal
set WORK=%TEMP%\dod43-fonts
set ZIP=%WORK%\Inter-4.1.zip
set OUT=%~dp0..\src\fonts
set CONV=call npx --yes lv_font_conv@1.5.3 --bpp 4 --no-compress --format lvgl --lv-include lvgl.h
set TEXT=-r 0x20-0x7E,0xB0,0xB7,0x2013

if not exist "%WORK%" mkdir "%WORK%"
if not exist "%OUT%" mkdir "%OUT%"
if not exist "%ZIP%" powershell -NoProfile -Command "Invoke-WebRequest https://github.com/rsms/inter/releases/download/v4.1/Inter-4.1.zip -OutFile '%ZIP%'" || exit /b 1
powershell -NoProfile -Command "Add-Type -AssemblyName System.IO.Compression.FileSystem; $z = [IO.Compression.ZipFile]::OpenRead('%ZIP%'); foreach ($n in 'extras/ttf/Inter-Regular.ttf', 'extras/ttf/Inter-SemiBold.ttf', 'LICENSE.txt') { [IO.Compression.ZipFileExtensions]::ExtractToFile($z.GetEntry($n), (Join-Path '%WORK%' (Split-Path $n -Leaf)), $true) }; $z.Dispose()" || exit /b 1
set REG=Inter-Regular.ttf
set SEMI=Inter-SemiBold.ttf

rem Run in the work folder with relative names, so the "Opts:" line in the
rem generated files carries no local paths.
pushd "%WORK%"
%CONV% --size 20 --font %REG% %TEXT% -o inter_20.c || exit /b 1
%CONV% --size 24 --font %REG% %TEXT% -o inter_24.c || exit /b 1
%CONV% --size 32 --font %REG% %TEXT% -o inter_32.c || exit /b 1
rem Percent sign and digits - percentages on the ring
%CONV% --size 24 --font %SEMI% -r 0x25,0x30-0x39 -o inter_sb_24.c || exit /b 1
rem Minus and digits - total in the middle of the ring
%CONV% --size 72 --font %SEMI% -r 0x2D,0x30-0x39 -o inter_sb_72.c || exit /b 1
rem Minus, digits and colon - the clock
%CONV% --size 160 --font %SEMI% -r 0x2D,0x30-0x3A -o inter_sb_160.c || exit /b 1
popd

move /y "%WORK%\inter_*.c" "%OUT%" >nul || exit /b 1
copy /y "%WORK%\LICENSE.txt" "%OUT%\OFL.txt" >nul
echo Fonts written to %OUT%
