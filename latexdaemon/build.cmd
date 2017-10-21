:: make sure the current directory is set to the directory containing this batch file
@call %~dp0setenv.cmd
cd /d %0\..
%VSDEVENV% /Rebuild Release latexdaemon.sln
pause
