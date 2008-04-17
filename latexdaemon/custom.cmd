@echo off
REM Instruction:
REM Launch latexdaemon with the following options:
REM  -afterjob=custom  -custom="\"C:\Path to this batch file\custom.cmd\""

echo Executing custom command:
echo -file name: %1
echo -base name: %~n1 
echo -extension: %~x1
dvipng %~n1.dvi