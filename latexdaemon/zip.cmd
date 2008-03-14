@echo off
@call setenv.cmd
del /q /s temp\
rem if not exist temp mkdir temp
mkdir temp\bin
copy "%VSINSTALLDIR%\VC\redist\x86\Microsoft.VC90.CRT\*" temp\bin\
copy Release\latexdaemon.exe temp\bin
mkdir temp\src
copy *.html *.cpp *.h *.vcproj *.sln *.cmd *.txt  temp\src
del latexdaemon.zip 
pushd temp
7za -tzip -r a ..\latexdaemon.zip .
popd
del /q /s temp\
rd /q /s temp

::7za -tzip a latexdaemon.zip *.html *.cpp *.h *.vcproj *.sln *.cmd *.txt Release\latexdaemon.exe
pause