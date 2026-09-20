@echo off
rem Unified build script for BrowserPageWidget (Qt 5.15.2 / Qt 6.x dual support).
rem
rem Usage:
rem   build.bat [5|6] [Debug|Release] [build-dir]
rem
rem Examples:
rem   build.bat 6 Debug     -> Qt 6.11.2 msvc2022_64 + VS2022 (default)
rem   build.bat 5 Debug     -> Qt 5.15.2 msvc2019_64 + VS2022
rem   build.bat 5 Release
rem
rem Different Qt majors always use separate build dirs
rem (build\ for Qt 6, build-qt5\ for Qt 5) to avoid CMakeCache conflicts.
setlocal

set "QT_MAJOR=%~1"
if "%QT_MAJOR%"=="" set "QT_MAJOR=6"
set "CONFIG=%~2"
if "%CONFIG%"=="" set "CONFIG=Debug"
set "BUILD_DIR=%~3"

if "%QT_MAJOR%"=="5" (
    set "QT_ROOT=C:\Qt\5.15.2\5.15.2\msvc2019_64"
    if "%BUILD_DIR%"=="" set "BUILD_DIR=%~dp0build-qt5"
) else (
    set "QT_MAJOR=6"
    set "QT_ROOT=C:\Qt\6.11.2\6.11.2\msvc2022_64"
    if "%BUILD_DIR%"=="" set "BUILD_DIR=%~dp0build"
)

call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 (
    echo Failed to set up MSVC environment.
    exit /b 1
)

echo [BrowserPageWidget] Qt%QT_MAJOR% %CONFIG% ^| build dir: %BUILD_DIR%

cmake -S "%~dp0." -B "%BUILD_DIR%" -G "NMake Makefiles" ^
    -DCMAKE_BUILD_TYPE=%CONFIG% ^
    -DBPW_QT_MAJOR=%QT_MAJOR% ^
    -DBROWSER_PAGE_WIDGET_QT_ROOT="%QT_ROOT%"
if errorlevel 1 exit /b 1

cmake --build "%BUILD_DIR%"
if errorlevel 1 exit /b 1

endlocal
