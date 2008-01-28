@echo off
set MSVC=C:\Program Files\Microsoft Visual Studio 9.0
rm -R temp\
rem if not exist temp mkdir temp
mkdir temp\bin
copy "%MSVC%\VC\redist\x86\Microsoft.VC90.CRT\*" temp\bin\
copy Release\latexdaemon.exe temp\bin
mkdir temp\src
cp *.html *.cpp *.h *.vcproj *.sln *.cmd *.txt  temp\src
del latexdaemon.zip 
pushd temp
7za -tzip -r a ..\latexdaemon.zip .
popd

::7za -tzip a latexdaemon.zip *.html *.cpp *.h *.vcproj *.sln *.cmd *.txt Release\latexdaemon.exe
pause