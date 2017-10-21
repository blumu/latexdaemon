@echo off
:: make sure the current directory is set to the directory containing this batch file
set VSINSTALLDIR=C:\Program Files (x86)\Microsoft Visual Studio\2017\Enterprise
set VSDEVENV="%VSINSTALLDIR%\Common7\IDE\devenv.com"
REM Uncomment one of the following line depending on which version of the Microsoft toolchain you are using
rem "C:\Program Files\Microsoft SDKs\Windows\v6.0\Bin\SetEnv.cmd"
REM @call "%VSINSTALLDIR%\VC\vcvarsall.bat"
cd /d %0\..
@echo on

%VSDEVENV% /Rebuild Release ..\latexdaemon.sln
pause
