@echo off
del /q /s temp\
rem if not exist temp mkdir temp
mkdir temp\bin
copy "%VSCRT%\*" temp\bin\
copy Release\latexdaemon.exe temp\bin
copy custom.cmd temp\bin
mkdir temp\src
copy *.html temp\src
copy *.cpp temp\src
copy *.h temp\src
copy *.vcproj temp\src
copy *.sln temp\src
copy *.cmd temp\src
copy *.txt  temp\src
del latexdaemon.zip 
pushd temp
del ..\latexdaemon.zip
7za -tzip -r a ..\latexdaemon.zip .
popd
del /q /s temp\
rd /q /s temp

::7za -tzip a latexdaemon.zip *.html *.cpp *.h *.vcproj *.sln *.cmd *.txt Release\latexdaemon.exe
pause