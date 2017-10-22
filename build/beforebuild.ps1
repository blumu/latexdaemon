#.SYNOPSIS
#    Patch version file with appveyor version numbers
if(-not $Env:APPVEYOR_BUILD_VERSION) {
    throw "Script need to run under appveyor"
}

$version = $Env:APPVEYOR_BUILD_VERSION -split '\.'

$content = Get-Content $PSScriptRoot\..\latexdaemon\version.h2
$content = $content | ForEach-Object {
    $_ `
        -replace '#define MAJOR_VERSION[ \t]*(.*)',"#define MAJOR_VERSION $($version[0])" `
        -replace '#define MINOR_VERSION[ \t]*(.*)',"#define MINOR_VERSION $($version[1])" `
        -replace '#define BUILD[ \t]*(.*)',"#define BUILD $($version[2])"
}
Write-Host "Patching version info:"
$content | Out-Host
$content | Set-Content -Path $PSScriptRoot\..\latexdaemon\version.h2


Write-Host "Patching chocolatey nuspec file"

$nuspec = Get-Content "$PSScriptRoot\..\choco\latexdaemon.nuspec" `
| ForEach-Object { $_ -replace '<version>.*</version>',"<version>$($Env:APPVEYOR_BUILD_VERSION)</version>" }
$nuspec | Set-Content -Path "$PSScriptRoot\..\choco\latexdaemon.nuspec" -Encoding UTF8
