
LaTexDaemon
===========

LaTeXDaemon speeds-up compilation of LaTeX documents.

Description
-----------

LaTeXDaemon offers two main advantages over the regular LaTeX processing workflow. First it runs
silenlty in the background and automatically recompiles your document whenever it is changed. This way you can concentrate on editing
your document and avoid back and forth trips between your editor and the LaTeX command prompt. 
Secondly it significantly accelerates the overall compilation time of your document by pre-compiling the preamble: the section
of your LaTeX source that loads external packages with \usepackage{...}.

The tool works by silently monitoring your documents using efficient OS-based file system notifications. All your document files are monitored including dependencies such as
auxiliary .tex files or .gif/.pdf graphics. The tool uses very little OS resources and only triggers
a LaTeX recompilation when necessary. In particular preamble compilation only occurs if the preamble part
of the .tex document is altered. 

Build status
------------
[![Build status](https://ci.appveyor.com/api/projects/status/53ft206wrgt2v5gw?svg=true)](https://ci.appveyor.com/project/blumu/latexdaemon)

Download
--------

https://github.com/blumu/latexdaemon/releases

Home page
---------

http://william.famille-blum.org/software/latexdaemon/
