#.SYNOPSIS
#    Patch version file with target version number
param($releaseVersion)

if ($releaseVersion) {
    Write-Output "Using provided version number: $releaseVersion"
} elseif ($Env:APPVEYOR_BUILD_VERSION) {
    $releaseVersion = $Env:APPVEYOR_BUILD_VERSION
    Write-Output "Using version number set by appveyor: $releaseVersion"
} else {
    throw "Script need to run under appveyor or version must be specified as parameter"
}

if ($releaseVersion -match '^\d+.\d+.\d+$') {
    $tag = ''
} else {
    $tag = "-$releaseVersion"
    $releaseVersion = "0.0.0"
}

$version = $releaseVersion -split '\.'

$content = Get-Content $PSScriptRoot\..\latexdaemon\version.h2
$content = $content | ForEach-Object {
    $_ `
        -replace '#define MAJOR_VERSION[ \t]*(.*)',"#define MAJOR_VERSION $($version[0])" `
        -replace '#define MINOR_VERSION[ \t]*(.*)',"#define MINOR_VERSION $($version[1])" `
        -replace '#define BUILD[ \t]*(.*)',"#define BUILD $($version[2])" `
        -replace '#define TAG[ \t]*(.*)', "#define TAG $($tag)"
}
Write-Host "Patching version info:"
$content | Out-Host
$content | Set-Content -Path $PSScriptRoot\..\latexdaemon\version.h2


Write-Host "Patching chocolatey nuspec file"

$nuspec = Get-Content "$PSScriptRoot\..\choco\latexdaemon.nuspec" `
| ForEach-Object { $_ -replace '<version>.*</version>', "<version>$($releaseVersion)</version>" }
$nuspec | Set-Content -Path "$PSScriptRoot\..\choco\latexdaemon.nuspec" -Encoding UTF8
