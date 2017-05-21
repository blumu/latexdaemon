:: Package build output for publication
:: REQUIREMENT:
::   7zip needs to be installed with:
::    choco install 7zip.install
@echo off
:: change to directory containg this batch file
cd /d %0\..

setlocal
call getver.cmd

echo Zipping LaTeXDaemon version %APP_VERSION%
echo.

if EXIST temp rd /q /s temp
del builds\%PACKAGE_FILENAME%.zip 

mkdir temp
copy Release\latexdaemon.exe temp
copy custom.cmd temp
pushd temp
7z -tzip -r a ../builds/%PACKAGE_FILENAME%.zip .
popd

rd /q /s temp

endlocal