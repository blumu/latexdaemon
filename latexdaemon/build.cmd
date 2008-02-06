::set VSINSTALLDIR=D:\Microsoft Visual Studio 8\
:: make sure the current directory is set to the directory containing this batch file
cd /d %0\..
"%VSINSTALLDIR%\Common7\IDE\devenv.com" /Rebuild Release latexdaemon.vcproj
pause
