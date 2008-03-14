@call setenv.cmd
:: make sure the current directory is set to the directory containing this batch file
cd /d %0\..
::%VSDEVENV% /Rebuild Release latexdaemon-vc80.vcproj
%VSDEVENV% /Rebuild Release latexdaemon.vcproj
pause
