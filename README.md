
LaTexDaemon
===========

LaTeXDaemon speeds-up compilation of LaTeX and TeX documents.

Why should I use it?
--------------------
LaTeXDaemon simplifies and accelerate your LaTeX processing workflow. It runs silently in the background and automatically recompiles your document when a change is detected, so you can focus on editing your document using your favorite file editor while avoiding round trips with the command prompt. Additionally, it speeds up compilation by pre-compiling the preamble section of your LaTeX source that loads packages (`\usepackage{...}`).

How does it work?
-----------------

The tool works by silently monitoring your documents using efficient OS-based file system notifications. All your document files are monitored including dependencies such as auxiliary `.tex` files, `.gif`, `.pdf` or other graphics. It uses very little OS resources and only triggers a LaTeX recompilation when necessary. In particular, preamble compilation only occurs if the preamble section of the .tex document has actually been touched. Changes to the main section of your document only trigger a quick re-compilation.

Installation
------------

Via [chocolatey](https://chocolatey.org/)

```batch
choco install latexdaemon
```

Wiki
----

The [wiki](https://github.com/blumu/latexdaemon/wiki)
 contains more info, samples and documentation.

Build status
------------
Last build (master): [![Last build (master)](https://ci.appveyor.com/api/projects/status/53ft206wrgt2v5gw/branch/master?svg=true)](https://ci.appveyor.com/project/blumu/latexdaemon/branch/master)

Latest build (any branch): [![Latest build (any branch)](https://ci.appveyor.com/api/projects/status/53ft206wrgt2v5gw?svg=true)](https://ci.appveyor.com/project/blumu/latexdaemon)

Download
--------

https://github.com/blumu/latexdaemon/releases

Home page
---------

http://william.famille-blum.org/software/latexdaemon/
