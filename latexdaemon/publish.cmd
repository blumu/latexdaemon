copy latexdaemon.zip %USERPROFILE%\documents\website\software\latexdaemon
copy changelog-inc.html %USERPROFILE%\documents\website\software\latexdaemon
pscp %USERPROFILE%\documents\website\software\latexdaemon\* william@www.famille-blum.org:public_html/software/latexdaemon
pause