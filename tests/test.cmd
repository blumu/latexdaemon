@echo off
::get the current directory in the variable cwd
for /f "delims=" %%i in ('cd') do set cwd=%%i

set DAEMONPATH=%cwd%\release\latexdaemon.exe

pushd %USERPROFILE%\documents\dphil\latex\Current\lmcs

:direct
timethis latex -interaction=nonstopmode --src-specials "\edef\TheAtCode{\the\catcode`\@} \catcode`\@=11 \newwrite\dependfile \openout\dependfile = lmcs.dep \ifx\TEXDAEMON@ORG@include\@undefined\let\TEXDAEMON@ORG@include\include\fi \ifx\TEXDAEMON@ORG@input\@undefined\let\TEXDAEMON@ORG@input\input\fi \def\include#1{\write\dependfile{#1}\TEXDAEMON@ORG@include{#1}} \def\TEXDAEMON@DumpDep@input#1{ \write\dependfile{#1}\TEXDAEMON@ORG@input #1} \def\TEXDAEMON@HookIgnoreFirst@input#1{ \let\input\TEXDAEMON@DumpDep@input } \def\input{\@ifnextchar\bgroup\TEXDAEMON@DumpDep@input\TEXDAEMON@ORG@input} \catcode`\@=\TheAtCode\relax \input lmcs.tex  \closeout\dependfile"

@echo.
@echo.
@echo.

:withdaemon
timethis "%DAEMONPATH%" -watch=no -force compile -filter=highlight lmcs.tex


:fin

popd
