@echo off
Set DAEMON=d:\documents\dphil\dev\latexdaemon\Release\latexdaemon.exe
echo Path to latexdaemon: %DAEMON%

pushd D:\documents\dphil\latex\Current\lmcs

:direct
timethis latex -interaction=nonstopmode --src-specials "\edef\TheAtCode{\the\catcode`\@} \catcode`\@=11 \newwrite\dependfile \openout\dependfile = lmcs.dep \ifx\TEXDAEMON@ORG@include\@undefined\let\TEXDAEMON@ORG@include\include\fi \ifx\TEXDAEMON@ORG@input\@undefined\let\TEXDAEMON@ORG@input\input\fi \def\include#1{\write\dependfile{#1}\TEXDAEMON@ORG@include{#1}} \def\TEXDAEMON@DumpDep@input#1{ \write\dependfile{#1}\TEXDAEMON@ORG@input #1} \def\TEXDAEMON@HookIgnoreFirst@input#1{ \let\input\TEXDAEMON@DumpDep@input } \def\input{\@ifnextchar\bgroup\TEXDAEMON@DumpDep@input\TEXDAEMON@ORG@input} \catcode`\@=\TheAtCode\relax \input lmcs.tex  \closeout\dependfile"

@echo.
@echo.
@echo.

:withdaemon
timethis %DAEMON% -watch=no -force compile -filter err lmcs.tex


:fin

popd
