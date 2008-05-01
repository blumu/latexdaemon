:: make sure the current directory is set to the directory containing this batch file
cd /d %0\..
%VSDEVENV% /Rebuild Release %PROJFILE%
pause
