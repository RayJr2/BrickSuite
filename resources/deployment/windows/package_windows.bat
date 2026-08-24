@echo off
setlocal EnableExtensions

rem ================================================================
rem BrickSuite Windows Packaging
rem
rem Usage:
rem   package_windows.bat "C:\path\to\Release-build-directory"
rem
rem If no argument is supplied, the current directory is used when
rem it contains BrickSuite.exe and CMakeCache.txt.
rem ================================================================

echo.
echo ============================================================
echo BrickSuite Windows Packaging
echo ============================================================
echo.

rem ----------------------------------------------------------------
rem Determine build directory
rem ----------------------------------------------------------------
if not "%~1"=="" (
    set "BUILD_DIR=%~f1"
) else (
    set "BUILD_DIR=%CD%"
)

echo Build directory:
echo   %BUILD_DIR%
echo.

rem ----------------------------------------------------------------
rem Validate Release build
rem ----------------------------------------------------------------
if not exist "%BUILD_DIR%\BrickSuite.exe" (
    echo ERROR: BrickSuite.exe was not found:
    echo   %BUILD_DIR%\BrickSuite.exe
    echo.
    echo Usage:
    echo   package_windows.bat "C:\path\to\Release-build-directory"
    exit /b 1
)

if not exist "%BUILD_DIR%\CMakeCache.txt" (
    echo ERROR: CMakeCache.txt was not found:
    echo   %BUILD_DIR%\CMakeCache.txt
    exit /b 1
)

rem ----------------------------------------------------------------
rem Discover Qt installation from CMakeCache.txt
rem ----------------------------------------------------------------
set "QT6_DIR="

for /f "tokens=1,* delims==" %%A in ('findstr /B /C:"Qt6_DIR:PATH=" "%BUILD_DIR%\CMakeCache.txt"') do (
    set "QT6_DIR=%%B"
)

if not defined QT6_DIR (
    echo ERROR: Qt6_DIR could not be found in CMakeCache.txt.
    exit /b 1
)

rem Qt6_DIR normally looks like:
rem C:/Qt/6.10.3/mingw_64/lib/cmake/Qt6
rem Move up three directories to reach mingw_64.
for %%I in ("%QT6_DIR%\..\..\..") do set "QT_ROOT=%%~fI"

set "WINDEPLOYQT=%QT_ROOT%\bin\windeployqt.exe"

echo Qt installation:
echo   %QT_ROOT%
echo.

if not exist "%WINDEPLOYQT%" (
    echo ERROR: windeployqt.exe was not found:
    echo   %WINDEPLOYQT%
    exit /b 1
)

echo windeployqt:
echo   %WINDEPLOYQT%
echo.

rem ----------------------------------------------------------------
rem Create clean staging directory
rem ----------------------------------------------------------------
set "DEPLOY_ROOT=%BUILD_DIR%\deploy"
set "STAGE_DIR=%DEPLOY_ROOT%\BrickSuite"

echo Cleaning staging directory...
if exist "%STAGE_DIR%" (
    rmdir /S /Q "%STAGE_DIR%"
)

mkdir "%STAGE_DIR%"

if errorlevel 1 (
    echo ERROR: Unable to create staging directory.
    exit /b 1
)

rem ----------------------------------------------------------------
rem Copy BrickSuite executable
rem ----------------------------------------------------------------
echo Copying BrickSuite.exe...

copy /Y "%BUILD_DIR%\BrickSuite.exe" "%STAGE_DIR%\BrickSuite.exe" >nul

if errorlevel 1 (
    echo ERROR: Unable to copy BrickSuite.exe.
    exit /b 1
)

rem ----------------------------------------------------------------
rem Run Qt deployment
rem ----------------------------------------------------------------
echo.
echo Running windeployqt...
echo.

"%WINDEPLOYQT%" ^
    --release ^
    --compiler-runtime ^
    --no-translations ^
    --dir "%STAGE_DIR%" ^
    "%STAGE_DIR%\BrickSuite.exe"

if errorlevel 1 (
    echo.
    echo ERROR: windeployqt failed.
    exit /b 1
)

rem ----------------------------------------------------------------
rem Validate critical runtime files
rem ----------------------------------------------------------------
echo.
echo Validating staged deployment...

set "VALIDATION_FAILED=0"

call :checkfile "Qt6Core.dll"
call :checkfile "Qt6Gui.dll"
call :checkfile "Qt6Widgets.dll"
call :checkfile "Qt6Sql.dll"
call :checkfile "Qt6Network.dll"
call :checkfile "platforms\qwindows.dll"
call :checkfile "sqldrivers\qsqlite.dll"

if "%VALIDATION_FAILED%"=="1" (
    echo.
    echo ERROR: Deployment validation failed.
    exit /b 1
)

rem ----------------------------------------------------------------
rem Compile Inno Setup installer
rem ----------------------------------------------------------------
echo.
echo Compiling BrickSuite installer...
echo.

set "ISCC=C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
set "ISS_SOURCE=%~dp0BrickSuite.iss"
set "GENERATED_ISS_DIR=%BUILD_DIR%\deployment\windows"
set "GENERATED_ISS=%GENERATED_ISS_DIR%\BrickSuite.iss"
set "VERSION_INCLUDE=%GENERATED_ISS_DIR%\BrickSuiteVersion.iss"
set "INSTALLER_OUTPUT=%DEPLOY_ROOT%\installer"

if not exist "%ISCC%" (
    echo ERROR: Inno Setup Compiler was not found:
    echo   %ISCC%
    exit /b 1
)

if not exist "%ISS_SOURCE%" (
    echo ERROR: Inno Setup script was not found:
    echo   %ISS_SOURCE%
    exit /b 1
)

if not exist "%VERSION_INCLUDE%" (
    echo ERROR: Generated installer metadata was not found:
    echo   %VERSION_INCLUDE%
    echo.
    echo Run CMake configure before packaging.
    exit /b 1
)

rem Compile from the generated deployment directory so BrickSuite.iss
rem and BrickSuiteVersion.iss are beside each other. This keeps the
rem generated version include out of the source tree.
copy /Y "%ISS_SOURCE%" "%GENERATED_ISS%" >nul

if errorlevel 1 (
    echo ERROR: Unable to prepare generated Inno Setup script.
    exit /b 1
)

if exist "%INSTALLER_OUTPUT%" (
    rmdir /S /Q "%INSTALLER_OUTPUT%"
)

mkdir "%INSTALLER_OUTPUT%"

if errorlevel 1 (
    echo ERROR: Unable to create installer output directory.
    exit /b 1
)

"%ISCC%" ^
    /DStageDir="%STAGE_DIR%" ^
    /DOutputDir="%INSTALLER_OUTPUT%" ^
    "%GENERATED_ISS%"

if errorlevel 1 (
    echo.
    echo ERROR: Inno Setup compilation failed.
    exit /b 1
)

echo.
echo ============================================================
echo BrickSuite Windows packaging completed successfully.
echo ============================================================
echo.
echo Staged application:
echo   %STAGE_DIR%\BrickSuite.exe
echo.
echo Installer output:
echo   %INSTALLER_OUTPUT%
echo.
exit /b 0


rem ================================================================
rem Validate one required deployment file
rem ================================================================
:checkfile
if not exist "%STAGE_DIR%\%~1" (
    echo   MISSING: %~1
    set "VALIDATION_FAILED=1"
) else (
    echo   OK:      %~1
)
exit /b 0
