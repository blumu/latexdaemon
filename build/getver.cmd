:: Requirement: GNU sed tool
:: Sed comes installed with Git
SET sed="C:\Program Files\Git\usr\bin\sed.exe"
set VERSIONFILE=%~dp0..\LatexDaemon\version.h2

:: pattern used for version info extraction
set MAJOR_PATTERN=s/\#define\s*MAJOR_VERSION\s*\(.*\)/\1/p
set MINOR_PATTERN=s/\#define\s*MINOR_VERSION\s*\(.*\)/\1/p
set BUILD_PATTERN=s/\#define\s*BUILD\s*\(.*\)/\1/p

:: get the version number from the header version file
%sed% -n '%MAJOR_PATTERN%' %VERSIONFILE% > tmp.tmp
set /p MAJOR_VERSION= < tmp.tmp

%sed% -n '%MINOR_PATTERN%' %VERSIONFILE% > tmp.tmp
set /p MINOR_VERSION= < tmp.tmp

%sed% -n '%BUILD_PATTERN%' %VERSIONFILE% > tmp.tmp
set /p BUILD= < tmp.tmp
del tmp.tmp

:::::::::::::::
:: Return extracted version numbers in env variables

:: concataneted version info
set APP_VERSION=%MAJOR_VERSION%.%MINOR_VERSION%.%BUILD%
:: concataneted version info without dots
set APP_VERSION_FLAT=%MAJOR_VERSION%%MINOR_VERSION%%BUILD%
:: package file name
set PACKAGE_FILENAME=LatexDaemon.%APP_VERSION%