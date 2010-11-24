@echo off
:: change to directory containg this batch file
cd /d %0\..

setlocal
call getver.cmd

echo Publishing LaTeXDaemon version %APP_VERSION%
echo. 


copy builds\%PACKAGE_FILENAME%.zip %WEBLOCAL%\software\latexdaemon\latexdaemon.zip
copy changelog-inc.html %WEBLOCAL%\software\latexdaemon\
if [%WEBREMOTE%] NEQ [] (
	pscp %WEBLOCAL%\software\latexdaemon\* %WEBREMOTE%/software/latexdaemon
	::pscp builds\%PACKAGE_FILENAME%.zip %WEBLOCAL%/software/latexdaemon/latexdaemon.zip
	::pscp changelog-inc.html %WEBREMOTE%/software/latexdaemon
)

endlocal
pause

