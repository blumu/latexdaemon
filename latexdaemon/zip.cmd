@echo off

setlocal
call getver.cmd

echo Zipping version %APP_VERSION_FLAT%
echo.

:: change to directory containg this batch file
cd /d %0\..

del /q /s temp\
rem if not exist temp mkdir temp
mkdir temp
copy "%VSCRT%\*" temp
copy Release\latexdaemon.exe temp
copy custom.cmd temp
goto zip
:prepare_sources
mkdir temp\src
copy *.html temp\src
copy *.cpp temp\src
copy *.h temp\src
copy *.vcproj temp\src
copy *.sln temp\src
copy *.cmd temp\src
copy *.txt  temp\src
:zip
del %PACKAGE_FILENAME%.zip 
pushd temp
7za -tzip -r a ../%PACKAGE_FILENAME%.zip .
popd
del /q /s temp\
rd /q /s temp

endlocal

::7za -tzip a latexdaemon.zip *.html *.cpp *.h *.vcproj *.sln *.cmd *.txt Release\latexdaemon.exe
