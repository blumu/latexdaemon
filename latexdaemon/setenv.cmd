@echo off
set HERE=~dp0
set VSINSTALLDIR=C:\Program Files (x86)\Microsoft Visual Studio 10.0
set VSCRT=%VSINSTALLDIR%\VC\redist\x86\Microsoft.VC100.CRT
::set VSCRT=%HERE%\Release\x86_microsoft.vc90.crt_1fc8b3b9a1e18e3b_9.0.21022.8_none_bcb86ed6ac711f91


set VSDEVENV="%VSINSTALLDIR%\Common7\IDE\devenv.com"
REM Uncomment one of the following line depending on which version of the Microsoft toolchain you are using
rem "C:\Program Files\Microsoft SDKs\Windows\v6.0\Bin\SetEnv.cmd"
@call "%VSINSTALLDIR%\VC\vcvarsall.bat"

set PROJFILE=latexdaemon.vcxproj

set path=c:\cygwin\bin\;%path%

if exist setenv_local.cmd (
call setenv_local.cmd
) else (
set WEBLOCAL=%TEMP%\Website\
set WEBREMOTE=
)

Title Latexdameon Build Environment