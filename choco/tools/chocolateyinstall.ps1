$ErrorActionPreference = 'Stop'
$packageName = 'latexdaemon'
$target = "$($env:ChocolateyInstall)\bin\$packageName.exe"
Install-ChocolateyShortcut -shortcutFilePath "$($env:USERPROFILE)\Desktop\LaTeXDaemon.lnk" -targetPath "$target" -description "LaTeX Daemon"
