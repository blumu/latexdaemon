copy Release\latexdaemon.exe d:\tools
copy latexdaemon.zip d:\documents\website\software\latexdaemon
copy changelog-inc.html d:\documents\website\software\latexdaemon
pscp d:\documents\website\common\altsidebar.html william@www.famille-blum.org:public_html/common/
rem pscp latexdaemon.zip william@www.famille-blum.org:public_html/softwares/latexdaemon
pscp d:\documents\website\software\latexdaemon\* william@www.famille-blum.org:public_html/software/latexdaemon
pause