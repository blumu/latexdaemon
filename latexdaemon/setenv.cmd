@echo off
set VSINSTALLDIR=C:\Program Files\Microsoft Visual Studio 9.0
set VSCRT=%VSINSTALLDIR%\VC\redist\x86\Microsoft.VC90.CRT
::set VSINSTALLDIR=D:\Microsoft Visual Studio 8
::set VSCRT=%VSINSTALLDIR%\VC\redist\x86\Microsoft.VC80.CRT"


set VSDEVENV="%VSINSTALLDIR%\Common7\IDE\devenv.com"
REM Uncomment one of the following line depending on which version of the Microsoft toolchain you are using
rem "C:\Program Files\Microsoft SDKs\Windows\v6.0\Bin\SetEnv.cmd"
@call "%VSINSTALLDIR%\VC\vcvarsall.bat"

set PROJFILE=latexdaemon.vcproj

set path=c:\cygwin\bin\;%path%

set WEBLOCAL=%USERPROFILE%\documents\website\
set WEBREMOTE=william@www.famille-blum.org:public_html


Title Latexdameon Build Environment