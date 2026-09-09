@echo off
setlocal EnableExtensions

rem ================================================================
rem BrickSuite Windows Packaging
rem
rem Usage:
rem   package_windows.bat "C:\path\to\Release-build-directory"
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
rem Determine source repository and license file
rem ----------------------------------------------------------------
for %%I in ("%~dp0..\..") do set "SOURCE_ROOT=%%~fI"
set "LICENSE_SOURCE=%SOURCE_ROOT%\LICENSE"
set "ICON_SOURCE=%SOURCE_ROOT%\resources\icons\bricksuite.ico"

echo Source repository:
echo   %SOURCE_ROOT%
echo.

if not exist "%LICENSE_SOURCE%" (
    echo ERROR: BrickSuite license file was not found:
    echo   %LICENSE_SOURCE%
    exit /b 1
)

if not exist "%ICON_SOURCE%" (
    echo ERROR: BrickSuite installer icon was not found:
    echo   %ICON_SOURCE%
    exit /b 1
)

rem ----------------------------------------------------------------
rem Validate Release build
rem ----------------------------------------------------------------
if not exist "%BUILD_DIR%\BrickSuite.exe" (
    echo ERROR: BrickSuite.exe was not found:
    echo   %BUILD_DIR%\BrickSuite.exe
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

for %%I in ("%QT6_DIR%\..\..\..") do set "QT_ROOT=%%~fI"

set "WINDEPLOYQT=%QT_ROOT%\bin\windeployqt.exe"

set "OPENSSL_ROOT_DIR="
for /f "tokens=1,* delims==" %%A in ('findstr /B /C:"OPENSSL_ROOT_DIR:" "%BUILD_DIR%\CMakeCache.txt"') do (
    set "OPENSSL_ROOT_DIR=%%B"
)

echo Qt installation:
echo   %QT_ROOT%
echo.
echo windeployqt:
echo   %WINDEPLOYQT%
echo.

if not exist "%WINDEPLOYQT%" (
    echo ERROR: windeployqt.exe was not found:
    echo   %WINDEPLOYQT%
    exit /b 1
)

if not defined OPENSSL_ROOT_DIR (
    echo ERROR: OPENSSL_ROOT_DIR could not be found in CMakeCache.txt.
    exit /b 1
)

if not exist "%OPENSSL_ROOT_DIR%\bin\libcrypto-3-x64.dll" (
    echo ERROR: OpenSSL 3 Crypto runtime was not found under OPENSSL_ROOT_DIR.
    exit /b 1
)
if not exist "%OPENSSL_ROOT_DIR%\bin\libssl-3-x64.dll" (
    echo ERROR: OpenSSL 3 TLS runtime was not found under OPENSSL_ROOT_DIR.
    exit /b 1
)

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
rem Copy BrickSuite executable and license
rem ----------------------------------------------------------------
echo Copying BrickSuite.exe...
copy /Y "%BUILD_DIR%\BrickSuite.exe" "%STAGE_DIR%\BrickSuite.exe" >nul
if errorlevel 1 (
    echo ERROR: Unable to copy BrickSuite.exe.
    exit /b 1
)

echo Copying LICENSE...
copy /Y "%LICENSE_SOURCE%" "%STAGE_DIR%\LICENSE" >nul
if errorlevel 1 (
    echo ERROR: Unable to copy LICENSE.
    exit /b 1
)

echo Copying BrickSuite icon...
copy /Y "%ICON_SOURCE%" "%STAGE_DIR%\bricksuite.ico" >nul
if errorlevel 1 (
    echo ERROR: Unable to copy BrickSuite icon.
    exit /b 1
)

rem ----------------------------------------------------------------
rem Run Qt deployment
rem ----------------------------------------------------------------
echo.
echo Running windeployqt...
echo.

"%WINDEPLOYQT%" --release --compiler-runtime --no-translations --openssl-root "%OPENSSL_ROOT_DIR%" --dir "%STAGE_DIR%" "%STAGE_DIR%\BrickSuite.exe"

if errorlevel 1 (
    echo.
    echo ERROR: windeployqt failed.
    exit /b 1
)

echo Copying OpenSSL 3 runtime libraries...
copy /Y "%OPENSSL_ROOT_DIR%\bin\libcrypto-3-x64.dll" "%STAGE_DIR%\libcrypto-3-x64.dll" >nul
copy /Y "%OPENSSL_ROOT_DIR%\bin\libssl-3-x64.dll" "%STAGE_DIR%\libssl-3-x64.dll" >nul
if errorlevel 1 (
    echo ERROR: Unable to stage the OpenSSL 3 runtime libraries.
    exit /b 1
)
if not exist "%OPENSSL_ROOT_DIR%\share\licenses\openssl\LICENSE" (
    echo ERROR: OpenSSL license attribution file was not found under OPENSSL_ROOT_DIR.
    exit /b 1
)
mkdir "%STAGE_DIR%\licenses\OpenSSL" 2>nul
copy /Y "%OPENSSL_ROOT_DIR%\share\licenses\openssl\LICENSE" "%STAGE_DIR%\licenses\OpenSSL\LICENSE" >nul
if errorlevel 1 (
    echo ERROR: Unable to stage the OpenSSL license attribution.
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
call :checkfile "Qt6WebSockets.dll"
call :checkfile "libcrypto-3-x64.dll"
call :checkfile "libssl-3-x64.dll"
call :checkfile "licenses\OpenSSL\LICENSE"
call :checkfile "platforms\qwindows.dll"
call :checkfile "sqldrivers\qsqlite.dll"
call :checkfile "tls\qopensslbackend.dll"
call :checkfile "LICENSE"
call :checkfile "bricksuite.ico"

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

set "ISCC=C:\PROGRA~2\Inno Setup 6\ISCC.exe"
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

"%ISCC%" /DStageDir="%STAGE_DIR%" /DOutputDir="%INSTALLER_OUTPUT%" "%GENERATED_ISS%"

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
