@echo off
set VSINSTALLDIR=C:\Program Files\Microsoft Visual Studio 9.0
::set VSCRT=%VSINSTALLDIR%\VC\redist\x86\Microsoft.VC90.CRT
set VSCRT=C:\Users\William\Documents\latexdaemon\Release\x86_microsoft.vc90.crt_1fc8b3b9a1e18e3b_9.0.21022.8_none_bcb86ed6ac711f91
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