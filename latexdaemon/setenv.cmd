@echo off

set VSINSTALLDIR=C:\Program Files\Microsoft Visual Studio 9.0
::set VSDEVENV=%VSINSTALLDIR%\Common7\IDE\devenv.com
set VSDEVENV="%VSINSTALLDIR%\Common7\IDE\devenv.com"

REM Uncomment one of the following line depending on which version of the Microsoft toolchain you are using
rem "C:\Program Files\Microsoft SDKs\Windows\v6.0\Bin\SetEnv.cmd"
@call "C:\Program Files\Microsoft Visual Studio 9.0\VC\vcvarsall.bat"

Title Cracklock Build Environment